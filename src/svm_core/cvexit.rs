/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file contains the CVM VM-Exit handlers for AMD-V.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::hint::cold_path;

use log::{error, trace};
use nvcvm::interface::{CvmMemoryAccessFlags, InterceptCode};

use crate::{cvm_core::x86::cpuid::dispatch_cpuid_handler, disasm::emulator::EmulatorOps, svm_core::{SvmVcpu, VmcbOps, custom::SvmCustomVcpu, exit::*, cvmsr::dispatch_msr_handler, npt::NptFaultCode, vmcb::{CrInterceptInformation, IoInterceptInformation}}, xpf_core::{asm::misc::wbinvd, x86::{crdr::Cr4, interrupts::{EventType, GENERAL_PROTECTION_FAULT, INVALID_OPCODE_FAULT}}}};

impl SvmCustomVcpu
{
	fn handle_unknown(&mut self,host_vcpu:&mut SvmVcpu)
	{
		error!("Unknown VM-Exit reason (0x{:X}) happened in CVM Guest!",self.read_exit_code());
		unsafe
		{
			(*self.vpcb).intercept_code=InterceptCode::SCHEDULER_BUG;
		}
		host_vcpu.switch_world_to_host(self);
	}

	fn handle_unknown_instruction(&mut self,_host_vcpu:&mut SvmVcpu)
	{
		// Emulation for the instruction is not supported yet. Inject a #UD.
		error!("Unknown Instruction was intercepted! Interception-Code: 0x{:X}",self.read_exit_code());
		self.inject_event(INVALID_OPCODE_FAULT,EventType::HardwareException,None,true);
	}

	fn handle_invalid(&mut self,host_vcpu:&mut SvmVcpu)
	{
		error!("Invalid CVM Guest State!");
		unsafe
		{
			(*self.vpcb).intercept_code=InterceptCode::INVALID_STATE;
		}
		host_vcpu.switch_world_to_host(self);
	}

	fn handle_cr4_read(&mut self,host_vcpu:&mut SvmVcpu)
	{
		let info=CrInterceptInformation::from_bits(self.read_exit_info1());
		if info.is_mov_cr()
		{
			// The CR4's MCE bit must always be set and should be shadowed to the guest.
			self.set_gpr(info.gpr_index(),self.read_cr4().with_mce(self.shadow_bits.mce()).into_bits());
			self.advance_rip();
		}
		else
		{
			// This is virtually impossible to happen.
			cold_path();
			error!("The CR4 is not accessed via regular mov cr instruction!");
			unsafe
			{
				(*self.vpcb).intercept_code=InterceptCode::SCHEDULER_BUG;
			}
			host_vcpu.switch_world_to_host(self);
		}
	}

	fn handle_cr4_write(&mut self,host_vcpu:&mut SvmVcpu)
	{
		let info=CrInterceptInformation::from_bits(self.read_exit_info1());
		if info.is_mov_cr()
		{
			// The CR4's MCE bit must always be set and should be shadowed to the guest.
			let new_cr4=Cr4::from_bits(self.get_gpr(info.gpr_index()));
			self.shadow_bits.set_mce(new_cr4.mce());
			self.write_cr4(new_cr4.with_mce(true));
			self.advance_rip();
		}
		else
		{
			// This is virtually impossible to happen.
			cold_path();
			error!("The CR4 is not accessed via regular mov cr instruction!");
			unsafe
			{
				(*self.vpcb).intercept_code=InterceptCode::SCHEDULER_BUG;
			}
			host_vcpu.switch_world_to_host(self);
		}
	}

	fn handle_physintr(&mut self,host_vcpu:&mut SvmVcpu)
	{
		unsafe
		{
			(*self.vpcb).intercept_code=InterceptCode::SCHEDULER_EXIT;
		}
		host_vcpu.switch_world_to_host(self);
	}

	fn handle_nmi(&mut self,host_vcpu:&mut SvmVcpu)
	{
		unsafe
		{
			(*self.vpcb).intercept_code=InterceptCode::SCHEDULER_EXIT;
		}
		host_vcpu.switch_world_to_host(self);
	}

	fn handle_smi(&mut self,host_vcpu:&mut SvmVcpu)
	{
		unsafe
		{
			(*self.vpcb).intercept_code=InterceptCode::SCHEDULER_EXIT;
		}
		host_vcpu.switch_world_to_host(self);
	}
	
	fn handle_cpuid(&mut self,_host_vcpu:&mut SvmVcpu)
	{
		// Dispatch it to the shared cpuid handler.
		let handler_fn=dispatch_cpuid_handler(self.read_rax() as u32);
		handler_fn(self);
		// The cpuid instruction never throws exception.
		// Note: while cpuid could be disabled for user mode by HWCR, we do not support HWCR yet.
		self.advance_rip();
	}

