/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file handles VM-Exits in AMD-V of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use npt::NptFaultCode;

use super::*;
use crate::mshv_core::cpuid::*;

// Place all VM-Exit handlers from the subverted host into this implementation!
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

	fn handle_npf(&mut self,_gpr_state:&mut GprState)
	{
		let vmcb=self.vmcb.virt;
		let fault:NptFaultCode=unsafe{vmread(vmcb,EXIT_INFO1)};
		let gpa:u64=unsafe{vmread(vmcb,EXIT_INFO2)};
		panic!("Nested Page Fault is intercepted! GPA=0x{:016X}\nReason: {}",gpa,fault);
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
	let handler=dispatch_handler(intercept_code as i64);
	// Handle the VM-Exit!
	gpr.rax=vmread(cur_vmcb,GUEST_RAX);
	handler(vp,gpr);
	// The rax in GPR state should be the physical address of VMCB
	// in order to execute the vmrun instruction properly.
	// Reading/Writing the rax is like the vmptrst/vmptrld instruction in Intel VT-x.
	gpr.rax=(*stack).guest_vmcb_pa;
}

pub const SVM_MAXIMUM_GROUPS:usize=2;
pub const SVM_MAXIMUM_CODE1:usize=0xA5;
pub const SVM_MAXIMUM_CODE2:usize=0x4;
pub const SVM_MAXIMUM_NEGATIVE:usize=3;

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

pub const NESTED_PAGE_FAULT:i64=0x400;
pub const AVIC_INCOMPLETE_IPI:i64=0x401;
pub const AVIC_NO_ACCELERATION:i64=0x402;
pub const INTERCEPTED_VMGEXIT:i64=0x403;

pub const INVALID_GUEST_STATE:i64=-1;
pub const INTERCEPTED_VMSA_BUSY:i64=-2;
pub const IDLE_REQUIRED:i64=-3;

type SvmExitHandler=fn(&mut SvmVcpu,&mut GprState);

// Defining sparse array is much easier in Rust than in C!
const SVM_EXIT_HANDLER_GROUP1:[SvmExitHandler;SVM_MAXIMUM_CODE1]=
{
	let mut array:[SvmExitHandler;SVM_MAXIMUM_CODE1]=[SvmVcpu::handle_unknown;SVM_MAXIMUM_CODE1];
	array[INTERCEPTED_CPUID as usize]=SvmVcpu::handle_cpuid;
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
const SVM_EXIT_HANDLER_GROUP_LIMITS:[usize;SVM_MAXIMUM_GROUPS]=[SVM_MAXIMUM_CODE1,SVM_MAXIMUM_CODE2];

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