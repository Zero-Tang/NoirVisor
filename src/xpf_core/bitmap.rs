/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file operates bitmaps in NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{ffi::c_void,arch::asm};
#[cfg(target_arch="x86_64")]
use core::arch::x86_64::{_bittest64,_bittestandcomplement64,_bittestandreset64,_bittestandset64};

pub struct Bitmap<const N:usize>;

impl<'a,const N:usize> Bitmap<N>
{
	/// ## `from_raw_parts` method
	/// `ptr` specifies the pointer to the bitmap base.
	/// 
	/// ## Safety
	/// You should guarantee `ptr` is at least aligned to pointer granularity. \
	/// Otherwise, panic may happen during bitmap operations.
	pub const unsafe fn from_raw_parts(ptr:*const c_void)->&'a Self
	{
		unsafe
		{
			&*ptr.cast()
		}
	}

	/// ## `from_raw_parts_mut` method
	/// `ptr` specifies the pointer to the bitmap base.
	/// 
	/// ## Safety
	/// You should guarantee `ptr` is at least aligned to pointer granularity. \
	/// Otherwise, panic may happen during bitmap operations.
	pub const unsafe fn from_raw_parts_mut(ptr:*mut c_void)->&'a mut Self
	{
		unsafe
		{
			&mut *ptr.cast()
		}
	}
}

impl<const N:usize> Bitmap<N>
{
	pub fn test(&self,position:usize)->bool
	{
		#[cfg(target_arch="x86_64")]
		{
			let bmp:*const i64=(&raw const *self).cast();
			unsafe
			{
				_bittest64(bmp,position as i64)!=0
			}
		}
	}

	pub fn set(&mut self,position:usize)->bool
	{
		#[cfg(target_arch="x86_64")]
		{
			let bmp:*mut i64=(&raw mut *self).cast();
			unsafe
			{
				_bittestandset64(bmp,position as i64)!=0
			}
		}
	}

	pub fn reset(&mut self,position:usize)->bool
	{
		#[cfg(target_arch="x86_64")]
		{
			let bmp:*mut i64=(&raw mut *self).cast();
			unsafe
			{
				_bittestandreset64(bmp,position as i64)!=0
			}
		}
	}

	pub fn complement(&mut self,position:usize)->bool
	{
		#[cfg(target_arch="x86_64")]
		{
			let bmp:*mut i64=(&raw mut *self).cast();
			unsafe
			{
				_bittestandcomplement64(bmp,position as i64)!=0
			}
		}
	}

	pub fn assign(&mut self,position:usize,value:bool)->bool
	{
		#[cfg(target_arch="x86_64")]
		{
			let bmp:*mut i64=(&raw mut *self).cast();
			unsafe
			{
				if value
				{
					_bittestandset64(bmp,position as i64)!=0
				}
				else
				{
					_bittestandreset64(bmp,position as i64)!=0
				}
			}
		}
	}

	pub fn search_cleared_forward(&self)->Option<usize>
	{
		#[cfg(target_arch="x86_64")]
		{
			let bmp:*const u64=(&raw const *self).cast();
			let lim=N>>6;
			for i in 0..lim
			{
				let j:u64;
				let b:u8;
				unsafe
				{
					asm!
					(
						"bsf {r},{v}",
						"setz {zf}",
						v=in(reg) !bmp.add(i).read(),
						r=out(reg) j,
						zf=out(reg_byte) b
					);
				}
				if b==0
				{
					return Some(j as usize);
				}
			}
			None
		}
	}
}