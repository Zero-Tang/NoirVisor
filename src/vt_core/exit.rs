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

use core::arch::x86_64::_xsetbv;

use paste::paste;

use crate::{mshv_core::cpuid::MSHV_CPUID_HANDLERS, vt_core::nvc_vt_resume_without_entry, xpf_core::{asm::{cpuid::cpuid2, crdr::*, misc::wbinvd, msr::rdmsr, seg::*, vt::*}, hv_host::NOIR_HYPERCALL_CODE_CALLEXIT, nvbdk::GprState, x86::{cpuid::*, crdr::*, descriptors::{DescriptorTable, SegmentFlags, SystemSegmentDescriptor}, interrupts::{EventType, InterruptStackFrameWithErrorCode, GENERAL_PROTECTION_FAULT}}}, *};
use super::{ia32::{cpuid::CPUID_VMX, msr::*}, vmcs::*, VtVcpu};

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
					println!("Guest {} Segment: {:X?}",stringify!($name:upper),[<$name:lower>]);
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
		println!("Guest CR0=0x{cr0:X}, CR3=0x{cr3:X}, CR4=0x{cr4:X}, DR7=0x{dr7:X}");
		let ssp=unsafe{vmreadptr(GUEST_SSP)};
		let rsp=unsafe{vmreadptr(GUEST_RSP).unwrap()};
		let rip=unsafe{vmreadptr(GUEST_RIP).unwrap()};
		let rflags=unsafe{vmreadptr(GUEST_RFLAGS).unwrap()};
		println!("Guest rsp: 0x{rsp:X}, rip: 0x{rip:X}, rflags: 0x{rflags:X}, ssp: 0x{ssp:X?}");
		let efer=unsafe{vmread64(GUEST_MSR_IA32_EFER).unwrap()};
		let pat=unsafe{vmread64(GUEST_MSR_IA32_PAT).unwrap()};
		let dbg_ctrl=unsafe{vmread64(GUEST_MSR_IA32_DEBUG_CTRL).unwrap()};
		println!("Guest EFER: 0x{efer:X}, PAT: 0x{pat:X}, Debug-Control: 0x{dbg_ctrl:X}");
	}

	fn handle_triple_fault(&mut self,_gpr_state:&mut GprState)
	{
		panic!("Triple-Fault occured!");
	}

	fn handle_init(&mut self,gpr_state:&mut GprState)
	{
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
			let mut entry_ctrl=VmxEntryControls(vmread32(VMENTRY_CONTROLS).unwrap());
			entry_ctrl.set_ia32e_mode_guest(false);
			vmwrite32(VMENTRY_CONTROLS,entry_ctrl.0);
			// Invalid TLB since paging is switched off.
			let ivc=InvvpidContext::Single(vmread16(GUEST_VPID).unwrap());
			invvpid(&ivc);
			// Upon INIT, vCPU enters inactive state to wait for Startup-IPI.
			vmwrite32(GUEST_ACTIVITY_STATE,ActivityState::WAIT_FOR_SIPI);
		}
	}

	fn handle_cpuid(&mut self,gpr_state:&mut GprState)
	{
		let hv:&VtHypervisor=unsafe{&*self.hypervisor.cast()};
		let ia=gpr_state.rax as u32;
		let ic=gpr_state.rcx as u32;
		let (a,b,c,d)=
		if (ia&0x40000000)==0x40000000
		{
			if hv.features.get_cpuid_hv_presence()
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
				c|=if hv.features.get_cpuid_hv_presence() {CPUID_UNDER_HYPERVISOR} else {0};
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

	fn handle_getsec(&mut self,_gpr_state:&mut GprState)
	{
		println!("SMX Virtualization is not supported!");
		unsafe{advance_rip()};
	}

	fn handle_invd(&mut self,_gpr_state:&mut GprState)
	{
		println!("The invd instruction is executed!");
		// In Hyper-V, it invoked wbinvd at invd exit.
		wbinvd();
		unsafe{advance_rip()};
	}

	fn handle_vmcall(&mut self,gpr_state:&mut GprState)
	{
		let vmcall_func=gpr_state.rcx as u32;
		let gcr3=unsafe{vmreadptr(GUEST_CR3)}.unwrap() as u64;
		println!("The vmcall instruction is intercepted! Hypercall Leaf: 0x{vmcall_func:X}");
		match vmcall_func
		{
			NOIR_HYPERCALL_CODE_CALLEXIT=>
			{
				let grip=unsafe{vmreadptr(GUEST_RIP)}.unwrap();
				let hv=self.hypervisor as *mut VtHypervisor;
				let start=unsafe{(*hv).image_base} as usize;
				let end=start+unsafe{(*hv).image_size} as usize;
				if (start..end).contains(&grip)
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
				else
				{
					println!("Invalid Call to restore system! rip=0x{grip:016X}");
				}
			}
			_=>
			{
				println!("Unknown Hypercall Code 0x{vmcall_func:X} is called!");
			}
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
				let new_value=gpr_state.read(q.get_gpr_index()).unwrap() as usize;
				println!("New Value: 0x{new_value:X}");
				unsafe
				{
					match q.get_cr_index()
					{
						4=>vmwriteptr(GUEST_CR4,new_value|CR4_VMXE as usize),
						x=>panic!("Interception to CR{x} is unsupported!")
					};
				}
			}
			_=>println!("Unrecognized Access: {}",q.get_access_type())
		}
		panic!("CR-Access Exit is not implemented!");
	}

	fn handle_rdmsr(&mut self,gpr_state:&mut GprState)
	{
		let index:u32=gpr_state.rcx as u32;
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
				gpr_state.rax=v&0xffffffff;
				gpr_state.rdx=v>>32;
				unsafe{advance_rip()};
			}
			None=>unsafe{inject_event(GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true,0)}
		}
	}

	fn handle_wrmsr(&mut self,gpr_state:&mut GprState)
	{
		let index:u32=gpr_state.rcx as u32;
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
				advance_rip();
			}
		};
	}

	fn handle_invalid_state(&mut self,_gpr_state:&mut GprState)
	{
		self.dump_current_vmcs();
		panic!("Invalid Guest State!");
	}

	fn handle_invalid_auto_msr(&mut self,_gpr_state:&mut GprState)
	{
		panic!("Invalid Auto-MSR List!");
	}

	fn handle_ept_violation(&mut self,_gpr_state:&mut GprState)
	{
		let gpa=unsafe{vmread64(GUEST_PHYSICAL_ADDRESS)}.unwrap();
		println!("EPT Violation happened! GPA=0x{gpa:X}");
	}

	fn handle_ept_misconfig(&mut self,_gpr_state:&mut GprState)
	{
		let gpa=unsafe{vmread64(GUEST_PHYSICAL_ADDRESS)}.unwrap();
		println!("EPT Misconfiguration happened! GPA=0x{gpa:X}");
	}

	fn handle_xsetbv(&mut self,gpr_state:&mut GprState)
	{
		let index=(gpr_state.rcx&0xFFFFFFFF) as u32;
		let value=(gpr_state.rax&0xFFFFFFFF)|(gpr_state.rdx&0xFFFFFFFF00000000);
		println!("The xsetbv instruction is intercepted! Index={index}, Value=0x{value:16X}");
		unsafe
		{
			_xsetbv(index,value);
			advance_rip();
		}
	}

	fn handle_unknown(&mut self,_gpr_state:&mut GprState)
	{
		let exit_reason=unsafe{vmread32(VMEXIT_REASON).unwrap()};
		panic!("Unknown VM-Exit is intercepted! Exit-Reason: {} (0x{exit_reason:X})",exit_reason&0xFFFF);
	}
}

#[unsafe(no_mangle)] unsafe extern "C" fn nvc_vt_exit_handler(gpr_state:*mut GprState,vcpu:*mut VtVcpu,guest_state:*mut InterruptStackFrameWithErrorCode)
{
	unsafe
	{
		(*guest_state).return_rsp=vmreadptr(GUEST_RSP).unwrap() as u64;
		(*guest_state).return_rip=vmreadptr(GUEST_RIP).unwrap() as u64;
		let exit_reason=vmread32(VMEXIT_REASON).unwrap();
		let vp=&mut (*vcpu);
		let gpr=&mut (*gpr_state);
		gpr.rsp=(*guest_state).return_rsp;
		let handler=dispatch_handler(exit_reason);
		handler(vp,gpr);
	}
}

#[unsafe(no_mangle)] unsafe extern "C" fn nvc_vt_resume_failure(_gpr_state:*mut GprState,_vcpu:*mut VtVcpu,_vmx_status:u8)
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

#[inline] fn dispatch_handler(exit_reason:u32)->VtExitHandler
{
	match VT_EXIT_HANDLERS.get((exit_reason&0xFFFF)as usize)
	{
		Some(f)=>*f,
		None=>VtVcpu::handle_unknown
	}
}