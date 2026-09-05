/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file handles VM-Exits in AMD-V of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{hint::{cold_path, spin_loop}, mem::MaybeUninit, slice, sync::atomic::Ordering};

use paste::paste;

use decode::{dispatch_decoder, SoftwareDecodeAssistOps};
use custom::SvmCustomVcpu;
use cvexit::dispatch_cvexit_handler;
use npt::NptFaultCode;
use hvcall::dispatch_hypercall;
use xpf_core::{asm::cpuid::cpuid2, ci::is_ci_phys_page, x86::{crdr::*, descriptors::SegmentFlags, rflags::Rflags}, trytask::try_task};
use disasm::emulator::{EmulatorOps, Instruction};

use crate::svm_core::decode::dispatch_cvexit_decoder;

use super::*;
use mshv_core::{cpuid::*,msr::dispatch_mshv_msr_handler};

// Place all VM-Exit handlers from the subverted host into this implementation!
// Rules of thumb in implementing VM-Exit Handlers: Do not allocate memories from heap!
impl SvmVcpu
{
	fn handle_unknown(&mut self,_context:&mut SvmStackTop)
	{
		let intercept_code:i64=unsafe{self.vmread(EXIT_CODE)};
		panic!("Unknown VM-Exit is intercepted! Code: 0x{:016X}",intercept_code);
	}

	fn handle_cr4_read(&mut self,_context:&mut SvmStackTop)
	{
		let gpr_index=(self.read_exit_info1()&0xF) as usize;
		// Virtualize CR4.MCE bit.
		self.set_gpr(gpr_index,self.read_cr4().with_mce(self.flags.mce()).into_bits());
		self.advance_rip();
	}

	fn handle_cr4_write(&mut self,_context:&mut SvmStackTop)
	{
		let gpr_index=(self.read_exit_info1()&0xF) as usize;
		// Virtualize CR4.MCE bit.
		let mut new_value=Cr4::from_bits(self.get_gpr(gpr_index));
		self.flags.set_mce(new_value.mce());
		// Flush TLB if certain bits are cleared.
		if new_value.require_flush_tlb(self.read_cr4())
		{
			self.write_tlb_control(TLB_CONTROL_FLUSH_GUEST_TLB);
		}
		// Force MCE bit in guest mode.
		new_value.set_mce(true);
		self.write_cr4(new_value);
		self.advance_rip();
	}

	fn handle_sx(&mut self,context:&mut SvmStackTop)
	{
		// #SX exception happened! This means INIT signal is converted to #SX.
		let err_code:u32=unsafe{self.vmread(EXIT_INFO1)};
		assert_eq!(err_code,1,"#SX error code is not 1! (actual: 0x{err_code:X})");
		self.wait_for_sipi.store(true,Ordering::SeqCst);
		// General-Purpose Registers
		for i in 0..16usize
		{
			context.gpr_state.write(i,0);
		}
		self.write_rsp(0);
		self.write_rip(0xFFF0);
		self.write_rflags(Rflags::from_bits(2));
		// Control Registers.
		let old_cr0=self.read_cr0();
		let cr0=Cr0::new().with_et(true).with_cd(old_cr0.cd()).with_nw(old_cr0.nw());
		self.write_cr0(cr0);
		self.write_cr2(0);
		self.write_cr3(0);
		self.write_cr4(Cr4::new());
		// INIT clears EFER to 0, but SVME bit must be set as a guest.
		self.write_efer(Efer::new().with_svme(true));
		// Segment Registers
		let mut attrib=SvmSegmentFlags::new().with_segment_type(SegmentFlags::CODE_EXECUTE_READ_ACCESSED).with_user_segment(true).with_present(true);
		self.write_cs(SvmSegmentRegister{selector:0xF000,attrib,limit:0xFFFF,base:0xFFFF0000});
		attrib.set_segment_type(SegmentFlags::DATA_READ_WRITE_ACCESSED);
		let data_seg=SvmSegmentRegister{selector:0,attrib,limit:0xFFFF,base:0};
		self.write_ds(data_seg);
		self.write_es(data_seg);
		self.write_fs(data_seg);
		self.write_gs(data_seg);
		self.write_ss(data_seg);
		{
			let idtr=self.ref_idtr_mut();
			idtr.limit=0xFFFF;
			idtr.base=0;
		}
		{
			let gdtr=self.ref_gdtr_mut();
			gdtr.limit=0xFFFF;
			gdtr.base=0;
		}
		attrib.set_user_segment(false);
		attrib.set_segment_type(SegmentFlags::LDT);
		self.write_ldtr(SvmSegmentRegister{selector:0,attrib,limit:0xFFFF,base:0});
		attrib.set_segment_type(SegmentFlags::BUSY_TSS_16BIT);
		self.write_tr(SvmSegmentRegister{selector:0,attrib,limit:0xFFFF,base:0});
		// Debug Registers
		self.write_dr6(Dr6::from_bits(0xFFFF0FF0));
		self.write_dr7(Dr7::from_bits(0x400));
		// Emulate the wait-for-SIPI via spin-locking.
		info!("INIT signal is successfully emulated! Waiting for SIPI now...");
		while self.wait_for_sipi.load(Ordering::SeqCst)
		{
			spin_loop();
		}
		// Startup-IPI is received. Resume.
		{
			let vector=self.sipi_vector.load(Ordering::SeqCst);
			info!("Received SIPI with vector 0x{vector:02X}!");
			let cs=self.ref_cs_mut();
			cs.selector=(vector as u16)<<8;
			cs.base=(vector as u64)<<12;
		}
		self.write_rip(0);
		// Because CR0/CR4 are changed, flush the TLBs.
		self.write_tlb_control(TLB_CONTROL_FLUSH_GUEST_TLB);
	}

