/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file defines x86 CVM of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use alloc::alloc::AllocError;
use nvcvm::interface::{CVM_MAPPING_ASID_DEFAULT, CvmHandle, CvmMapping, ExitContext, ExitContextUnion, SegmentRegister, Vpcb, X64SyncFlags};

use crate::*;
use xpf_core::{nvbdk::*, x86::xstate::BoxedXState};

pub struct CvmX86Vcpu
{
	// Processor State
	pub gpr:GprState,
	pub seg:SegmentState,
	pub crs:CrState,
	pub drs:DrState,
	pub msrs:MsrState,
	pub xcrs:XcrState,
	pub rflags:u64,
	pub rip:u64,
	pub tsc:u64,
	pub xsaves_state:BoxedXState,
	pub current_as_id:u32,
	pub exit_context:ExitContext
}

impl CvmX86Vcpu
{
	pub fn new()->Result<Self,AllocError>
	{
		let xsave=BoxedXState::try_new(0x340)?;
		Ok
		(
			Self
			{
				gpr:GprState
				{
					rax:0,rcx:0,rdx:0,rbx:0,rsp:0,rbp:0,rsi:0,rdi:0,
					r8:0,r9:0,r10:0,r11:0,r12:0,r13:0,r14:0,r15:0
				},
				seg:SegmentState
				{
					es:SegmentRegister::reset_data(),
					cs:SegmentRegister::reset_code(),
					ss:SegmentRegister::reset_data(),
					ds:SegmentRegister::reset_data(),
					fs:SegmentRegister::reset_data(),
					gs:SegmentRegister::reset_data(),
					tr:SegmentRegister::reset_tss(),
					gdtr:SegmentRegister::reset_dt(),
					idtr:SegmentRegister::reset_dt(),
					ldtr:SegmentRegister::reset_ldt()
				},
				crs:CrState
				{
					cr0:0x60000010,
					cr2:0,cr3:0,cr4:0,cr8:0
				},
				drs:DrState
				{
					dr0:0,dr1:0,dr2:0,dr3:0,
					dr6:0xFFFF0FF0,dr7:0x400
				},
				msrs:MsrState
				{
					sysenter_cs:0,sysenter_esp:0,sysenter_eip:0,
					pat:0,
					efer:0,
					star:0,lstar:0,cstar:0,sfmask:0,ststar:0,gsswap:0,
					debug_ctrl:0
				},
				xcrs:XcrState{xcr0:0},
				rflags:2,
				rip:0xFFF0,
				tsc:0,
				xsaves_state:xsave,
				current_as_id:CVM_MAPPING_ASID_DEFAULT,
				exit_context:ExitContext
				{
					next_rip:0,
					context:ExitContextUnion::default()
				}
			}
		)
	}

