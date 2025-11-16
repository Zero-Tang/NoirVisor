/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file handles Hypercalls in AMD-V of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::ffi::c_void;

use log::warn;
use nvcvm::status::Status;

use crate::xpf_core::x86::{descriptors::DescriptorTable, interrupts::*, paging::PageTranslationHelper};
use super::*;

impl SvmVcpu
{
	fn hvcall_unknown(&mut self,code:u32,context:*mut c_void)->Result<Status,(u8,Option<u32>)>
	{
		warn!("Unknown Hypercall Code 0x{code:X} is called! Context={context:p}");
		Err((INVALID_OPCODE_FAULT,None))
	}

	fn hvcall_restore(&mut self,_code:u32,_context:*mut c_void)->Result<Status,(u8,Option<u32>)>
	{
		let gcr3:u64=unsafe{vmread(self.vmcb.virt,GUEST_CR3)};
		let nrip:u64=unsafe{vmread(self.vmcb.virt,NEXT_RIP)};
		let gflags:u64=unsafe{vmread(self.vmcb.virt,GUEST_RFLAGS)};
		let gpr_state=&mut self.get_stack_top_mut().gpr_state;
		let saved_state:GprState=GprState
		{
			rax:nrip,
			rcx:gflags,
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
		let gcr4:u64=unsafe{vmread(self.vmcb.virt,GUEST_CR4)};
		write_cr3(gcr3);
		write_cr4(gcr4);
		// Restore the processor's hidden state.
		vmload(self.vmcb.phys);
		unsafe
		{
			// Switch to Restored IDT.
			let gidtr:DescriptorTable=DescriptorTable
			{
				limit:vmread(self.vmcb.virt,GUEST_IDTR_LIMIT),
				base:vmread(self.vmcb.virt,GUEST_IDTR_BASE)
			};
			write_idtr(&raw const gidtr);
			// Switch to Restored GDT.
			let ggdtr:DescriptorTable=DescriptorTable
			{
				limit:vmread(self.vmcb.virt,GUEST_GDTR_LIMIT),
				base:vmread(self.vmcb.virt,GUEST_GDTR_BASE)
			};
			write_gdtr(&raw const ggdtr);
			// Note that TSS is switched in previous vmload.
		}
		// Set the GIF. Otherwise the host will never be interrupted.
		stgi();
		// Return to the caller in Host Mode.
		unsafe
		{
			nvc_svm_return(&raw const saved_state);
		}
		// Never reaches here!
	}

	fn hvcall_alloc_tlb_tag(&mut self,_code:u32,context:*mut c_void)->Result<Status,(u8,Option<u32>)>
	{
		let hv:&mut SvmHypervisor=unsafe{&mut *self.hypervisor.cast()};
		match hv.alloc_asid()
		{
			Some(asid)=>
			{
				let buf=asid.to_le_bytes();
				let mut fault_va:Option<u64>=None;
				match self.write_virt(context as u64,&buf,&mut fault_va)
				{
					Ok(_)=>Ok(Status::SUCCESS),
					Err(e)=>
					{
						error!("Failed to write ASID back! Error-Code: {e}, Linear-Address: 0x{:X}",fault_va.unwrap());
						// Free this ASID as we can't write it back.
						hv.free_asid(asid);
						unsafe
						{
							// Inject #PF.
							vmwrite(self.vmcb.virt,GUEST_CR2,fault_va.unwrap());
							vmcb_clean_cr2(self.vmcb.virt);
						}
						Err((PAGE_FAULT,Some(e.into_bits())))
					}
				}
			}
			None=>Ok(Status::INSUFFICIENT_RESOURCES)
		}
	}

	fn hvcall_exit_boot_services(&mut self,_code:u32,_context:*mut c_void)->Result<Status,(u8,Option<u32>)>
	{
		// If NoirVisor is loaded as a UEFI runtime-driver, this routine will be called by Guest OS.
		// Current implementation just outputs a log and returns.
		info!("ExitBootServices event is triggered! UEFI now enters Runtime Stage!");
		Ok(Status::SUCCESS)
	}
}

pub(super) type SvmHypercallHandler=fn(&mut SvmVcpu,code:u32,context:*mut c_void)->Result<Status,(u8,Option<u32>)>;

static SVM_HYPERCALL_HANDLER_BASE:[SvmHypercallHandler;4]=
[
	SvmVcpu::hvcall_unknown,
	SvmVcpu::hvcall_restore,
	SvmVcpu::hvcall_alloc_tlb_tag,
	SvmVcpu::hvcall_exit_boot_services
];

static SVM_HYPERCALL_HANDLER_CVM:[SvmHypercallHandler;6]=
[
	
	SvmVcpu::hvcall_unknown,
	SvmVcpu::hvcall_unknown,
	SvmVcpu::hvcall_unknown,
	SvmVcpu::hvcall_unknown,
	SvmVcpu::hvcall_unknown,
	SvmVcpu::hvcall_unknown
];

static SVM_HYPERCALL_HANDLER_GROUPS:[&[SvmHypercallHandler];2]=[&SVM_HYPERCALL_HANDLER_BASE,&SVM_HYPERCALL_HANDLER_CVM];

#[inline] pub(super) fn dispatch_hypercall(code:u32)->SvmHypercallHandler
{
	let group=(code>>16) as usize;
	match SVM_HYPERCALL_HANDLER_GROUPS.get(group)
	{
		Some(&g)=>
		{
			let index=(code&0xffff) as usize;
			match g.get(index)
			{
				Some(&h)=>h,
				None=>SvmVcpu::hvcall_unknown
			}
		}
		None=>SvmVcpu::hvcall_unknown
	}
}