	fn handle_cpuid(&mut self,context:&mut SvmStackTop)
	{
		let hv:&SvmHypervisor=unsafe{&*self.hypervisor.cast()};
		let gpr_state:&mut GprState=&mut context.gpr_state;
		let ia=gpr_state.rax as u32;
		let ic=gpr_state.rcx as u32;
		let (a,b,c,d)=
		if (ia&0x40000000)==0x40000000
		{
			if self.under_hvm
			{
				// We're under a hypervisor which exposes its hypervisor interfaces.
				// Current implementation would directly pass-thru their CPUID.
				cpuid2(ia,ic)
			}
			else if hv.features.cpuid_hv_presence()
			{
				// This is Hypervisor's CPUID.
				let leaf_func=(ia&0x3FFFFFFF) as usize;
				match MSHV_CPUID_HANDLERS.get(leaf_func)
				{
					Some(f)=>f(ia,ic),
					None=>(0,0,0,0)
				}
			}
			else
			{
				(0,0,0,0)
			}
		}
		else
		{
			// This is processor's CPUID.
			// Execute the original CPUID and filter stuff.
			let (mut a,mut b,mut c,mut d)=cpuid2(ia,ic);
			match ia
			{
				CPUID_STD_PROCESSOR_FEATURE=>
				{
					c|=if hv.features.cpuid_hv_presence() {CPUID_UNDER_HYPERVISOR} else {0};
					if !self.ref_cr4().osxsave()
					{
						c&=!CPUID_OSXSAVE;
					}
				}
				// NoirVisor currently does not support nested virtualization.
				CPUID_EXT_PROCESSOR_FEATURE=>c&=!CPUID_SVM,
				CPUID_EXT_SECURE_VIRTUAL_MACHINE_FEATURE=>(a,b,c,d)=(0,0,0,0),
				_=>()
			};
			(a,b,c,d)
		};
		// Write the results back to eax, ebx, ecx and edx and clear the higher 32 bits.
		gpr_state.rax=a as u64;
		gpr_state.rbx=b as u64;
		gpr_state.rcx=c as u64;
		gpr_state.rdx=d as u64;
		// Advance the rip.
		self.advance_rip();
	}

