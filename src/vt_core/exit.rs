/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file handles VM-Exits in Intel VT-x of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{arch::x86_64::_xsetbv, ffi::c_void};

use iced_x86::{Decoder, Formatter};
use paste::paste;
use log::*;

#[cfg(windows)] use mshv_core::{forwarder::MshvForwardStack, hvcall::TlfsHypercallCode};
#[cfg(windows)] use xpf_core::nvbdk::{nvc_forward_fast_hypercall, nvc_forward_memory_mapped_hypercall};
use crate::{disasm::MnemonicString, mshv_core::cpuid::MSHV_CPUID_HANDLERS, xpf_core::{asm::{cpuid::cpuid2, crdr::*, misc::wbinvd, msr::rdmsr, seg::*, vt::*}, ci::is_ci_phys_page, hv_host::NOIR_HYPERCALL_CODE_CALLEXIT, nvbdk::GprState, trytask::try_task, x86::{cpuid::*, crdr::*, descriptors::{DescriptorTable, SegmentFlags, SystemSegmentDescriptor}, interrupts::{EventType, GENERAL_PROTECTION_FAULT, INVALID_OPCODE_FAULT}}}, *};
use super::{ia32::{cpuid::CPUID_VMX, msr::*}, vmcs::*, VtVcpu, VtStackTop, nvc_vt_resume_without_entry};

impl VtVcpu
{
	fn dump_current_vmcs(&mut self)
	{
		macro_rules! print_segment
		{
			($name:tt) =>
			{
				paste!
				{
					let [<$name:lower>]=read_guest_segment!($name);
					error!("Guest {} Segment: {:X?}",stringify!($name:upper),[<$name:lower>]);
				}
			};
		}
		print_segment!(cs);
		print_segment!(ds);
		print_segment!(es);
		print_segment!(fs);
		print_segment!(gs);
		print_segment!(ss);
		print_segment!(tr);
		print_segment!(ldtr);
		let cr0=unsafe{vmreadptr(GUEST_CR0).unwrap()};
		let cr3=unsafe{vmreadptr(GUEST_CR3).unwrap()};
		let cr4=unsafe{vmreadptr(GUEST_CR4).unwrap()};
		let dr7=unsafe{vmreadptr(GUEST_DR7).unwrap()};
		error!("Guest CR0=0x{cr0:X}, CR3=0x{cr3:X}, CR4=0x{cr4:X}, DR7=0x{dr7:X}");
		let ssp=unsafe{vmreadptr(GUEST_SSP)};
		let rsp=unsafe{vmreadptr(GUEST_RSP).unwrap()};
		let rip=unsafe{vmreadptr(GUEST_RIP).unwrap()};
		let rflags=unsafe{vmreadptr(GUEST_RFLAGS).unwrap()};
		error!("Guest rsp: 0x{rsp:X}, rip: 0x{rip:X}, rflags: 0x{rflags:X}, ssp: 0x{ssp:X?}");
		let efer=unsafe{vmread64(GUEST_MSR_IA32_EFER).unwrap()};
		let pat=unsafe{vmread64(GUEST_MSR_IA32_PAT).unwrap()};
		let dbg_ctrl=unsafe{vmread64(GUEST_MSR_IA32_DEBUG_CTRL).unwrap()};
		error!("Guest EFER: 0x{efer:X}, PAT: 0x{pat:X}, Debug-Control: 0x{dbg_ctrl:X}");
	}

	fn handle_triple_fault(&mut self,_context:&mut VtStackTop)
	{
		panic!("Triple-Fault occured!");
	}

