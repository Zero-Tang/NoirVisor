/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file defines assembly-based utilities of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

#![allow(dead_code)]

pub mod io
{
	// FIXME: Use a macro to reduce effort.
	// Note that the string-based I/O will never be implemented, in that x86-S will remove them.
	use core::arch::asm;
	
	pub unsafe fn in_byte(port:u16)->u8
	{
		let v:u8;
		asm!
		(
			"in al,dx",
			in("dx") port,
			out("al") v
		);
		v
	}

	pub unsafe fn in_word(port:u16)->u16
	{
		let v:u16;
		asm!
		(
			"in ax,dx",
			in("dx") port,
			out("ax") v
		);
		v
	}

	pub unsafe fn in_dword(port:u16)->u32
	{
		let v:u32;
		asm!
		(
			"in eax,dx",
			in("dx") port,
			out("eax") v
		);
		v
	}

	pub unsafe fn out_byte(port:u16,val:u8)->()
	{
		asm!
		(
			"out dx,al",
			in("dx") port,
			in("al") val
		)
	}

	pub unsafe fn out_word(port:u16,val:u16)->()
	{
		asm!
		(
			"out dx,ax",
			in("dx") port,
			in("ax") val
		)
	}

	pub unsafe fn out_dword(port:u16,val:u32)->()
	{
		asm!
		(
			"out dx,eax",
			in("dx") port,
			in("eax") val
		)
	}
}

pub mod cpuid
{
	use core::arch::asm;
	
	pub fn cpuid(ia:u32,ic:u32,a:Option<&mut u32>,b:Option<&mut u32>,c:Option<&mut u32>,d:Option<&mut u32>)->()
	{
		let ca:u32;
		let cb:u32;
		let cc:u32;
		let cd:u32;
		unsafe
		{
			asm!
			(
				"push rbx",
				"cpuid",
				"mov r8,rbx",
				"pop rbx",
				in("eax") ia,
				in("ecx") ic,
				lateout("eax") ca,
				out("r8d") cb,
				lateout("ecx") cc,
				out("edx") cd
			);
		}
		if let Some(ra)=a
		{
			*ra=ca;
		}
		if let Some(rb)=b
		{
			*rb=cb;
		}
		if let Some(rc)=c
		{
			*rc=cc;
		}
		if let Some(rd)=d
		{
			*rd=cd;
		}
	}

	pub fn cpuid2(ia:u32,ic:u32)->(u32,u32,u32,u32)
	{
		let ca:u32;
		let cb:u32;
		let cc:u32;
		let cd:u32;
		unsafe
		{
			asm!
			(
				"push rbx",
				"cpuid",
				"mov r8,rbx",
				"pop rbx",
				in("eax") ia,
				in("ecx") ic,
				lateout("eax") ca,
				out("r8d") cb,
				lateout("ecx") cc,
				out("edx") cd
			);
		}
		(ca,cb,cc,cd)
	}
}

pub mod msr
{
	use core::arch::asm;

	/** # Read MSR
	 * Returns the Model-Specific Register value with specified `index`.
	 */
	pub fn rdmsr(index:u32)->u64
	{
		let lo:u32;
		let hi:u32;
		unsafe
		{
			asm!
			(
				"rdmsr",
				in("ecx") index,
				out("eax") lo,
				out("edx") hi
			);
		}
		(lo as u64)|((hi as u64)<<32)
	}

	/** # Write MSR
	 * Writes the Model-Specific Register specified in `index` with `value`.
	 */
	pub fn wrmsr(index:u32,value:u64)->()
	{
		let lo:u32=(value&0xffffffff) as u32;
		let hi:u32=(value>>32) as u32;
		unsafe
		{
			asm!
			(
				"wrmsr",
				in("ecx") index,
				in("eax") lo,
				in("edx") hi
			);
		}
	}
}

pub mod svm
{
	use core::arch::asm;

	pub fn vmload(vmcb_phys:u64)->()
	{
		unsafe
		{
			asm!
			(
				"vmload",
				in("rax") vmcb_phys
			);
		}
	}

	pub fn vmsave(vmcb_phys:u64)->()
	{
		unsafe
		{
			asm!
			(
				"vmsave",
				in("rax") vmcb_phys
			);
		}
	}

	pub fn stgi()->()
	{
		unsafe
		{
			asm!("stgi");
		}
	}

	pub fn clgi()->()
	{
		unsafe 
		{
			asm!("clgi");
		}
	}

	pub fn invlpga(ptr:u64,asid:u32)->()
	{
		unsafe
		{
			asm!
			(
				"invlpga rax,ecx",
				in("rax") ptr,
				in("ecx") asid
			);
		}
	}
}