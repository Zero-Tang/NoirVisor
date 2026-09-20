/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file handles cpuid instructions for NoirVisor CVM Guests.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::cmp::Ordering;

use paste::paste;

use crate::xpf_core::x86::msr::{Efer, MSR_EFER, MSR_KERNEL_GS_BASE, MSR_PAT, MSR_SYSENTER_CS};
use super::{custom::SvmCustomVcpu, vmcb::VmcbOps};

struct MsrHandlerGroup
{
	start:u32,
	group:&'static [MsrHandler]
}

impl PartialEq<u32> for MsrHandlerGroup
{
	fn eq(&self,other:&u32)->bool
	{
		(self.start..self.start+(self.group.len() as u32)).contains(other)
	}
}

impl PartialOrd<u32> for MsrHandlerGroup
{
	fn partial_cmp(&self,other:&u32)->Option<Ordering>
	{
		if *other<self.start
		{
			Some(Ordering::Less)
		}
		else if *other>=(self.start+(self.group.len() as u32))
		{
			Some(Ordering::Greater)
		}
		else
		{
			Some(Ordering::Equal)
		}
	}
}

// Reduce the effort for defining the MSRs that simply forwards accesses to the VMCB.
macro_rules! handle_straightforward_msr
{
	($name:tt)=>
	{
		paste!
		{
			fn [<handle_ $name>](&mut self,write:bool)->bool
			{
				if write
				{
					self.[<write_ $name>](self.emu_wrmsr());
				}
				else
				{
					self.emu_rdmsr(self.[<read_ $name>]());
				}
				true
			}
		}
	};
}

impl SvmCustomVcpu
{
	// 
	#[inline(always)] const fn emu_wrmsr(&self)->u64
	{
		(self.state.gpr.rax&0xFFFFFFFF)|(self.state.gpr.rdx<<32)
	}

	#[inline(always)] const fn emu_rdmsr(&mut self,val:u64)
	{
		self.state.gpr.rcx=val&0xFFFFFFFF;
		self.state.gpr.rdx=val>>32;
	}

	handle_straightforward_msr!(sysenter_cs);
	handle_straightforward_msr!(sysenter_esp);
	handle_straightforward_msr!(sysenter_eip);
	handle_straightforward_msr!(pat);

	fn handle_efer(&mut self,write:bool)->bool
	{
		// Check is EFER bits are valid.
		if write
		{
			const MASK:Efer=Efer::new().with_sce(true).with_lme(true).with_lma(true).with_nxe(true);
			let new_val=Efer::from_bits(self.emu_wrmsr());
			if (new_val.into_bits()&MASK.into_bits())!=new_val.into_bits()
			{
				// New EFER value is invalid. Throw #GP(0) back...
				false
			}
			else if new_val.lma() && !new_val.lme()
			{
				// LME must be set if LMA is set!
				false
			}
			else
			{
				self.shadow_bits.set_svme(new_val.svme());
				self.write_efer(new_val.with_svme(true));
				true
			}
		}
		else
		{
			self.emu_rdmsr(self.read_efer().with_svme(self.shadow_bits.svme()).into_bits());
			true
		}
	}

	handle_straightforward_msr!(star);
	handle_straightforward_msr!(lstar);
	handle_straightforward_msr!(cstar);
	handle_straightforward_msr!(sfmask);
	handle_straightforward_msr!(kgsbase);
}

type MsrHandler=fn(&mut SvmCustomVcpu,write:bool)->bool;

// Note: You must ensure the groups are sorted in ascending order in terms of the MSR indices.
static MSR_HANDLER_GROUPS:[MsrHandlerGroup;4]=
[
	MsrHandlerGroup
	{
		start:MSR_SYSENTER_CS,
		group:&
		[
			SvmCustomVcpu::handle_sysenter_cs,
			SvmCustomVcpu::handle_sysenter_esp,
			SvmCustomVcpu::handle_sysenter_eip
		]
	},
	MsrHandlerGroup
	{
		start:MSR_PAT,
		group:&[SvmCustomVcpu::handle_pat]
	},
	MsrHandlerGroup
	{
		start:MSR_EFER,
		group:&
		[
			SvmCustomVcpu::handle_efer,
			SvmCustomVcpu::handle_star,
			SvmCustomVcpu::handle_lstar,
			SvmCustomVcpu::handle_cstar,
			SvmCustomVcpu::handle_sfmask
		]
	},
	MsrHandlerGroup
	{
		start:MSR_KERNEL_GS_BASE,
		group:&[SvmCustomVcpu::handle_kgsbase]
	}
];

pub fn dispatch_msr_handler(index:u32)->Option<MsrHandler>
{
	// It is virtually impossible to dispatch the MSR handler in O(1) time in that they're very scattered.
	// However, even though the distribution is scattered, MSRs usually appear in groups.
	// As such, it is possible to binary-search by groups to reduce the number of binary-search iterations.
	// In other words, we perform a "dense" binary search, rather than a "sparse" binary search.
	match MSR_HANDLER_GROUPS.binary_search_by(|g| g.partial_cmp(&index).unwrap())
	{
		Ok(i)=>MSR_HANDLER_GROUPS[i].group.get((index-MSR_HANDLER_GROUPS[i].start) as usize).copied(),
		Err(_)=>None
	}
}