	/// # `handle_rdmsr`
	/// Handles an incoming `rdmsr` on this vCPU.
	/// Return `Some(u64)` if this `rdmsr` request should advance rip.
	/// Return `None` if this `rdmsr` request failed. Exception was injected.
	fn handle_rdmsr(&mut self,index:u32)->Option<u64>
	{
		#[repr(C,align(8))] struct MsrContext
		{
			index:u32,
			value:MaybeUninit<u64>
		}
		extern "C" fn try_rdmsr(ctxt:*mut c_void)
		{
			let ctxt:&mut MsrContext=unsafe{&mut *ctxt.cast()};
			ctxt.value.write(rdmsr(ctxt.index));
		}
		if self.under_hvm && (0x40000000..0x80000000).contains(&index)
		{
			// If NoirVisor is running under a hypervisor (e.g.: Hyper-V), we may pass-thru this MSR to upper hypervisor.
			let mut x=MsrContext{index,value:MaybeUninit::uninit()};
			match unsafe{try_task(try_rdmsr,(&raw mut x).cast())}
			{
				Ok(_)=>return Some(unsafe{x.value.assume_init()}),
				Err(e)=>
				{
					error!("Failed to pass-thru Microsoft TLFS MSR-read (index=0x{index:X}) request! Vector={}, Error-Code: {:X?}",e.vector,e.error_code);
					self.inject_event(e.vector,EventType::HardwareException,e.error_code,true);
					return None;
				}
			}
		}
		match index
		{
			(0x40000000..0x80000000)=>
			{
				let f=dispatch_mshv_msr_handler(index);
				let mut r:u64=0;
				if f(&mut self.mshv_ctxt,false,&mut r)
				{
					Some(r)
				}
				else
				{
					None
				}
			}
			MSR_EFER=>
			{
				// Read the EFER value from VMCB.
				let mut v=self.read_efer();
				// The SVME bit should be filtered.
				v.set_svme(self.flags.svme());
				Some(v.into_bits())
			}
			MSR_TSC_RATIO=>
			{
				error!("TSC Ratio rdmsr handler is not implemented!");
				self.inject_event(GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true);
				None
			}
			MSR_VMCR=>
			{
				Some(self.nested_hvm.vmcr)
			}
			MSR_IGNNE=>
			{
				Some(self.nested_hvm.ignne)
			}
			MSR_SMM_CTRL=>
			{
				error!("SMM_CTRL rdmsr handler is not implemented!");
				self.inject_event(GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true);
				None
			}
			MSR_HSAVE_PA=>
			{
				Some(self.nested_hvm.hsave_pa)
			}
			_=>
			{
				let mut x:MsrContext=MsrContext{index,value:MaybeUninit::uninit()};
				warn!("Unexpected rdmsr is intercepted! Index=0x{index:X}");
				match unsafe{try_task(try_rdmsr,(&raw mut x).cast())}
				{
					Ok(_)=>
					{
						let v=unsafe{x.value.assume_init()};
						warn!("Value is 0x{v:X}");
						Some(v)
					}
					Err(e)=>
					{
						error!("The rdmsr task failed! Vector={}, Error-Code: {:X?}",e.vector,e.error_code);
						self.inject_event(e.vector,EventType::HardwareException,e.error_code,true);
						None
					}
				}
			}
		}
	}

	/// # `handle_wrmsr`
	/// Handles an incoming `wrmsr` on this vCPU.
	/// Return `true` if this `wrmsr` request should advance rip.
	/// Return `false` if this `wrmsr` request failed. Exception was injected.
	fn handle_wrmsr(&mut self,index:u32,value:u64)->bool
	{
		#[repr(C)] struct MsrContext
		{
			index:u32,
			value:u64
		}
		extern "C" fn try_wrmsr(ctxt:*mut c_void)
		{
			let ctxt:&mut MsrContext=unsafe{&mut *ctxt.cast()};
			wrmsr(ctxt.index,ctxt.value);
		}
		if self.under_hvm && (0x40000000..0x80000000).contains(&index)
		{
			// If NoirVisor is running under a hypervisor (e.g.: Hyper-V), we may pass-thru this MSR to upper hypervisor.
			let mut x=MsrContext{index,value};
			// It's not guaranteed this MSR is valid.
			match unsafe{try_task(try_wrmsr,(&raw mut x).cast())}
			{
				Ok(_)=>return true,
				Err(e)=>
				{
					error!("Failed to pass-thru Microsoft TLFS MSR-write (index=0x{index:X}) request! Vector={}, Error-Code: {:X?}",e.vector,e.error_code);
					self.inject_event(e.vector,EventType::HardwareException,e.error_code,true);
					return false;
				}
			}
		}
		match index
		{
			(0x40000000..0x80000000)=>
			{
				let mut v=value;
				let f=dispatch_mshv_msr_handler(index);
				f(&mut self.mshv_ctxt,true,&mut v)
			}
			MSR_EFER=>
			{
				let efer=Efer::from_bits(value);
				self.flags.set_svme(efer.svme());
				// SVME bit should always be set.
				self.write_efer(efer.with_svme(true));
				true
			}
			MSR_TSC_RATIO=>
			{
				// TSC Ratio is not supported.
				self.inject_event(GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true);
				false
			}
			MSR_VMCR=>
			{
				todo!("TODO: VMCR Emulation not implemented yet!");
			}
			MSR_IGNNE=>
			{
				// Only the lowest bit can be set to 1.
				if (value&(u64::MAX-1))!=0
				{
					self.inject_event(GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true);
					false
				}
				else
				{
					self.nested_hvm.ignne=value;
					true
				}
			}
			MSR_SMM_CTRL=>
			{
				error!("SMM_CTRL wrmsr handler is not implemented!");
				self.inject_event(GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true);
				false
			}
			MSR_HSAVE_PA=>
			{
				// HSAVE must be aligned on page-boundary.
				if page_4kb_offset(value)!=0
				{
					self.inject_event(GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true);
					false
				}
				else
				{
					self.nested_hvm.hsave_pa=value;
					true
				}
			}
			MSR_SVM_KEY=>
			{
				todo!("SVM-Key Emulation is not implemented!");
			}
			_=>
			{
				let mut x:MsrContext=MsrContext{index,value};
				warn!("Unexpected wrmsr is intercepted! Index=0x{index:X}");
				match unsafe{try_task(try_wrmsr,(&raw mut x).cast())}
				{
					Ok(_)=>true,
					Err(e)=>
					{
						error!("The wrmsr task failed! Vector={}, Error-Code: {:X?}",e.vector,e.error_code);
						self.inject_event(e.vector,EventType::HardwareException,e.error_code,true);
						false
					}
				}
			}
		}
	}

