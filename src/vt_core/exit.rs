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

use crate::{mshv_core::cpuid::MSHV_CPUID_HANDLERS, xpf_core::{asm::{cpuid::cpuid2, vt::*}, nvbdk::GprState, x86::{cpuid::*, interrupts::InterruptStackFrameWithErrorCode}},*};
use super::{ia32::cpuid::CPUID_VMX, vmcs::*, VtVcpu};

impl VtVcpu
{
	fn handle_triple_fault(&mut self,_gpr_state:&mut GprState)
	{
		panic!("Triple-Fault occured!");
	}

	fn handle_cpuid(&mut self,gpr_state:&mut GprState)
	{
		let ia=gpr_state.rax as u32;
		let ic=gpr_state.rcx as u32;
		let (a,b,c,d)=
		if (ia&0x40000000)==0x40000000
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
			// This is processor's CPUID.
			// Execute the original CPUID and filter stuff.
			#[allow(unused_mut)]
			let (mut a,mut b,mut c,mut d)=cpuid2(ia,ic);
			if ia==CPUID_STD_PROCESSOR_FEATURE
			{
				c|=CPUID_UNDER_HYPERVISOR;
				c&=!CPUID_VMX;
			}
			(a,b,c,d)
		};
		unsafe
		{
			// Write the results back to eax, ebx, ecx and edx and clear the higher 32 bits.
			gpr_state.rax=a as u64;
			gpr_state.rbx=b as u64;
			gpr_state.rcx=c as u64;
			gpr_state.rdx=d as u64;
			// Advance the rip.
			advance_rip();
		}
	}

	fn handle_cr_access(&mut self,gpr_state:&mut GprState)
	{
		let q=ControlRegisterQualification::read();
		println!("CR Index: {}, GPR Index: {}, Access: {}",q.get_cr_index(),q.get_access_type(),q.get_gpr_index());
		match q.get_access_type()
		{
			ControlRegisterQualification::WRITE_CR=>
			{
				gpr_state.rsp=unsafe{vmread64(GUEST_RSP)}.unwrap();
				println!("New Value: 0x{:X}",gpr_state.read(q.get_gpr_index()).unwrap());
			}
			_=>println!("Unrecognized Access: {}",q.get_access_type())
		}
		panic!("CR-Access Exit is not implemented!");
	}

	fn handle_invalid(&mut self,_gpr_state:&mut GprState)
	{
		panic!("Invalid Guest State!");
	}

	fn handle_unknown(&mut self,_gpr_state:&mut GprState)
	{
		let exit_reason=unsafe{vmread32(VMEXIT_REASON).unwrap()};
		panic!("Unknown VM-Exit is intercepted! Exit-Reason: {} (0x{exit_reason:X})",exit_reason&0xFFFF);
	}
}

#[no_mangle] unsafe extern "C" fn nvc_vt_exit_handler(gpr_state:*mut GprState,vcpu:*mut VtVcpu,_guest_state:*mut InterruptStackFrameWithErrorCode)
{
	let exit_reason=vmread32(VMEXIT_REASON).unwrap();
	let vp=&mut (*vcpu);
	let gpr=&mut (*gpr_state);
	let handler=dispatch_handler(exit_reason);
	handler(vp,gpr);
}

#[no_mangle] unsafe extern "C" fn nvc_vt_resume_failure(_gpr_state:*mut GprState,_vcpu:*mut VtVcpu,_vmx_status:u8)
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

type VtExitHandler=fn(&mut VtVcpu,&mut GprState);

// Defining sparse array is much easier in Rust than in C!
const VT_EXIT_HANDLERS:[VtExitHandler;VT_MAXIMUM_CODE]=
{
	let mut array:[VtExitHandler;VT_MAXIMUM_CODE]=[VtVcpu::handle_unknown;VT_MAXIMUM_CODE];
	array[INTERCEPTED_TRIPLE_FAULT as usize]=VtVcpu::handle_triple_fault;
	array[INTERCEPTED_CPUID as usize]=VtVcpu::handle_cpuid;
	array[INTERCEPTED_CR_ACCESS as usize]=VtVcpu::handle_cr_access;
	array[INVALID_GUEST_STATE as usize]=VtVcpu::handle_invalid;
	array
};

#[inline] fn dispatch_handler(exit_reason:u32)->VtExitHandler
{
	match VT_EXIT_HANDLERS.get((exit_reason&0xFFFF)as usize)
	{
		Some(f)=>*f,
		None=>VtVcpu::handle_unknown
	}
}