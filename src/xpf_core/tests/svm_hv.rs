/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file mocks SVM Hypercall APIs for testing NoirVisor CVM in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use nvcvm::status::Status;
use static_collections::bitmap::RefBitmap;

use crate::xpf_core::hv_host::{NOIR_HYPERCALL_CODE_CVM_ALLOC_TLB_TAG, NOIR_HYPERCALL_CODE_CVM_FREE_TLB_TAG};
use super::std::{println,sync::Mutex};

static ASID_BITMAP:Mutex<[u64;4]>=Mutex::new([3,0,0,0]);

pub fn mock_vmmcall(index:u32,context:usize)->u32
{
	match index
	{
		NOIR_HYPERCALL_CODE_CVM_ALLOC_TLB_TAG=>
		{
			let ret_asid:*mut u32=context as *mut u32;
			let mut lk=ASID_BITMAP.lock().unwrap();
			let bmp:&mut RefBitmap<256>=unsafe{RefBitmap::from_raw_mut_ptr(lk.as_mut_ptr().cast())};
			match bmp.search_cleared_backward()
			{
				Some(i)=>
				{
					let _=bmp.set(i);
					unsafe
					{
						*ret_asid=i as u32;
					}
					Status::SUCCESS.0
				}
				None=>Status::INSUFFICIENT_RESOURCES.0
			}
		}
		NOIR_HYPERCALL_CODE_CVM_FREE_TLB_TAG=>
		{
			let src_asid:*const u32=context as *const u32;
			let mut lk=ASID_BITMAP.lock().unwrap();
			let bmp:&mut RefBitmap<256>=unsafe{RefBitmap::from_raw_mut_ptr(lk.as_mut_ptr().cast())};
			match bmp.reset(unsafe{*src_asid as usize})
			{
				Ok(v)=> if v {Status::SUCCESS.0} else {Status::INVALID_PARAMETER.0}
				Err(_)=>Status::INVALID_PARAMETER.0
			}
		}
		_=>
		{
			println!("Unknown Hypercall index: 0x{index:X}!");
			Status::INVALID_PARAMETER.0
		}
	}
}