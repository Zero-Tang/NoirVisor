/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 * 
 * This file defines I/O Filtering Architecture of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{cmp::Ordering, ffi::c_void};
use alloc::{alloc::{Allocator, Global}, boxed::Box, vec::Vec};

use log::info;
use nvcvm::status::Status;

pub trait IoRegionOps
{
	/// ## `name` method
	/// Returns the name of the I/O Region.
	fn name(&self)->&str;
	/// ## `input` method
	/// If `forward_input` returns `true`, the implementation can be empty. \
	/// Otherwise, this trait method must implement input filtering.
	fn input(&mut self,address:u64,value:&mut [u8],context:*mut c_void)->Status;
	/// `output` method
	/// If `forward_output` returns `true`, the implementation can be empty. \
	/// Otherwise, this trait method must implement output filtering.
	fn output(&mut self,address:u64,value:&[u8],context:*mut c_void)->Status;
	/// `base_size` method
	/// Returns the base address and the size.
	fn base_size(&self)->(u64,usize);
	/// `forward_input` method
	/// Returns whether the input operation should be forwarded to the hardware.
	fn forward_input(&self)->bool;
	/// `forward_output` method
	/// Returns whether the output operation should be forwarded to the hardware.
	fn forward_output(&self)->bool;

	/// `binary_search_helper` method
	/// Helps locates the I/O Region faster.
	fn binary_search_helper(&self,addr:u64)->Ordering
	{
		let (b,s)=self.base_size();
		if addr<b
		{
			Ordering::Less
		}
		else if addr>=(b+s as u64)
		{
			Ordering::Greater
		}
		else
		{
			Ordering::Equal
		}
	}
}

pub struct IoAddressSpace<A:Allocator=Global>
{
	pub regions:Vec<Box<dyn IoRegionOps,A>,A>
}

impl<A:Allocator> IoAddressSpace<A>
{
	/// ## `add_region` method
	/// This method binds a region to this I/O address space. 
	pub fn add_region(&mut self,region:Box<dyn IoRegionOps,A>)
	{
		let (base,_)=region.base_size();
		if let Err(i)=self.regions.binary_search_by(|r| r.binary_search_helper(base))
		{
			info!("Inserting region to index {i}...");
			self.regions.insert(i,region);
		}
	}

	/// ## `try_dispatch` method
	/// This is an internal method which uses binary search to dispatch I/O.
	fn try_dispatch(&mut self,addr:u64)->Option<&mut dyn IoRegionOps>
	{
		// Use binary search.
		match self.regions.binary_search_by(|r| r.binary_search_helper(addr))
		{
			Ok(i)=>Some(&mut *self.regions[i]),
			Err(_)=>None
		}
	}

	/// ## `dispatch_input` method
	/// This method dispatches an input operation to the corresponding handler.
	pub fn dispatch_input(&mut self,addr:u64,value:&mut [u8],context:*mut c_void)->Result<(),Status>
	{
		let io_region=self.try_dispatch(addr);
		match io_region
		{
			Some(r)=>match r.input(addr,value,context)
			{
				Status::SUCCESS=>Ok(()),
				st=>Err(st)
			}
			None=>Err(Status::DISPATCH_FAILURE)
		}
	}

	/// ## `dispatch_output` method
	/// This method dispatches an output operation to the corresponding handler.
	pub fn dispatch_output(&mut self,addr:u64,value:&[u8],context:*mut c_void)->Result<(),Status>
	{
		let io_region=self.try_dispatch(addr);
		match io_region
		{
			Some(r)=>match r.output(addr,value,context)
			{
				Status::SUCCESS=>Ok(()),
				st=>Err(st)
			}
			None=>Err(Status::DISPATCH_FAILURE)
		}
	}
}