	pub fn sync_from_vpcb(&mut self,vpcb:&mut Vpcb)
	{
		if vpcb.sync_flags.gpr()
		{
			self.gpr.rax=vpcb.sync_regs.rax;
			self.gpr.rcx=vpcb.sync_regs.rcx;
			self.gpr.rdx=vpcb.sync_regs.rdx;
			self.gpr.rbx=vpcb.sync_regs.rbx;
			self.gpr.rsp=vpcb.sync_regs.rsp;
			self.gpr.rbp=vpcb.sync_regs.rbp;
			self.gpr.rsi=vpcb.sync_regs.rsi;
			self.gpr.rdi=vpcb.sync_regs.rdi;
			self.gpr.r8=vpcb.sync_regs.r8;
			self.gpr.r9=vpcb.sync_regs.r9;
			self.gpr.r10=vpcb.sync_regs.r10;
			self.gpr.r11=vpcb.sync_regs.r11;
			self.gpr.r12=vpcb.sync_regs.r12;
			self.gpr.r13=vpcb.sync_regs.r13;
			self.gpr.r14=vpcb.sync_regs.r14;
			self.gpr.r15=vpcb.sync_regs.r15;
			self.rflags=vpcb.sync_regs.rflags;
			self.rip=vpcb.sync_regs.rip;
			trace!("new rip: 0x{:X}",self.rip);
		}
		if vpcb.sync_flags.seg()
		{
			self.seg.es.selector=vpcb.sync_regs.es.selector;
			self.seg.es.attrib=vpcb.sync_regs.es.attrib;
			self.seg.es.limit=vpcb.sync_regs.es.limit;
			self.seg.es.base=vpcb.sync_regs.es.base;
			self.seg.cs.selector=vpcb.sync_regs.cs.selector;
			self.seg.cs.attrib=vpcb.sync_regs.cs.attrib;
			self.seg.cs.limit=vpcb.sync_regs.cs.limit;
			self.seg.cs.base=vpcb.sync_regs.cs.base;
			self.seg.ss.selector=vpcb.sync_regs.ss.selector;
			self.seg.ss.attrib=vpcb.sync_regs.ss.attrib;
			self.seg.ss.limit=vpcb.sync_regs.ss.limit;
			self.seg.ss.base=vpcb.sync_regs.ss.base;
			self.seg.ds.selector=vpcb.sync_regs.ds.selector;
			self.seg.ds.attrib=vpcb.sync_regs.ds.attrib;
			self.seg.ds.limit=vpcb.sync_regs.ds.limit;
			self.seg.ds.base=vpcb.sync_regs.ds.base;
		}
		if vpcb.sync_flags.fg()
		{
			self.seg.fs.selector=vpcb.sync_regs.fs.selector;
			self.seg.fs.attrib=vpcb.sync_regs.fs.attrib;
			self.seg.fs.limit=vpcb.sync_regs.fs.limit;
			self.seg.fs.base=vpcb.sync_regs.fs.base;
			self.seg.gs.selector=vpcb.sync_regs.gs.selector;
			self.seg.gs.attrib=vpcb.sync_regs.gs.attrib;
			self.seg.gs.limit=vpcb.sync_regs.gs.limit;
			self.seg.gs.base=vpcb.sync_regs.gs.base;
			self.msrs.gsswap=vpcb.sync_regs.kgsbase;
		}
		if vpcb.sync_flags.dt()
		{
			self.seg.gdtr.selector=vpcb.sync_regs.gdtr.selector;
			self.seg.gdtr.attrib=vpcb.sync_regs.gdtr.attrib;
			self.seg.gdtr.limit=vpcb.sync_regs.gdtr.limit;
			self.seg.gdtr.base=vpcb.sync_regs.gdtr.base;
			self.seg.idtr.selector=vpcb.sync_regs.idtr.selector;
			self.seg.idtr.attrib=vpcb.sync_regs.idtr.attrib;
			self.seg.idtr.limit=vpcb.sync_regs.idtr.limit;
			self.seg.idtr.base=vpcb.sync_regs.idtr.base;
		}
		if vpcb.sync_flags.lt()
		{
			self.seg.ldtr.selector=vpcb.sync_regs.ldtr.selector;
			self.seg.ldtr.attrib=vpcb.sync_regs.ldtr.attrib;
			self.seg.ldtr.limit=vpcb.sync_regs.ldtr.limit;
			self.seg.ldtr.base=vpcb.sync_regs.ldtr.base;
			self.seg.tr.selector=vpcb.sync_regs.tr.selector;
			self.seg.tr.attrib=vpcb.sync_regs.tr.attrib;
			self.seg.tr.limit=vpcb.sync_regs.tr.limit;
			self.seg.tr.base=vpcb.sync_regs.tr.base;
		}
		if vpcb.sync_flags.cr()
		{
			self.crs.cr0=vpcb.sync_regs.cr0;
			self.crs.cr2=vpcb.sync_regs.cr2;
			self.crs.cr3=vpcb.sync_regs.cr3;
			self.crs.cr4=vpcb.sync_regs.cr4;
			self.crs.cr8=vpcb.sync_regs.cr8;
			self.msrs.efer=vpcb.sync_regs.efer;
		}
		if vpcb.sync_flags.dr()
		{
			self.drs.dr0=vpcb.sync_regs.dr0;
			self.drs.dr1=vpcb.sync_regs.dr1;
			self.drs.dr2=vpcb.sync_regs.dr2;
			self.drs.dr3=vpcb.sync_regs.dr3;
			self.drs.dr6=vpcb.sync_regs.dr6;
			self.drs.dr7=vpcb.sync_regs.dr7;
		}
		if vpcb.sync_flags.sc()
		{
			self.msrs.star=vpcb.sync_regs.star;
			self.msrs.lstar=vpcb.sync_regs.lstar;
			self.msrs.cstar=vpcb.sync_regs.cstar;
			self.msrs.sfmask=vpcb.sync_regs.sfmask;
			self.msrs.ststar=vpcb.sync_regs.ststar;
		}
		if vpcb.sync_flags.se()
		{
			// The ABI in X64SyncReg does not encode sysenter CS/ESP/EIP, so this group is left as-is.
			self.msrs.sysenter_cs=vpcb.sync_regs.sysenter_cs;
			self.msrs.sysenter_esp=vpcb.sync_regs.sysenter_esp;
			self.msrs.sysenter_eip=vpcb.sync_regs.sysenter_eip;
		}
		vpcb.sync_flags=X64SyncFlags::new();
	}