	fn handle_init(&mut self,context:&mut VtStackTop)
	{
		let gpr_state=&mut context.gpr_state;
		unsafe 
		{
			// General-Purpose Registers
			for i in 0..16usize
			{
				gpr_state.write(i,0);
			}
			gpr_state.rdx=self.cpuid_fms as u64;
			vmwriteptr(GUEST_RSP,0);
			vmwriteptr(GUEST_RIP,0xFFF0);
			vmwriteptr(GUEST_RFLAGS,2);
			// Control Registers
			let mut cr0=vmreadptr(GUEST_CR0).unwrap();
			cr0&=(CR0_CD|CR0_NW) as usize;	// CR0.CD and CR0.NW are unchanged during INIT. Other bits except CR0.ET should be cleared.
			cr0|=CR0_ET as usize;			// CR0.ET is always set during INIT.
			cr0|=rdmsr(MSR_VMX_CR0_FIXED1) as usize;
			cr0&=rdmsr(MSR_VMX_CR0_FIXED0) as usize;
			vmwriteptr(GUEST_CR0,cr0);
			write_cr2(0);
			vmwriteptr(GUEST_CR3,0);
			let mut cr4=vmreadptr(GUEST_CR4).unwrap();
			cr4|=rdmsr(MSR_VMX_CR4_FIXED0) as usize;
			cr4&=rdmsr(MSR_VMX_CR4_FIXED1) as usize;
			vmwriteptr(GUEST_CR4,cr4);
			vmwriteptr(GUEST_MSR_IA32_EFER,0);
			// Debug Registers
			write_dr0(0);
			write_dr1(0);
			write_dr2(0);
			write_dr3(0);
			write_dr6(0xffff0ff0);
			vmwriteptr(GUEST_DR7,0x400);
			// IDTR & GDTR
			vmwriteptr(GUEST_GDTR_BASE,0);
			vmwriteptr(GUEST_IDTR_BASE,0);
			vmwrite32(GUEST_GDTR_LIMIT,0xFFFF);
			vmwrite32(GUEST_IDTR_LIMIT,0xFFFF);
			// VM-Entry Controls: Guest is definitely not in IA-32e mode.
			let mut entry_ctrl=VmxEntryControls::from_bits(vmread32(VMENTRY_CONTROLS).unwrap());
			entry_ctrl.set_ia32e_mode_guest(false);
			vmwrite32(VMENTRY_CONTROLS,entry_ctrl.into_bits());
			// Invalid TLB since paging is switched off.
			let ivc=InvvpidContext::Single(vmread16(GUEST_VPID).unwrap());
			invvpid(&ivc);
			// Upon INIT, vCPU enters inactive state to wait for Startup-IPI.
			vmwrite32(GUEST_ACTIVITY_STATE,ActivityState::WAIT_FOR_SIPI);
		}
	}