	fn handle_msr(&mut self,context:&mut SvmStackTop)
	{
		let index=context.gpr_state.rcx as u32;
		let op_write:bool=unsafe{self.vmread(EXIT_INFO1)};
		if op_write
		{
			let value=(context.gpr_state.rax&u32::MAX as u64)|(context.gpr_state.rdx<<32);
			if self.handle_wrmsr(index,value)
			{
				// Advance rip.
				self.advance_rip();
			}
		}
		else if let Some(value)=self.handle_rdmsr(index)
		{
			// Write back to registers and advance rip.
			let lo=value as u32;
			let hi=(value>>32) as u32;
			unsafe
			{
				(&raw mut context.gpr_state.rax).cast::<u32>().write(lo);
				(&raw mut context.gpr_state.rdx).cast::<u32>().write(hi);
				self.advance_rip();
			}
		}
	}

	fn handle_shutdown(&mut self,_context:&mut SvmStackTop)
	{
		let gdt_base:u64=unsafe{self.vmread(GUEST_GDTR_BASE)};
		let idt_base:u64=unsafe{self.vmread(GUEST_IDTR_BASE)};
		info!("GDT-Base: 0x{gdt_base:X}, IDT-Base: 0x{idt_base:X}");
		panic!("Shutdown occured!");
	}

	fn handle_vmrun(&mut self,context:&mut SvmStackTop)
	{
		error!("Nested virtualization is unsupported! Nested VMCB RAX=0x{:016X}",context.gpr_state.rax);
		self.inject_event(INVALID_OPCODE_FAULT,EventType::HardwareException,None,true);
	}

	fn handle_vmmcall(&mut self,context:&mut SvmStackTop)
	{
		let cpl=self.read_cpl();
		if cpl!=0
		{
			error!("NoirVisor forbids user-mode hypercalls!");
			self.inject_event(INVALID_OPCODE_FAULT,EventType::HardwareException,None,true);
			return;
		}
		let gpr_state=&mut context.gpr_state;
		let vmmcall_func=gpr_state.rcx as u32;
		let handler_fn=dispatch_hypercall(vmmcall_func);
		match handler_fn(self,gpr_state.rdx as *mut c_void,gpr_state.r8 as usize)
		{
			Ok(st)=>
			{
				// This hypercall is known. Put status in rax register.
				gpr_state.rax=st.0 as u64;
				self.advance_rip();
			}
			Err((vector,error_code))=>
			{
				// Exception happened while servicing the hypercall. Inject into the Guest.
				self.inject_event(vector,EventType::HardwareException,error_code,true);
			}
		}
	}

	fn handle_vmload(&mut self,context:&mut SvmStackTop)
	{
		error!("Nested virtualization is unsupported! Nested VMCB RAX=0x{:016X}",context.gpr_state.rax);
		self.inject_event(INVALID_OPCODE_FAULT,EventType::HardwareException,None,true);
	}

	fn handle_vmsave(&mut self,context:&mut SvmStackTop)
	{
		error!("Nested virtualization is unsupported! Nested VMCB RAX=0x{:016X}",context.gpr_state.rax);
		self.inject_event(INVALID_OPCODE_FAULT,EventType::HardwareException,None,true);
	}

	fn handle_stgi(&mut self,_context:&mut SvmStackTop)
	{
		error!("Nested virtualization is unsupported!");
		self.inject_event(INVALID_OPCODE_FAULT,EventType::HardwareException,None,true);
	}

	fn handle_clgi(&mut self,_context:&mut SvmStackTop)
	{
		error!("Nested virtualization is unsupported!");
		self.inject_event(INVALID_OPCODE_FAULT,EventType::HardwareException,None,true);
	}