	pub fn sync_to_vpcb(&self,vpcb:&mut Vpcb)
	{
		vpcb.sync_regs.rax=self.gpr.rax;
		vpcb.sync_regs.rcx=self.gpr.rcx;
		vpcb.sync_regs.rdx=self.gpr.rdx;
		vpcb.sync_regs.rbx=self.gpr.rbx;
		vpcb.sync_regs.rsp=self.gpr.rsp;
		vpcb.sync_regs.rbp=self.gpr.rbp;
		vpcb.sync_regs.rsi=self.gpr.rsi;
		vpcb.sync_regs.rdi=self.gpr.rdi;
		vpcb.sync_regs.r8=self.gpr.r8;
		vpcb.sync_regs.r9=self.gpr.r9;
		vpcb.sync_regs.r10=self.gpr.r10;
		vpcb.sync_regs.r11=self.gpr.r11;
		vpcb.sync_regs.r12=self.gpr.r12;
		vpcb.sync_regs.r13=self.gpr.r13;
		vpcb.sync_regs.r14=self.gpr.r14;
		vpcb.sync_regs.r15=self.gpr.r15;
		vpcb.sync_regs.rflags=self.rflags;
		vpcb.sync_regs.rip=self.rip;

		vpcb.sync_regs.es.selector=self.seg.es.selector;
		vpcb.sync_regs.es.attrib=self.seg.es.attrib;
		vpcb.sync_regs.es.limit=self.seg.es.limit;
		vpcb.sync_regs.es.base=self.seg.es.base;
		vpcb.sync_regs.cs.selector=self.seg.cs.selector;
		vpcb.sync_regs.cs.attrib=self.seg.cs.attrib;
		vpcb.sync_regs.cs.limit=self.seg.cs.limit;
		vpcb.sync_regs.cs.base=self.seg.cs.base;
		vpcb.sync_regs.ss.selector=self.seg.ss.selector;
		vpcb.sync_regs.ss.attrib=self.seg.ss.attrib;
		vpcb.sync_regs.ss.limit=self.seg.ss.limit;
		vpcb.sync_regs.ss.base=self.seg.ss.base;
		vpcb.sync_regs.ds.selector=self.seg.ds.selector;
		vpcb.sync_regs.ds.attrib=self.seg.ds.attrib;
		vpcb.sync_regs.ds.limit=self.seg.ds.limit;
		vpcb.sync_regs.ds.base=self.seg.ds.base;

		vpcb.sync_regs.fs.selector=self.seg.fs.selector;
		vpcb.sync_regs.fs.attrib=self.seg.fs.attrib;
		vpcb.sync_regs.fs.limit=self.seg.fs.limit;
		vpcb.sync_regs.fs.base=self.seg.fs.base;
		vpcb.sync_regs.gs.selector=self.seg.gs.selector;
		vpcb.sync_regs.gs.attrib=self.seg.gs.attrib;
		vpcb.sync_regs.gs.limit=self.seg.gs.limit;
		vpcb.sync_regs.gs.base=self.seg.gs.base;
		vpcb.sync_regs.kgsbase=self.msrs.gsswap;

		vpcb.sync_regs.gdtr.selector=self.seg.gdtr.selector;
		vpcb.sync_regs.gdtr.attrib=self.seg.gdtr.attrib;
		vpcb.sync_regs.gdtr.limit=self.seg.gdtr.limit;
		vpcb.sync_regs.gdtr.base=self.seg.gdtr.base;
		vpcb.sync_regs.idtr.selector=self.seg.idtr.selector;
		vpcb.sync_regs.idtr.attrib=self.seg.idtr.attrib;
		vpcb.sync_regs.idtr.limit=self.seg.idtr.limit;
		vpcb.sync_regs.idtr.base=self.seg.idtr.base;

		vpcb.sync_regs.ldtr.selector=self.seg.ldtr.selector;
		vpcb.sync_regs.ldtr.attrib=self.seg.ldtr.attrib;
		vpcb.sync_regs.ldtr.limit=self.seg.ldtr.limit;
		vpcb.sync_regs.ldtr.base=self.seg.ldtr.base;
		vpcb.sync_regs.tr.selector=self.seg.tr.selector;
		vpcb.sync_regs.tr.attrib=self.seg.tr.attrib;
		vpcb.sync_regs.tr.limit=self.seg.tr.limit;
		vpcb.sync_regs.tr.base=self.seg.tr.base;

		vpcb.sync_regs.cr0=self.crs.cr0;
		vpcb.sync_regs.cr2=self.crs.cr2;
		vpcb.sync_regs.cr3=self.crs.cr3;
		vpcb.sync_regs.cr4=self.crs.cr4;
		vpcb.sync_regs.cr8=self.crs.cr8;
		vpcb.sync_regs.efer=self.msrs.efer;

		vpcb.sync_regs.dr0=self.drs.dr0;
		vpcb.sync_regs.dr1=self.drs.dr1;
		vpcb.sync_regs.dr2=self.drs.dr2;
		vpcb.sync_regs.dr3=self.drs.dr3;
		vpcb.sync_regs.dr6=self.drs.dr6;
		vpcb.sync_regs.dr7=self.drs.dr7;

		vpcb.sync_regs.star=self.msrs.star;
		vpcb.sync_regs.lstar=self.msrs.lstar;
		vpcb.sync_regs.cstar=self.msrs.cstar;
		vpcb.sync_regs.sfmask=self.msrs.sfmask;
		vpcb.sync_regs.ststar=self.msrs.ststar;

		vpcb.sync_regs.sysenter_cs=self.msrs.sysenter_cs;
		vpcb.sync_regs.sysenter_esp=self.msrs.sysenter_esp;
		vpcb.sync_regs.sysenter_eip=self.msrs.sysenter_eip;
	}
}

