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

use crate::svm_core::{SvmVcpu, VmcbOps, custom::SvmCustomVcpu, exit::*, npt::NptFaultCode};

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

	fn handle_invalid(&mut self,host_vcpu:&mut SvmVcpu)
	{
		error!("Invalid CVM Guest State!");
		unsafe
		{
			(*self.vpcb).intercept_code=InterceptCode::INVALID_STATE;
		}
		host_vcpu.switch_world_to_host(self);
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

	fn handle_hlt(&mut self,host_vcpu:&mut SvmVcpu)
	{
		let vpcb=unsafe{&mut *self.vpcb};
		vpcb.intercept_code=InterceptCode::HLT_INSTRUCTION;
		vpcb.exit_context.next_rip=self.read_next_rip();
		host_vcpu.switch_world_to_host(self);
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
	set(&mut x,INTERCEPTED_INTERRUPT,SvmCustomVcpu::handle_physintr);
	set(&mut x,INTERCEPTED_NMI,SvmCustomVcpu::handle_nmi);
	set(&mut x,INTERCEPTED_SMI,SvmCustomVcpu::handle_smi);
	set(&mut x,INTERCEPTED_HLT,SvmCustomVcpu::handle_hlt);
	set(&mut x,INTERCEPTED_SHUTDOWN,SvmCustomVcpu::handle_shutdown);
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