	fn handle_skinit(&mut self,_context:&mut SvmStackTop)
	{
		error!("Nested virtualization is unsupported!");
		self.inject_event(INVALID_OPCODE_FAULT,EventType::HardwareException,None,true);
	}

	fn handle_npf(&mut self,_context:&mut SvmStackTop)
	{
		let fault:NptFaultCode=unsafe{self.vmread(EXIT_INFO1)};
		let gpa:u64=unsafe{self.vmread(EXIT_INFO2)};
		let rip:u64=self.read_rip();
		// Check if this #NPF is due to Code Integrity violation.
		if is_ci_phys_page(gpa)
		{
			// Decode the instruction length.
			let ins_bytes=self.instruction_bytes();
			let mut ins=Instruction::default();
			ins.copy_from_slice(ins_bytes);
			ins.decode(self.get_current_bitness());
			error!("CI-fault for GPA=0x{gpa:016X} is intercepted! rip=0x{rip:016X}, Fault-Reason: {fault}, Instruction-Length: {}",ins.len());
			debug!("CI-fault Instruction: {:02X?} | {ins}",&ins_bytes[..ins.len()]);
			self.advance_rip_manually(ins.len());
		}
		else if !fault.code_fetch()
		{
			// This could be MMIO Filter.
			let ins_bytes:&[u8]=unsafe{slice::from_raw_parts(self.vmcb.virt.byte_add(GUEST_INSTRUCTION_BYTES).cast(),15)};
			// Call disassembler.
			let mut ins=Instruction::default();
			ins.copy_from_slice(ins_bytes);
			ins.decode(self.get_current_bitness());
			// trace!("Intercepted filtered MMIO! Instruction: {ins}");
			if fault.write()
			{
				self.emulate_mmio_output(&ins,gpa);
			}
			else
			{
				self.emulate_mmio_input(&ins,gpa);
			}
			self.advance_rip_manually(ins.len());
		}
		else
		{
			panic!("Unexpected #NPF is intercepted!\nrip=0x{rip:016X},GPA=0x{gpa:X}\n");
		}
	}

	fn handle_invalid(&mut self,_context:&mut SvmStackTop)
	{
		let intercept_code:i64=unsafe{self.vmread(EXIT_CODE)};
		panic!("Invalid State! Exit Code: 0x{:X}",intercept_code);
	}
}

/// # Safety
/// This function is unsafe is because it's called from assembly.
/// DO NOT CALL THIS FUNCTION FROM RUST CODE!
#[unsafe(no_mangle)] unsafe extern "win64" fn nvc_svm_exit_handler(stack:*mut SvmStackTop)
{
	let vcpu=unsafe{&mut *(*stack).vcpu};
	let gpr=unsafe{&mut (*stack).gpr_state};
	if unsafe{(*stack).guest_vmcb_pa}==vcpu.vmcb.phys
	{
		// Allow debugger to display the stack trace from the guest.
		// Note: this stack trace is only meaningful from subverted host.
		unsafe
		{
			(*stack).guest_frame.return_rip=vcpu.read_rip();
			(*stack).guest_frame.return_rsp=vcpu.read_rsp();
		}
		// Intercept code is supposed to be 64-bit, but Linux KVM has a bug that treats the intercept code as 32-bit.
		let intercept_code:i32=vcpu.read_exit_code() as i32;
		let decoder=dispatch_decoder(intercept_code as i64);
		let handler=dispatch_handler(intercept_code as i64);
		// If VMCB-Clean-Bits is supported, we may cache the VMCB fields.
		if vcpu.svm_feats.vmcb_clean()
		{
			vcpu.write_clean_field(VmcbCleanField::ALL_CACHED);
		}
		// Handle the VM-Exit!
		gpr.rax=vcpu.read_rax();
		gpr.rsp=vcpu.read_rsp();
		vcpu.decoded_instruction.clear();
		decoder(vcpu);
		handler(vcpu,unsafe{&mut *stack});
		// The rax in GPR state should be the physical address of VMCB
		// in order to execute the vmrun instruction properly.
		// Reading/Writing the rax is like the vmptrst/vmptrld instruction in Intel VT-x.
	}
	else if unsafe{!(*stack).custom_vcpu.is_null()}
	{
		let cvcpu:&mut SvmCustomVcpu=unsafe{&mut *(*stack).custom_vcpu.cast()};
		// Intercept code is supposed to be 64-bit, but Linux KVM has a bug that treats the intercept code as 32-bit.
		let intercept_code:i32=cvcpu.read_exit_code() as i32;
		let decoder=dispatch_cvexit_decoder(intercept_code as i64);
		let handler=dispatch_cvexit_handler(intercept_code as i64);
		if vcpu.svm_feats.vmcb_clean()
		{
			cvcpu.write_clean_field(VmcbCleanField::ALL_CACHED);
		}
		// Handle the VM-Exit!
		gpr.rax=cvcpu.read_rax();
		gpr.rsp=cvcpu.read_rsp();
		cvcpu.decoded_instruction.clear();
		decoder(cvcpu);
		handler(cvcpu,vcpu);
	}
	else
	{
		cold_path();
		panic!("Current VMCB Physical-Address (0x{:X}) is unexpected!",unsafe{(*stack).guest_vmcb_pa});
	}
	// Restore the rax and rsp. Note that the VMCB might be switched.
	if unsafe{(*stack).guest_vmcb_pa}==vcpu.vmcb.phys
	{
		if let Some((vm_handle,vcpu_id))=vcpu.cv_host_save.from_vcpu
		{
			// We switched back from a vCPU.
			let hv=unsafe{&mut *(vcpu.hypervisor as *mut SvmHypervisor)};
			if let Some(vm)=hv.vm_list.read().get(vm_handle.0 as usize).unwrap() && let Some(Some(vcpu))=vm.read().vcpus.read().get(vcpu_id as usize)
			{
				unsafe
				{
					vcpu.force_unlock();
				}
			}
			vcpu.cv_host_save.from_vcpu=None;
		}
		vcpu.write_rax(gpr.rax);
		vcpu.write_rsp(gpr.rsp);
	}
	else if unsafe{!(*stack).custom_vcpu.is_null()}
	{
		let cvcpu:&mut SvmCustomVcpu=unsafe{&mut *(*stack).custom_vcpu.cast()};
		cvcpu.write_rax(gpr.rax);
		cvcpu.write_rsp(gpr.rsp);
	}
	// Specify the VMCB we will run next.
	gpr.rax=unsafe{(*stack).guest_vmcb_pa};
}