	fn handle_cpuid(&mut self,context:&mut VtStackTop)
	{
		let gpr_state=&mut context.gpr_state;
		let hv:&VtHypervisor=unsafe{&*self.hypervisor.cast()};
		let ia=gpr_state.rax as u32;
		let ic=gpr_state.rcx as u32;
		let (a,b,c,d)=
		if (ia&0x40000000)==0x40000000
		{
			if hv.features.cpuid_hv_presence()
			{
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
			#[allow(unused_mut)]
			let (mut a,mut b,mut c,mut d)=cpuid2(ia,ic);
			if ia==CPUID_STD_PROCESSOR_FEATURE
			{
				c|=if hv.features.cpuid_hv_presence() {CPUID_UNDER_HYPERVISOR} else {0};
				c&=!CPUID_VMX;
			}
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

	fn handle_getsec(&mut self,_context:&mut VtStackTop)
	{
		error!("SMX Virtualization is not supported!");
		self.advance_rip();
	}

	fn handle_invd(&mut self,_context:&mut VtStackTop)
	{
		trace!("The invd instruction is executed!");
		// In Hyper-V, it invoked wbinvd at invd exit.
		wbinvd();
		self.advance_rip();
	}

	fn handle_vmcall(&mut self,context:&mut VtStackTop)
	{
		let gpr_state=&mut context.gpr_state;
		let vmcall_func=gpr_state.rcx as u32;
		let gcr3=unsafe{vmreadptr(GUEST_CR3)}.unwrap() as u64;
		let grip=unsafe{vmreadptr(GUEST_RIP)}.unwrap();
		let hv:&mut VtHypervisor=unsafe{&mut *self.hypervisor.cast()};
		if hv.is_rip_from_hypervisor(grip)
		{
			debug!("The vmcall instruction is intercepted! Hypercall Leaf: 0x{vmcall_func:X}");
			match vmcall_func
			{
				NOIR_HYPERCALL_CODE_CALLEXIT=>
				{
					let nrip=grip+unsafe{vmread32(VMEXIT_INSTRUCTION_LENGTH).unwrap() as usize};
					let gflags=unsafe{vmreadptr(GUEST_RFLAGS)}.unwrap();
					let saved_state:GprState=GprState
					{
						rax:nrip as u64,
						rcx:gflags as u64,
						rdx:gpr_state.rsp,
						rbx:gpr_state.rbx,
						rsp:gpr_state.rsp,
						rbp:gpr_state.rbp,
						rsi:gpr_state.rsi,
						rdi:gpr_state.rdi,
						r8:gpr_state.r8,
						r9:gpr_state.r9,
						r10:gpr_state.r10,
						r11:gpr_state.r11,
						r12:gpr_state.r12,
						r13:gpr_state.r13,
						r14:gpr_state.r14,
						r15:gpr_state.r15,
					};
					// Switch to Restored Control Registers.
					let gcr4=unsafe{vmreadptr(GUEST_CR4)}.unwrap() as u64;
					write_cr3(gcr3);
					write_cr4(gcr4);
					unsafe
					{
						// Switch to Restored IDT.
						let gidtr=DescriptorTable
						{
							limit:vmread32(GUEST_IDTR_LIMIT).unwrap() as u16,
							base:vmreadptr(GUEST_IDTR_BASE).unwrap() as u64
						};
						write_idtr(&raw const gidtr);
						// Switch to Restored GDT.
						let ggdtr=DescriptorTable
						{
							limit:vmread32(GUEST_GDTR_LIMIT).unwrap() as u16,
							base:vmreadptr(GUEST_GDTR_BASE).unwrap() as u64
						};
						write_gdtr(&raw const ggdtr);
						// Switch to Restored TSS.
						let tr_sel=vmread32(GUEST_TR_SELECTOR).unwrap() as u16;
						// Before actually switching TSS, make it available.
						let tss_entry=(ggdtr.base+(tr_sel as u64 & 0xFFF8)) as *mut SystemSegmentDescriptor;
						(*tss_entry).flags=SegmentFlags::AVAILABLE_TSS;
						((ggdtr.base+tr_sel as u64+0x5) as *mut u8).write(0x89);
						// Switch it.
						write_tr(tr_sel);
					}
					// Return to the caller in Host Mode.
					unsafe
					{
						nvc_vt_resume_without_entry(&raw const saved_state);
					}
					// Never reaches here!
				}
				_=>
				{
					error!("Unknown Hypercall Code 0x{vmcall_func:X} is called!");
					unsafe{inject_event(INVALID_OPCODE_FAULT,EventType::HardwareException,None,true,0);}
				}
			}
		}
		else
		{
			// This hypercall might be compliant to Microsoft TLFS.
			// Check if forwarder exists.
			#[cfg(windows)]
			if hv.mshvcall_forwarder.is_some()
			{
				let stack:&mut VtStackTop=unsafe{&mut *self.hv_stack.virt.byte_add(HYPERVISOR_STACK_SIZE-size_of::<VtStackTop>()).cast()};
				let hvcall_code=TlfsHypercallCode::from_bits(gpr_state.rcx);
				// Construct the forward stack.
				let mut fwd_stack=MshvForwardStack::from_context(gpr_state,&mut stack.volatile_xmms);
				if hvcall_code.fast()
				{
					unsafe
					{
						nvc_forward_fast_hypercall(&raw mut fwd_stack);
						fwd_stack.to_context(gpr_state);
						self.advance_rip();
					}
				}
				else
				{
					// FIXME: This sort of hypercall (e.g.: HvPostMessage) only happens in Hyper-V. It seems Windows does not invoke such hypercalls in QEMU/KVM.
					info!("Microsoft Memory-Mapped Hypercall is intercepted! Code: 0x{:X}, Input GPA: 0x{:X}, Output GPA: 0x{:X}",hvcall_code.into_bits(),gpr_state.rdx,gpr_state.r8);
					unsafe
					{
						gpr_state.rax=nvc_forward_memory_mapped_hypercall(hvcall_code.into_bits(),gpr_state.rdx,gpr_state.r8,gpr_state.rax);
						info!("Return-Value: 0x{:X}",gpr_state.rax);
						self.advance_rip();
					}
				}
			}
			else
			{
				unimplemented!("Microsoft TLFS Hypercall handler is not implemented yet!");
			}
		}
	}

	fn handle_cr_access(&mut self,context:&mut VtStackTop)
	{
		let gpr_state=&mut context.gpr_state;
		let q=ControlRegisterQualification::read();
		debug!("CR Index: {}, GPR Index: {}, Access: {}",q.cr_index(),q.access_type(),q.gpr_index());
		match q.access_type()
		{
			ControlRegisterQualification::WRITE_CR=>
			{
				gpr_state.rsp=unsafe{vmread64(GUEST_RSP)}.unwrap();
				let new_value=gpr_state.read(q.gpr_index()).unwrap() as usize;
				debug!("New Value: 0x{new_value:X}");
				unsafe
				{
					match q.cr_index()
					{
						4=>vmwriteptr(GUEST_CR4,new_value|CR4_VMXE as usize),
						x=>panic!("Interception to CR{x} is unsupported!")
					};
				}
			}
			_=>error!("Unrecognized Access: {}",q.access_type())
		}
		panic!("CR-Access Exit is not implemented!");
	}

	fn handle_rdmsr(&mut self,context:&mut VtStackTop)
	{
		let index:u32=context.gpr_state.rcx as u32;
		let ret_val:Option<u64>=match index
		{
			// Prevent the Guest from updating microcode.
			// Returning u64::MAX should prevent the guest from loading microcodes,
			// unless they ignore the current version of microcode.
			MSR_BIOS_UPDATE_TRIGGER=>Some(u64::MAX),
			_=>panic!("Unexpected interception to rdmsr! MSR-Index: 0x{index:X}")
		};
		match ret_val
		{
			Some(v)=>
			{
				context.gpr_state.rax=v&u32::MAX as u64;
				context.gpr_state.rdx=v>>32;
				self.advance_rip();
			}
			None=>unsafe{inject_event(GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true,0)}
		}
	}

	fn handle_wrmsr(&mut self,context:&mut VtStackTop)
	{
		let index:u32=context.gpr_state.rcx as u32;
		let fault=match index
		{
			// Prevent the Guest from updating microcode.
			// Do so by ignoring the update request.
			MSR_BIOS_UPDATE_TRIGGER=>false,
			_=>panic!("Unexpected interception to wrmsr! MSR-Index: 0x{index:X}")
		};
		unsafe
		{
			if fault
			{
				inject_event(GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true,0);
			}
			else
			{
				self.advance_rip();
			}
		};
	}

	fn handle_invalid_state(&mut self,_context:&mut VtStackTop)
	{
		self.dump_current_vmcs();
		panic!("Invalid Guest State!");
	}

	fn handle_invalid_auto_msr(&mut self,_context:&mut VtStackTop)
	{
		panic!("Invalid Auto-MSR List!");
	}

	fn handle_ept_violation(&mut self,_context:&mut VtStackTop)
	{
		let gpa=unsafe{vmread64(GUEST_PHYSICAL_ADDRESS)}.unwrap();
		let rip=unsafe{vmreadptr(GUEST_RIP).unwrap()};
		if is_ci_phys_page(gpa)
		{
			let mut inslen=self.cached_ctxt.exit_instruction_length();
			error!("CI-fault for GPA=0x{gpa:X} is intercepted! rip=0x{rip:X}, Instruction-Length: {inslen}");
			if inslen==0
			{
				// VMware's nested virtualization does not forward instruction length upon EPT-violation.
				// Fetch instruction from guest and manually advance rip.
				warn!("Instruction-Length from VMCS is 0! Fetching instruction via software...");
				let instruction_bytes:[u8;15]=self.fetch_instruction();
				let mut decoder=Decoder::with_ip(self.get_current_bitness(),&instruction_bytes,rip as u64,0);
				let ins=decoder.decode();
				inslen=ins.len() as u32;
				let mut mnemonic=MnemonicString::default();
				self.disasm_fmter.format(&ins,&mut mnemonic);
				debug!("CI-fault instruction bytes: {:02X?} | {}",&instruction_bytes[..ins.len()],mnemonic);
			}
			self.advance_rip_manually(inslen);
		}
		else
		{
			panic!("Unexpected EPT Violation happened! GPA=0x{gpa:X}, rip=0x{rip:X}");
		}
	}

	fn handle_ept_misconfig(&mut self,_context:&mut VtStackTop)
	{
		let gpa=unsafe{vmread64(GUEST_PHYSICAL_ADDRESS)}.unwrap();
		let hv:&mut VtHypervisor=unsafe{&mut *self.hypervisor.cast()};
		let p=unsafe{&mut *hv.eptm.locate_pdpte(gpa)};
		error!("Dumping EPT Page Entries for EPT Misconfiguration...");
		error!("EPT PDPTE Entry: 0x{:016X}",p.into_bits());
		if let Some(p)=hv.eptm.locate_pde(gpa)
		{
			error!("EPT PDE Entry: 0x{:016X}",unsafe{(*p).into_bits()});
		}
		if let Some(p)=hv.eptm.locate_pte(gpa)
		{
			error!("EPT PTE Entry: 0x{:016X}",unsafe{(*p).into_bits()});
		}
		panic!("EPT Misconfiguration happened! GPA=0x{gpa:X}");
	}

	fn handle_xsetbv(&mut self,context:&mut VtStackTop)
	{
		let gpr_state=&mut context.gpr_state;
		let index=gpr_state.rcx as u32;
		let value=(gpr_state.rax&u32::MAX as u64)|(gpr_state.rdx<<32);
		debug!("The xsetbv instruction is intercepted! Index={index}, Value=0x{value:16X}");
		unsafe
		{
			#[repr(C)] struct XcrContext
			{
				index:u32,
				value:u64
			}
			extern "C" fn try_xsetbv(context:*mut c_void)
			{
				let ctxt:&mut XcrContext=unsafe{&mut *context.cast()};
				unsafe{_xsetbv(ctxt.index,ctxt.value)};
			}
			let mut x=XcrContext{index,value};
			// Expect an exception may come.
			match try_task(try_xsetbv,(&raw mut x).cast())
			{
				Ok(_)=>self.advance_rip(),
				Err(e)=>
				{
					error!("The xsetbv task failed! Vector={}, Error-Code: {:X?}",e.vector,e.error_code);
					inject_event(e.vector,EventType::HardwareException,e.error_code,true,0);
				}
			}
		}
	}

	fn handle_unknown(&mut self,_context:&mut VtStackTop)
	{
		let exit_reason=unsafe{vmread32(VMEXIT_REASON).unwrap()};
		panic!("Unknown VM-Exit is intercepted! Exit-Reason: {} (0x{exit_reason:X})",exit_reason&0xFFFF);
	}
}

#[unsafe(no_mangle)] unsafe extern "C" fn nvc_vt_exit_handler(context:*mut VtStackTop)
{
	let ctxt=unsafe{&mut *context};
	let vcpu=unsafe{&mut *ctxt.vcpu};
	vcpu.cached_ctxt.reset();
	ctxt.gpr_state.rsp=vcpu.cached_ctxt.rsp;
	ctxt.guest_frame.return_rsp=vcpu.cached_ctxt.rsp;
	ctxt.guest_frame.return_rip=vcpu.cached_ctxt.rip;
	let handler=dispatch_handler(vcpu.cached_ctxt.exit_reason);
	handler(unsafe{&mut *ctxt.vcpu},ctxt);
}

#[unsafe(no_mangle)] unsafe extern "C" fn nvc_vt_resume_failure(_context:*mut VtStackTop,_vmx_status:u8)
{
	panic!("VM-Entry failed on resume!");
}

pub const INTERCEPTED_EXCEPTION_NMI:u32=0;
pub const INTERCEPTED_EXTERNAL_INTERRUPT:u32=1;
pub const INTERCEPTED_TRIPLE_FAULT:u32=2;
pub const INTERCEPTED_INIT_SIGNAL:u32=3;
pub const INTERCEPTED_STARTUP_IPI:u32=4;
pub const INTERCEPTED_IO_SMI:u32=5;
pub const INTERCEPTED_OTHER_SMI:u32=6;
pub const INTERCEPTED_INTERRUPT_WINDOW:u32=7;
pub const INTERCEPTED_NMI_WINDOW:u32=8;
pub const INTERCEPTED_TASK_SWITCH:u32=9;
pub const INTERCEPTED_CPUID:u32=10;
pub const INTERCEPTED_GETSEC:u32=11;
pub const INTERCEPTED_HLT:u32=12;
pub const INTERCEPTED_INVD:u32=13;
pub const INTERCEPTED_INVLPG:u32=14;
pub const INTERCEPTED_RDPMC:u32=15;
pub const INTERCEPTED_RDTSC:u32=16;
pub const INTERCEPTED_RSM:u32=17;
pub const INTERCEPTED_VMCALL:u32=18;
pub const INTERCEPTED_VMCLEAR:u32=19;
pub const INTERCEPTED_VMLAUNCH:u32=20;
pub const INTERCEPTED_VMPTRLD:u32=21;
pub const INTERCEPTED_VMPTRST:u32=22;
pub const INTERCEPTED_VMREAD:u32=23;
pub const INTERCEPTED_VMRESUME:u32=24;
pub const INTERCEPTED_VMWRITE:u32=25;
pub const INTERCEPTED_VMXOFF:u32=26;
pub const INTERCEPTED_VMXON:u32=27;
pub const INTERCEPTED_CR_ACCESS:u32=28;
pub const INTERCEPTED_DR_ACCESS:u32=29;
pub const INTERCEPTED_IO:u32=30;
pub const INTERCEPTED_RDMSR:u32=31;
pub const INTERCEPTED_WRMSR:u32=32;
pub const INVALID_GUEST_STATE:u32=33;
pub const MSR_LOADING_FAILURE:u32=34;
pub const INTERCEPTED_MWAIT:u32=36;
pub const MONITOR_TRAP_FLAG:u32=37;
pub const INTERCEPTED_MONITOR:u32=39;
pub const INTERCEPTED_PAUSE:u32=40;
pub const MACHINE_CHECK_AT_ENTRY:u32=41;
pub const TPR_BELOW_THRESHOLD:u32=43;
pub const INTERCEPTED_APIC_ACCESS:u32=44;
pub const VIRTUALIZED_EOI:u32=45;
pub const INTERCEPTED_GDTR_IDTR_ACCESS:u32=46;
pub const INTERCEPTED_LDTR_TR_ACCESS:u32=47;
pub const EPT_VIOLATION:u32=48;
pub const EPT_MISCONFIGURATION:u32=49;
pub const INTERCEPTED_INVEPT:u32=50;
pub const INTERCEPTED_RDTSCP:u32=51;
pub const VMX_PREEMPTION_TIMER_EXPIRED:u32=52;
pub const INTERCEPTED_INVVPID:u32=53;
pub const INTERCEPTED_WBINVD:u32=54;
pub const INTERCEPTED_XSETBV:u32=55;
pub const INTERCEPTED_APIC_WRITE:u32=56;
pub const INTERCEPTED_RDRAND:u32=57;
pub const INTERCEPTED_INVPCID:u32=58;
pub const INTERCEPTED_VMFUNC:u32=59;
pub const INTERCEPTED_ENCLS:u32=60;
pub const INTERCEPTED_RDSEED:u32=61;
pub const INTERCEPTED_PML_FULL:u32=62;
pub const INTERCEPTED_XSAVES:u32=63;
pub const INTERCEPTED_XRSTORS:u32=64;
pub const INTERCEPTED_PCONFIG:u32=65;
pub const INTERCEPTED_SPP_RELATED_EVENT:u32=66;
pub const INTERCEPTED_UMWAIT:u32=67;
pub const INTERCEPTED_TPAUSE:u32=68;
pub const INTERCEPTED_LOADIWKEY:u32=69;
pub const INTERCEPTED_ENCLV:u32=70;
pub const ENQCMD_PASID_TRANSLATION_FAILURE:u32=72;
pub const ENQCMDS_PASID_TRANSLATION_FAILURE:u32=73;
pub const INTERCEPTED_BUS_LOCK:u32=74;
pub const INSTRUCTION_TIMEOUT:u32=75;
pub const INTERCEPTED_SEAMCALL:u32=76;
pub const INTERCEPTED_TDCALL:u32=77;
pub const INTERCEPTED_RDMSRLIST:u32=78;
pub const INTERCEPTED_WRMSRLIST:u32=79;

const VT_MAXIMUM_CODE:usize=80;

type VtExitHandler=fn(&mut VtVcpu,&mut VtStackTop);

// Defining sparse array is much easier in Rust than in C!
const VT_EXIT_HANDLERS:[VtExitHandler;VT_MAXIMUM_CODE]=
{
	let mut array:[VtExitHandler;VT_MAXIMUM_CODE]=[VtVcpu::handle_unknown;VT_MAXIMUM_CODE];
	array[INTERCEPTED_TRIPLE_FAULT as usize]=VtVcpu::handle_triple_fault;
	array[INTERCEPTED_INIT_SIGNAL as usize]=VtVcpu::handle_init;
	array[INTERCEPTED_CPUID as usize]=VtVcpu::handle_cpuid;
	array[INTERCEPTED_GETSEC as usize]=VtVcpu::handle_getsec;
	array[INTERCEPTED_INVD as usize]=VtVcpu::handle_invd;
	array[INTERCEPTED_VMCALL as usize]=VtVcpu::handle_vmcall;
	array[INTERCEPTED_CR_ACCESS as usize]=VtVcpu::handle_cr_access;
	array[INTERCEPTED_RDMSR as usize]=VtVcpu::handle_rdmsr;
	array[INTERCEPTED_WRMSR as usize]=VtVcpu::handle_wrmsr;
	array[INVALID_GUEST_STATE as usize]=VtVcpu::handle_invalid_state;
	array[MSR_LOADING_FAILURE as usize]=VtVcpu::handle_invalid_auto_msr;
	array[EPT_VIOLATION as usize]=VtVcpu::handle_ept_violation;
	array[EPT_MISCONFIGURATION as usize]=VtVcpu::handle_ept_misconfig;
	array[INTERCEPTED_XSETBV as usize]=VtVcpu::handle_xsetbv;
	array
};

#[inline] fn dispatch_handler(exit_reason:VmxExitReason)->VtExitHandler
{
	match VT_EXIT_HANDLERS.get(exit_reason.basic_exit_reason() as usize)
	{
		Some(f)=>*f,
		None=>VtVcpu::handle_unknown
	}
}