pub trait CvmHvOps:Send+Sync
{
	fn create_vm(&self)->Result<CvmHandle,Status>;
	fn delete_vm(&self,vm:CvmHandle)->Result<(),Status>;
	fn create_vcpu(&self,vm:CvmHandle,vcpu_id:u32,vpcb_hpa:u64)->Result<(),Status>;
	fn delete_vcpu(&self,vm:CvmHandle,vcpu_id:u32)->Result<(),Status>;
	fn set_mapping(&self,vm:CvmHandle,info:&CvmMapping,hpa_list:&[u64])->Result<(),Status>;
	fn msrpm(&self)->u64;
	fn iopm(&self)->u64;
}

pub fn create_vm()->Result<CvmHandle,Status>
{
	let hv=HVM.get().unwrap();
	hv.create_vm()
}

pub fn delete_vm(vm:CvmHandle)->Result<(),Status>
{
	let hv=HVM.get().unwrap();
	hv.delete_vm(vm)
}

pub fn create_vcpu(vm:CvmHandle,vcpu_id:u32,vpcb_hpa:u64)->Result<(),Status>
{
	let hv=HVM.get().unwrap();
	hv.create_vcpu(vm,vcpu_id,vpcb_hpa)
}

pub fn delete_vcpu(vm:CvmHandle,vcpu_id:u32)->Result<(),Status>
{
	let hv=HVM.get().unwrap();
	hv.delete_vcpu(vm,vcpu_id)
}

pub fn set_mapping(vm:CvmHandle,info:&CvmMapping,hpa_list:&[u64])->Result<(),Status>
{
	let hv=HVM.get().unwrap();
	hv.set_mapping(vm,info,hpa_list)
}