pub const SVM_MAXIMUM_GROUPS:usize=2;
pub const SVM_MAXIMUM_CODE1:usize=0xA7;
pub const SVM_MAXIMUM_CODE2:usize=0x4;
pub const SVM_MAXIMUM_NEGATIVE:usize=4;

// Use a macro to reduce repetitions for defining CR/DR interceptions.
macro_rules! build_crdr_interception
{
	($name:tt,$action:tt,$start:literal) =>
	{
		paste!
		{
			pub const [<INTERCEPTED_ $name:upper 0 _ $action:upper>]:i64=$start+0;
			pub const [<INTERCEPTED_ $name:upper 1 _ $action:upper>]:i64=$start+1;
			pub const [<INTERCEPTED_ $name:upper 2 _ $action:upper>]:i64=$start+2;
			pub const [<INTERCEPTED_ $name:upper 3 _ $action:upper>]:i64=$start+3;
			pub const [<INTERCEPTED_ $name:upper 4 _ $action:upper>]:i64=$start+4;
			pub const [<INTERCEPTED_ $name:upper 5 _ $action:upper>]:i64=$start+5;
			pub const [<INTERCEPTED_ $name:upper 6 _ $action:upper>]:i64=$start+6;
			pub const [<INTERCEPTED_ $name:upper 7 _ $action:upper>]:i64=$start+7;
			pub const [<INTERCEPTED_ $name:upper 8 _ $action:upper>]:i64=$start+8;
			pub const [<INTERCEPTED_ $name:upper 9 _ $action:upper>]:i64=$start+9;
			pub const [<INTERCEPTED_ $name:upper 10 _ $action:upper>]:i64=$start+10;
			pub const [<INTERCEPTED_ $name:upper 11 _ $action:upper>]:i64=$start+11;
			pub const [<INTERCEPTED_ $name:upper 12 _ $action:upper>]:i64=$start+12;
			pub const [<INTERCEPTED_ $name:upper 13 _ $action:upper>]:i64=$start+13;
			pub const [<INTERCEPTED_ $name:upper 14 _ $action:upper>]:i64=$start+14;
			pub const [<INTERCEPTED_ $name:upper 15 _ $action:upper>]:i64=$start+15;
		}
	};
}

// This macro is probably not as beautiful as the one for CR/DR interceptions.
macro_rules! build_exception_interception
{
	($name:tt,$index:literal) =>
	{
		paste!
		{
			pub const [<INTERCEPTED_ $name:upper _EXCEPTION>]:i64=0x40+$index;
		}
	};
}

