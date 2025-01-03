/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file handles VM-Exits in AMD-V of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::slice;

use paste::paste;
use iced_x86::*;

use decode::dispatch_decoder;
use npt::NptFaultCode;
use xpf_core::{ci::is_ci_phys_page, x86::interrupts::*};

use super::*;
use crate::mshv_core::cpuid::*;

pub(super) fn svm_apic_output_handler(_region:&IoRegion<u64>,address:u64,size:u64,value:*const c_void,_context:*mut c_void)
{
	// Current implementation is simply pass-thru.
	unsafe
	{
		match size
		{
			1=>(address as *mut u8).write(value.cast::<u8>().read()),
			2=>(address as *mut u16).write(value.cast::<u16>().read()),
			4=>(address as *mut u32).write(value.cast::<u32>().read()),
			8=>(address as *mut u64).write(value.cast::<u64>().read()),
			_=>panic!("Unknown size: {size}!")
		}
	}
}

// Place all VM-Exit handlers from the subverted host into this implementation!
// Rules of thumb in implementing VM-Exit Handlers: Do not allocate memories from heap!
impl SvmVcpu
{
	fn handle_unknown(&mut self,_gpr_state:&mut GprState)
	{
		let vmcb=self.vmcb.virt;
		let intercept_code:i64=unsafe{vmread(vmcb,EXIT_CODE)};
		println!("Unknown VM-Exit is intercepted! Code: 0x{:016X}",intercept_code);
	}

	fn handle_cpuid(&mut self,gpr_state:&mut GprState)
	{
		let ia=gpr_state.rax as u32;
		let ic=gpr_state.rcx as u32;
		let (a,b,c,d)=
		if (ia&0x40000000)==0x40000000
		{
			// This is Hypervisor's CPUID.
			let leaf_func=(ia&0x3FFFFFFF) as usize;
			if leaf_func>=MSHV_CPUID_HANDLERS_COUNT
			{
				(0,0,0,0)
			}
			else
			{
				MSHV_CPUID_HANDLERS[leaf_func](ia,ic)
			}
		}
		else
		{
			// This is processor's CPUID.
			// Execute the original CPUID and filter stuff.
			let (mut a,mut b,mut c,mut d)=cpuid2(ia,ic);
			match ia
			{
				CPUID_STD_PROCESSOR_FEATURE=>c|=CPUID_UNDER_HYPERVISOR,
				// NoirVisor currently does not support nested virtualization.
				CPUID_EXT_PROCESSOR_FEATURE=>b&=!CPUID_SVM,
				CPUID_EXT_SECURE_VIRTUAL_MACHINE_FEATURE=>
				{
					a=0;
					b=0;
					c=0;
					d=0;
				}
				_=>{}
			}
			(a,b,c,d)
		};
		unsafe
		{
			// Write the results back to eax, ebx, ecx and edx.
			(&raw mut gpr_state.rax).cast::<u32>().write(a);
			(&raw mut gpr_state.rbx).cast::<u32>().write(b);
			(&raw mut gpr_state.rcx).cast::<u32>().write(c);
			(&raw mut gpr_state.rdx).cast::<u32>().write(d);
			// Advance the rip.
			advance_rip(self.vmcb.virt);
		}
	}

	/// # `handle_rdmsr`
	/// Handles an incoming `rdmsr` on this vCPU.
	/// Return `Some(u64)` if this `rdmsr` request should advance rip.
	/// Return `None` if this `rdmsr` request failed. Exception was injected.
	fn handle_rdmsr(&mut self,index:u32)->Option<u64>
	{
		match index
		{
			MSR_EFER=>
			{
				// Read the EFER value from VMCB.
				let v:u64=unsafe{vmread(self.vmcb.virt,GUEST_EFER)};
				// The SVME bit should be filtered.
				if self.nested_hvm.svme
				{
					Some(v)
				}
				else
				{
					Some(v&!MSR_EFER_SVME)
				}
			}
			MSR_TSC_RATIO=>
			{
				// TSC Ratio is not supported.
				unsafe{inject_event(self.vmcb.virt,GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true)};
				None
			}
			MSR_VMCR=>
			{
				Some(self.nested_hvm.vmcr)
			}
			MSR_IGNNE=>
			{
				Some(if self.nested_hvm.ignne {1} else {0})
			}
			MSR_SMM_CTRL=>
			{
				unsafe{inject_event(self.vmcb.virt,GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true)};
				None
			}
			MSR_HSAVE_PA=>
			{
				Some(self.nested_hvm.hsave_pa)
			}
			_=>
			{
				println!("Unexpected rdmsr is intercepted! Index=0x{:X}",index);
				unsafe
				{
					inject_event(self.vmcb.virt,GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true);
				}
				None
			}
		}
	}