	fn handle_invd(&mut self,_host_vcpu:&mut SvmVcpu)
	{
		// The invd instruction would corrupt global cache and it must thereby be intercepted.
		// Execute wbinvd to protect global cache.
		wbinvd();
		self.advance_rip();
	}

	fn handle_hlt(&mut self,host_vcpu:&mut SvmVcpu)
	{
		let vpcb=unsafe{&mut *self.vpcb};
		vpcb.intercept_code=InterceptCode::HLT_INSTRUCTION;
		vpcb.exit_context.next_rip=self.read_next_rip();
		host_vcpu.switch_world_to_host(self);
	}

	fn handle_io(&mut self,host_vcpu:&mut SvmVcpu)
	{
		// For I/O instructions, it needs to be delivered to the user hypervisor.
		let vpcb=unsafe{&mut *self.vpcb};
		vpcb.intercept_code=InterceptCode::IO_INSTRUCTION;
		vpcb.exit_context.next_rip=self.read_next_rip();
		// Save the exit information for user hypervisor to handle.
		let ctxt=unsafe{&mut vpcb.exit_context.context.io};
		let info=IoInterceptInformation::from_bits(self.read_exit_info1());
		ctxt.set_io_type(info.r#type());
		ctxt.set_string(info.string());
		ctxt.set_repeat(info.repeat());
		ctxt.set_operand_size(info.op_size());
		ctxt.set_address_width(info.addr_width()<<1);
		ctxt.set_port(info.port());
		// Switch the world to host in order to handle the I/O instruction.
		host_vcpu.switch_world_to_host(self);
	}

	fn handle_msr(&mut self,_host_vcpu:&mut SvmVcpu)
	{
		// Dispatch it to the shared MSR handler.
		let index=self.state.gpr.rcx as u32;
		match dispatch_msr_handler(index)
		{
			Some(handler_fn)=>
			{
				let write=self.read_exit_info1()!=0;
				if handler_fn(self,write)
				{
					// No exceptions happened. Advance the rip.
					self.advance_rip();
				}
				else
				{
					// Exception happened. Throw it into the guest.
					cold_path();
					self.inject_event(GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true);
				}
			}
			None=>
			{
				cold_path();
				error!("MSR 0x{index:X} is not supported! Throwing #GP(0)...");
				self.inject_event(GENERAL_PROTECTION_FAULT,EventType::HardwareException,Some(0),true);
			}
		}
	}

	fn handle_shutdown(&mut self,host_vcpu:&mut SvmVcpu)
	{
		error!("CVM Guest triggered a triple-fault!");
		unsafe
		{
			(*self.vpcb).intercept_code=InterceptCode::SHUTDOWN_CONDITION;
		}
		host_vcpu.switch_world_to_host(self);
	}
	
	fn handle_npf(&mut self,host_vcpu:&mut SvmVcpu)
	{
		let err_code=NptFaultCode::from_bits(self.read_exit_info1());
		let gpa=self.read_exit_info2();
		let rip=self.read_rip();
		trace!("CVM Guest triggered an #NPF! rip=0x{rip:X}, gpa=0x{gpa:X}, reason: {err_code}");
		let vpcb=unsafe{&mut *self.vpcb};
		vpcb.intercept_code=InterceptCode::MEMORY_ACCESS;
		vpcb.exit_context.next_rip=0;
		let ctxt=unsafe{&mut vpcb.exit_context.context.mem};
		// Copy memory-access information.
		ctxt.access.set_present(err_code.present());
		ctxt.access.set_write(err_code.write());
		ctxt.access.set_execute(err_code.code_fetch());
		ctxt.access.set_guest_pt(err_code.translate_page_table());
		// Copy instruction bytes.
		let bytes_count=self.read_fetched_bytes_count() as usize;
		ctxt.access.set_fetched_bytes(bytes_count);
		ctxt.instruction_bytes[..bytes_count].copy_from_slice(&self.ref_fetched_bytes()[..bytes_count]);
		// GPA of #NPF.
		ctxt.gpa=gpa;
		// Decode instruction. (Not implemented yet)
		ctxt.flags=CvmMemoryAccessFlags::new();
		// Switch the world to host to handle it.
		host_vcpu.switch_world_to_host(self);
	}
}

pub(super) type SvmCvExitHandler=fn(cvcpu:&mut SvmCustomVcpu,host_vcpu:&mut SvmVcpu);

pub(super) static SVM_CVEXIT_HANDLER_GROUP1:[SvmCvExitHandler;SVM_MAXIMUM_CODE1]=
{
	let mut x:[SvmCvExitHandler;SVM_MAXIMUM_CODE1]=[SvmCustomVcpu::handle_unknown;SVM_MAXIMUM_CODE1];
	const fn set(array:&mut [SvmCvExitHandler],code:i64,f:SvmCvExitHandler)
	{
		array[code as usize]=f;
	}
	set(&mut x,INTERCEPTED_CR4_READ,SvmCustomVcpu::handle_cr4_read);
	set(&mut x,INTERCEPTED_CR4_WRITE,SvmCustomVcpu::handle_cr4_write);
	set(&mut x,INTERCEPTED_INTERRUPT,SvmCustomVcpu::handle_physintr);
	set(&mut x,INTERCEPTED_NMI,SvmCustomVcpu::handle_nmi);
	set(&mut x,INTERCEPTED_SMI,SvmCustomVcpu::handle_smi);
	set(&mut x,INTERCEPTED_CPUID,SvmCustomVcpu::handle_cpuid);
	set(&mut x,INTERCEPTED_RSM,SvmCustomVcpu::handle_unknown_instruction);
	set(&mut x,INTERCEPTED_INVD,SvmCustomVcpu::handle_invd);
	set(&mut x,INTERCEPTED_HLT,SvmCustomVcpu::handle_hlt);
	set(&mut x,INTERCEPTED_INVLPGA,SvmCustomVcpu::handle_unknown_instruction);
	set(&mut x,INTERCEPTED_IO,SvmCustomVcpu::handle_io);
	set(&mut x,INTERCEPTED_MSR,SvmCustomVcpu::handle_msr);
	set(&mut x,INTERCEPTED_SHUTDOWN,SvmCustomVcpu::handle_shutdown);
	set(&mut x,INTERCEPTED_VMRUN,SvmCustomVcpu::handle_unknown_instruction);
	set(&mut x,INTERCEPTED_VMMCALL,SvmCustomVcpu::handle_unknown_instruction);
	set(&mut x,INTERCEPTED_VMLOAD,SvmCustomVcpu::handle_unknown_instruction);
	set(&mut x,INTERCEPTED_VMSAVE,SvmCustomVcpu::handle_unknown_instruction);
	set(&mut x,INTERCEPTED_STGI,SvmCustomVcpu::handle_unknown_instruction);
	set(&mut x,INTERCEPTED_CLGI,SvmCustomVcpu::handle_unknown_instruction);
	set(&mut x,INTERCEPTED_SKINIT,SvmCustomVcpu::handle_unknown_instruction);
	set(&mut x,INTERCEPTED_XSETBV,SvmCustomVcpu::handle_unknown_instruction);
	x
};

pub(super) static SVM_CVEXIT_HANDLER_GROUP2:[SvmCvExitHandler;SVM_MAXIMUM_CODE2]=
{
	let mut x:[SvmCvExitHandler;SVM_MAXIMUM_CODE2]=[SvmCustomVcpu::handle_unknown;SVM_MAXIMUM_CODE2];
	x[(NESTED_PAGE_FAULT-0x400) as usize]=SvmCustomVcpu::handle_npf;
	x
};

pub(super) static SVM_CVEXIT_HANDLER_NEGATIVE_GROUP:[SvmCvExitHandler;SVM_MAXIMUM_NEGATIVE]=
{
	let mut x:[SvmCvExitHandler;SVM_MAXIMUM_NEGATIVE]=[SvmCustomVcpu::handle_unknown;SVM_MAXIMUM_NEGATIVE];
	x[!INVALID_GUEST_STATE as usize]=SvmCustomVcpu::handle_invalid;
	x
};

pub(super) static SVM_CVEXIT_HANDLER_GROUPS:[&[SvmCvExitHandler];SVM_MAXIMUM_NEGATIVE]=
[
	&SVM_CVEXIT_HANDLER_GROUP1,
	&SVM_CVEXIT_HANDLER_GROUP2,
	&[],&[]
];

#[inline] pub(super) fn dispatch_cvexit_handler(code:i64)->SvmCvExitHandler
{
	if code<0
	{
		cold_path();
		let index=!code as usize;
		SVM_CVEXIT_HANDLER_NEGATIVE_GROUP.get(index).copied().unwrap_or(SvmCustomVcpu::handle_unknown)
	}
	else
	{
		let group:usize=(code as usize)>>10;
		match SVM_CVEXIT_HANDLER_GROUPS.get(group)
		{
			Some(&g)=>
			{
				let index:usize=(code as usize)&0x3FF;
				g.get(index).copied().unwrap_or(SvmCustomVcpu::handle_unknown)
			}
			None=>SvmCustomVcpu::handle_unknown
		}
	}
}