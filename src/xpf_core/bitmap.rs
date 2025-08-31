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
		if position>=N
		{
			panic!("Bit position {position} exceeds the limit {N}!");
		}
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
		if position>=N
		{
			panic!("Bit position {position} exceeds the limit {N}!");
		}
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
		if position>=N
		{
			panic!("Bit position {position} exceeds the limit {N}!");
		}
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
		if position>=N
		{
			panic!("Bit position {position} exceeds the limit {N}!");
		}
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
		if position>=N
		{
			panic!("Bit position {position} exceeds the limit {N}!");
		}
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
			let lim=(N>>6)+if (N&0x3F)!=0 {1} else {0};
			for i in 0..lim
			{
				let j:u64;
				let b:u8;
				unsafe
				{
					asm!
					(
						"mov {v},qword ptr [{p}]",
						"not {v}",
						"bsf {r},{v}",
						"setz {zf}",
						p=in(reg) bmp.add(i),
						v=out(reg) _,
						r=out(reg) j,
						zf=out(reg_byte) b
					);
				}
				if b==0
				{
					let pos=(i<<6)+j as usize;
					return if pos<N {Some(pos)} else {None};
				}
			}
			None
		}
	}
}

#[cfg(test)]
mod tests
{
	extern crate std;

	use std::println;
	use super::Bitmap;
	
	#[test] fn set_and_test()
	{
		let mut bmp_raw:[u64;8]=[0;8];
		let bmp:&mut Bitmap<512>=unsafe{Bitmap::from_raw_parts_mut(bmp_raw.as_mut_ptr().cast())};
		assert_eq!(bmp.test(123),false);
		bmp.set(123);
		assert_eq!(bmp.test(123),true);
		assert_eq!(bmp.test(233),false);
		assert_eq!(bmp_raw[0],0);
		assert_eq!(bmp_raw[1],1<<(123-64));
	}

	#[test] fn search_cleared_forward()
	{
		let mut bmp_raw:[u64;8]=[0;8];
		bmp_raw[0]=u64::MAX;
		bmp_raw[1]=0x207;
		let bmp:&mut Bitmap<512>=unsafe{Bitmap::from_raw_parts_mut(bmp_raw.as_mut_ptr().cast())};
		let pos=bmp.search_cleared_forward();
		println!("Searched! Result is {pos:?}");
		assert!(pos.is_some());
		let pos=pos.unwrap();
		assert_eq!(pos,67);
		bmp.set(pos);
		assert_eq!(bmp_raw[1],0x20F);
	}
}