	/// # `handle_wrmsr`
	/// Handles an incoming `wrmsr` on this vCPU.
	/// Return `true` if this `wrmsr` request should advance rip.
	/// Return `false` if this `wrmsr` request failed. Exception was injected.
	fn handle_wrmsr(&mut self,index:u32,value:u64)->bool
	{
		match index
		{
			MSR_EFER=>
			{
				let svme=(value&MSR_EFER_SVME)==MSR_EFER_SVME;
				self.nested_hvm.svme=svme;
				unsafe
				{
					// SVME bit should always be set.
					vmwrite(self.vmcb.virt,GUEST_EFER,value|MSR_EFER_SVME);
					// We have updated EFER. Therefore, Control-Register fields should be invalidated.
					vmcb_clean_cr(self.vmcb.virt);
				};
				true
			}
			MSR_TSC_RATIO=>
			{
				// TSC Ratio is not supported.
				unsafe{inject_event(self.vmcb.virt,GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true)};
				false
			}
			MSR_VMCR=>
			{
				todo!("TODO: VMCR Emulation not implemented yet!");
			}
			MSR_IGNNE=>
			{
				// Only the lowest bit can be set to 1.
				if (value&0xFFFFFFFFFFFFFFFE)!=0
				{
					unsafe{inject_event(self.vmcb.virt,GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true)};
					false
				}
				else
				{
					self.nested_hvm.ignne=value==1;
					true
				}
			}
			MSR_SMM_CTRL=>
			{
				unsafe{inject_event(self.vmcb.virt,GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true)};
				false
			}
			MSR_HSAVE_PA=>
			{
				// HSAVE must be aligned on page-boundary.
				if page_4kb_offset(value as usize)!=0
				{
					unsafe{inject_event(self.vmcb.virt,GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true)};
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
				println!("Unexpected wrmsr is intercepted! Index=0x{:X}, Value=0x{:016X}",index,value);
				unsafe
				{
					inject_event(self.vmcb.virt,GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true);
				}
				false
			}
		}
	}

	fn handle_msr(&mut self,gpr_state:&mut GprState)
	{
		let index=gpr_state.rcx as u32;
		let op_write:bool=unsafe{vmread(self.vmcb.virt,EXIT_INFO1)};
		if op_write
		{
			let value=(gpr_state.rax&0xFFFFFFFF)|(gpr_state.rdx<<32);
			if self.handle_wrmsr(index,value)
			{
				// Advance rip.
				unsafe{advance_rip(self.vmcb.virt)};
			}
		}
		else if let Some(value)=self.handle_rdmsr(index)
		{
			// Write back to registers and advance rip.
			let lo=(value&0xFFFFFFFF) as u32;
			let hi=(value>>32) as u32;
			unsafe
			{
				(&raw mut gpr_state.rax).cast::<u32>().write(lo);
				(&raw mut gpr_state.rdx).cast::<u32>().write(hi);
				advance_rip(self.vmcb.virt);
			}
		}
	}

	fn handle_shutdown(&mut self,_gpr_state:&mut GprState)
	{
		panic!("Shutdown occured!");
	}

	fn handle_vmrun(&mut self,gpr_state:&mut GprState)
	{
		panic!("Nested virtualization is unsupported! Nested VMCB RAX=0x{:016X}",gpr_state.rax);
	}

	fn handle_vmmcall(&mut self,gpr_state:&mut GprState)
	{
		panic!("Hypercall is unsupported! Code: 0x{:08X}",gpr_state.rax as u32);
	}

	fn handle_vmload(&mut self,gpr_state:&mut GprState)
	{
		panic!("Nested virtualization is unsupported! Nested VMCB RAX=0x{:016X}",gpr_state.rax);
	}

	fn handle_vmsave(&mut self,gpr_state:&mut GprState)
	{
		panic!("Nested virtualization is unsupported! Nested VMCB RAX=0x{:016X}",gpr_state.rax);
	}

	fn handle_stgi(&mut self,_gpr_state:&mut GprState)
	{
		panic!("Nested virtualization is unsupported!");
	}

	fn handle_clgi(&mut self,_gpr_state:&mut GprState)
	{
		panic!("Nested virtualization is unsupported!");
	}

	fn handle_skinit(&mut self,_gpr_state:&mut GprState)
	{
		panic!("Nested virtualization is unsupported!");
	}

	fn handle_npf(&mut self,gpr_state:&mut GprState)
	{
		let vmcb=self.vmcb.virt;
		let fault:NptFaultCode=unsafe{vmread(vmcb,EXIT_INFO1)};
		let gpa:u64=unsafe{vmread(vmcb,EXIT_INFO2)};
		let rip:u64=unsafe{vmread(vmcb,GUEST_RIP)};
		// Check if this #NPF is due to Code Integrity violation.
		if is_ci_phys_page(gpa)
		{
			println!("CI-fault is intercepted!");
		}
		else if !fault.is_code_read()
		{
			let hv:&mut SvmHypervisor=unsafe{&mut *self.hypervisor.cast()};
			// This could be MMIO Filter.
			let ins_bytes:&[u8]=unsafe{slice::from_raw_parts(self.vmcb.virt.byte_add(GUEST_INSTRUCTION_BYTES).cast(),15)};
			// Check bitness.
			let bitness:u32=unsafe
			{
				if vmcb_bt32(self.vmcb.virt,GUEST_CS_ATTRIB,9)	// The CS.L bit.
				{
					64
				}
				else if vmcb_bt32(self.vmcb.virt,GUEST_CS_ATTRIB,10)	// The CS.D bit.
				{
					32
				}
				else
				{
					16
				}
			};
			// Call disassembler.
			let mut decoder=Decoder::with_ip(bitness,ins_bytes,rip,DecoderOptions::AMD);
			assert!(decoder.can_decode());
			let ins_info=decoder.decode();
			let gpr_array=gpr_state as *mut GprState as *mut u64;
			match ins_info.mnemonic()
			{
				Mnemonic::Mov=>
				{
					// Decode the operand.
					if fault.is_write()
					{
						let data=match ins_info.op1_kind()
						{
							OpKind::Register=>unsafe{gpr_array.add(ins_info.op1_register().number()).read()},
							OpKind::Immediate8=>ins_info.immediate8().into(),
							OpKind::Immediate16=>ins_info.immediate16().into(),
							OpKind::Immediate32=>ins_info.immediate32().into(),
							_=>panic!("Unknown opcode kind: {:?}!",ins_info.op1_kind())
						};
						if let Err(e)=hv.mmio_space.dispatch_output(gpa,(ins_info.op_code().operand_size()>>3).into(),(&raw const data).cast(),self as *mut Self as *mut c_void)
						{
							panic!("Failed to dispatch MMIO output! Reason: {e}");
						}
					}
					else
					{
						unimplemented!("MMIO Input virtualization is not implemented!");
					};
				}
				_=>panic!("Unsupported instruction: {:?} is intercepted for decoding MMIO instruction!\nrip=0x{rip:X}, GPA=0x{gpa:X}",ins_info.mnemonic())
			}
			unsafe{advance_rip_manually(vmcb,ins_info.len())};
		}
	}

	fn handle_invalid(&mut self,_gpr_state:&mut GprState)
	{
		let intercept_code:i64=unsafe{vmread(self.vmcb.virt,EXIT_CODE)};
		panic!("Invalid State! Exit Code: 0x{:X}",intercept_code);
	}
}

/// # Safety
/// This function is unsafe is because it's called from assembly.
/// DO NOT CALL THIS FUNCTION FROM RUST CODE!
#[no_mangle] pub unsafe extern "C" fn nvc_svm_exit_handler(gpr_state:*mut GprState,vcpu:*mut SvmVcpu)
{
	let vp=&mut (*vcpu);
	let gpr=&mut (*gpr_state);
	let stack:*mut SvmStackTop=(vp.hv_stack.byte_add(HYPERVISOR_STACK_SIZE-size_of::<SvmStackTop>())).cast();
	let cur_vmcb=(*gpr_state).rax as *mut c_void;
	// Intercept code is supposed to be 64-bit, but Linux KVM has a bug that treats the intercept code as 32-bit.
	let intercept_code:i32=vmread(cur_vmcb,EXIT_CODE);
	let decoder=dispatch_decoder(intercept_code as i64);
	let handler=dispatch_handler(intercept_code as i64);
	// If VMCB-Clean-Bits is supported, we may cache the VMCB fields.
	if (*vcpu).vmcb_clean {vmwrite::<u32>((*vcpu).vmcb.virt,VMCB_CLEAN_BITS,0xFFFFFFFF)};
	// Handle the VM-Exit!
	gpr.rax=vmread(cur_vmcb,GUEST_RAX);
	decoder(vp);
	handler(vp,gpr);
	// The rax in GPR state should be the physical address of VMCB
	// in order to execute the vmrun instruction properly.
	// Reading/Writing the rax is like the vmptrst/vmptrld instruction in Intel VT-x.
	gpr.rax=(*stack).guest_vmcb_pa;
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

type SvmExitHandler=fn(&mut SvmVcpu,&mut GprState);

// Defining sparse array is much easier in Rust than in C!
const SVM_EXIT_HANDLER_GROUP1:[SvmExitHandler;SVM_MAXIMUM_CODE1]=
{
	let mut array:[SvmExitHandler;SVM_MAXIMUM_CODE1]=[SvmVcpu::handle_unknown;SVM_MAXIMUM_CODE1];
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

const SVM_EXIT_HANDLER_GROUP2:[SvmExitHandler;SVM_MAXIMUM_CODE2]=
{
	let mut array:[SvmExitHandler;SVM_MAXIMUM_CODE2]=[SvmVcpu::handle_unknown;SVM_MAXIMUM_CODE2];
	array[(NESTED_PAGE_FAULT-0x400) as usize]=SvmVcpu::handle_npf;
	array
};

const SVM_EXIT_HANDLER_GROUP_NEGATIVE:[SvmExitHandler;SVM_MAXIMUM_NEGATIVE]=
{
	let mut array:[SvmExitHandler;SVM_MAXIMUM_NEGATIVE]=[SvmVcpu::handle_unknown;SVM_MAXIMUM_NEGATIVE];
	array[!INVALID_GUEST_STATE as usize]=SvmVcpu::handle_invalid;
	array
};

const SVM_EXIT_HANDLER_GROUPS:[&[SvmExitHandler];SVM_MAXIMUM_GROUPS]=[&SVM_EXIT_HANDLER_GROUP1,&SVM_EXIT_HANDLER_GROUP2];
pub const SVM_EXIT_HANDLER_GROUP_LIMITS:[usize;SVM_MAXIMUM_GROUPS]=[SVM_MAXIMUM_CODE1,SVM_MAXIMUM_CODE2];

// This function is supposed to be time-sensitive!
// The O(1) method of dispatching handler.
// If we use match-expression, it could be O(n) if optimizer went dumb!
#[inline] fn dispatch_handler(intercept_code:i64)->SvmExitHandler
{
	if intercept_code<0
	{
		let index:usize=!intercept_code as usize;
		if index<SVM_MAXIMUM_NEGATIVE
		{
			SVM_EXIT_HANDLER_GROUP_NEGATIVE[index]
		}
		else
		{
			SvmVcpu::handle_unknown
		}
	}
	else
	{
		let group:usize=(intercept_code as usize)>>10;
		if group<SVM_MAXIMUM_GROUPS
		{
			let index:usize=(intercept_code as usize)&0x3ff;
			if index<SVM_EXIT_HANDLER_GROUP_LIMITS[group]
			{
				SVM_EXIT_HANDLER_GROUPS[group][index]
			}
			else
			{
				SvmVcpu::handle_unknown
			}
		}
		else
		{
			SvmVcpu::handle_unknown
		}
	}
}