build_crdr_interception!(CR,READ,0x0);
build_crdr_interception!(CR,WRITE,0x10);
build_crdr_interception!(DR,READ,0x20);
build_crdr_interception!(DR,WRITE,0x30);
build_crdr_interception!(CR,WRITE_TRAP,0x90);

build_exception_interception!(DE,0);
build_exception_interception!(DB,1);
build_exception_interception!(BP,3);
build_exception_interception!(OF,4);
build_exception_interception!(BR,5);
build_exception_interception!(UD,6);
build_exception_interception!(NM,7);
build_exception_interception!(DF,8);
build_exception_interception!(TS,10);
build_exception_interception!(NP,11);
build_exception_interception!(SS,12);
build_exception_interception!(GP,13);
build_exception_interception!(PF,14);
build_exception_interception!(MF,16);
build_exception_interception!(AC,17);
build_exception_interception!(MC,18);
build_exception_interception!(XF,19);
build_exception_interception!(CP,21);
build_exception_interception!(HV,28);
build_exception_interception!(VC,29);
build_exception_interception!(SX,30);

pub const INTERCEPTED_CR_ACCESS:i64=0x00;
pub const INTERCEPTED_DR_ACCESS:i64=0x20;
pub const INTERCEPTED_EXCEPTIONS:i64=0x40;
pub const INTERCEPTED_INTERRUPT:i64=0x60;
pub const INTERCEPTED_NMI:i64=0x61;
pub const INTERCEPTED_SMI:i64=0x62;
pub const INTERCEPTED_INIT:i64=0x63;
pub const INTERCEPTED_VINTR:i64=0x64;
pub const INTERCEPTED_CR0_NOTTSMP:i64=0x65;
pub const INTERCEPTED_SIDT:i64=0x66;
pub const INTERCEPTED_SGDT:i64=0x67;
pub const INTERCEPTED_SLDT:i64=0x68;
pub const INTERCEPTED_STR:i64=0x69;
pub const INTERCEPTED_LIDT:i64=0x6A;
pub const INTERCEPTED_LGDT:i64=0x6B;
pub const INTERCEPTED_LLDT:i64=0x6C;
pub const INTERCEPTED_LTR:i64=0x6D;
pub const INTERCEPTED_RDTSC:i64=0x6E;
pub const INTERCEPTED_RDPMC:i64=0x6F;
pub const INTERCEPTED_PUSHF:i64=0x70;
pub const INTERCEPTED_POPF:i64=0x71;
pub const INTERCEPTED_CPUID:i64=0x72;
pub const INTERCEPTED_RSM:i64=0x73;
pub const INTERCEPTED_IRET:i64=0x74;
pub const INTERCEPTED_INT:i64=0x75;
pub const INTERCEPTED_INVD:i64=0x76;
pub const INTERCEPTED_PAUSE:i64=0x77;
pub const INTERCEPTED_HLT:i64=0x78;
pub const INTERCEPTED_INVLPG:i64=0x79;
pub const INTERCEPTED_INVLPGA:i64=0x7A;
pub const INTERCEPTED_IO:i64=0x7B;
pub const INTERCEPTED_MSR:i64=0x7C;
pub const INTERCEPTED_TASK_SWITCH:i64=0x7D;
pub const INTERCEPTED_FERR_FREEZE:i64=0x7E;
pub const INTERCEPTED_SHUTDOWN:i64=0x7F;
pub const INTERCEPTED_VMRUN:i64=0x80;
pub const INTERCEPTED_VMMCALL:i64=0x81;
pub const INTERCEPTED_VMLOAD:i64=0x82;
pub const INTERCEPTED_VMSAVE:i64=0x83;
pub const INTERCEPTED_STGI:i64=0x84;
pub const INTERCEPTED_CLGI:i64=0x85;
pub const INTERCEPTED_SKINIT:i64=0x86;
pub const INTERCEPTED_RDTSCP:i64=0x87;
pub const INTERCEPTED_ICEBP:i64=0x88;
pub const INTERCEPTED_WBINVD:i64=0x89;
pub const INTERCEPTED_MONITOR:i64=0x8A;
pub const INTERCEPTED_MWAIT:i64=0x8B;
pub const INTERCEPTED_MWAIT_COND:i64=0x8C;
pub const INTERCEPTED_XSETBV:i64=0x8D;
pub const INTERCEPTED_RDPRU:i64=0x8E;
pub const INTERCEPTED_EFER_W_TRAP:i64=0x8F;
pub const INTERCEPTED_INVLPGB:i64=0xA0;
pub const ILLEGAL_INVLPGB:i64=0xA2;
pub const INTERCEPTED_MCOMMIT:i64=0xA3;
pub const INTERCEPTED_TLBSYNC:i64=0xA4;
pub const INTERCEPTED_BUSLOCK:i64=0xA5;
pub const INTERCEPTED_IDLE_HLT:i64=0xA6;

pub const NESTED_PAGE_FAULT:i64=0x400;
pub const AVIC_INCOMPLETE_IPI:i64=0x401;
pub const AVIC_NO_ACCELERATION:i64=0x402;
pub const INTERCEPTED_VMGEXIT:i64=0x403;

pub const INVALID_GUEST_STATE:i64=-1;
pub const INTERCEPTED_VMSA_BUSY:i64=-2;
pub const IDLE_REQUIRED:i64=-3;
pub const INVALID_PMC:i64=-4;

type SvmExitHandler=fn(&mut SvmVcpu,&mut SvmStackTop);

// Defining sparse array is much easier in Rust than in C!
static SVM_EXIT_HANDLER_GROUP1:[SvmExitHandler;SVM_MAXIMUM_CODE1]=
{
	let mut array:[SvmExitHandler;SVM_MAXIMUM_CODE1]=[SvmVcpu::handle_unknown;SVM_MAXIMUM_CODE1];
	array[INTERCEPTED_CR4_READ as usize]=SvmVcpu::handle_cr4_read;
	array[INTERCEPTED_CR4_WRITE as usize]=SvmVcpu::handle_cr4_write;
	array[INTERCEPTED_SX_EXCEPTION as usize]=SvmVcpu::handle_sx;
	array[INTERCEPTED_CPUID as usize]=SvmVcpu::handle_cpuid;
	array[INTERCEPTED_MSR as usize]=SvmVcpu::handle_msr;
	array[INTERCEPTED_SHUTDOWN as usize]=SvmVcpu::handle_shutdown;
	array[INTERCEPTED_VMRUN as usize]=SvmVcpu::handle_vmrun;
	array[INTERCEPTED_VMMCALL as usize]=SvmVcpu::handle_vmmcall;
	array[INTERCEPTED_VMLOAD as usize]=SvmVcpu::handle_vmload;
	array[INTERCEPTED_VMSAVE as usize]=SvmVcpu::handle_vmsave;
	array[INTERCEPTED_STGI as usize]=SvmVcpu::handle_stgi;
	array[INTERCEPTED_CLGI as usize]=SvmVcpu::handle_clgi;
	array[INTERCEPTED_SKINIT as usize]=SvmVcpu::handle_skinit;
	array
};

static SVM_EXIT_HANDLER_GROUP2:[SvmExitHandler;SVM_MAXIMUM_CODE2]=
{
	let mut array:[SvmExitHandler;SVM_MAXIMUM_CODE2]=[SvmVcpu::handle_unknown;SVM_MAXIMUM_CODE2];
	array[(NESTED_PAGE_FAULT-0x400) as usize]=SvmVcpu::handle_npf;
	array
};

static SVM_EXIT_HANDLER_GROUP_NEGATIVE:[SvmExitHandler;SVM_MAXIMUM_NEGATIVE]=
{
	let mut array:[SvmExitHandler;SVM_MAXIMUM_NEGATIVE]=[SvmVcpu::handle_unknown;SVM_MAXIMUM_NEGATIVE];
	array[!INVALID_GUEST_STATE as usize]=SvmVcpu::handle_invalid;
	array
};

static SVM_EXIT_HANDLER_GROUPS:[&[SvmExitHandler];SVM_MAXIMUM_GROUPS]=[&SVM_EXIT_HANDLER_GROUP1,&SVM_EXIT_HANDLER_GROUP2];

// This function is supposed to be time-sensitive!
// The O(1) method of dispatching handler.
// If we use match-expression, it could be O(n) if optimizer went dumb!
#[inline] fn dispatch_handler(intercept_code:i64)->SvmExitHandler
{
	if intercept_code<0
	{
		cold_path();
		let index:usize=!intercept_code as usize;
		SVM_EXIT_HANDLER_GROUP_NEGATIVE.get(index).copied().unwrap_or(SvmVcpu::handle_unknown)
	}
	else
	{
		let group:usize=(intercept_code as usize)>>10;
		match SVM_EXIT_HANDLER_GROUPS.get(group)
		{
			Some(&g)=>
			{
				let index:usize=(intercept_code as usize)&0x3ff;
				g.get(index).copied().unwrap_or(SvmVcpu::handle_unknown)
			}
			None=>SvmVcpu::handle_unknown
		}
	}
}