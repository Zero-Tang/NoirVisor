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

use core::{ffi::c_void,cmp::Ordering,ops::Add};
use alloc::vec::Vec;

use log::info;
use nvcvm::status::Status;
use static_collections::string::StaticString;

pub type IoInputFilterHandler<T>=fn(region:&IoRegion<T>,address:T,size:T,value:*mut c_void,context:*mut c_void);
pub type IoOutputFilterHandler<T>=fn(region:&IoRegion<T>,address:T,size:T,value:*const c_void,context:*mut c_void);

// Use generics on I/O filtering architecture, as the sizes of addresses can vary.
// For example, in x86 systems, there are two I/O subsystems: Port I/O and Memory-Mapped I/O.
// PIO is 16-bit addressing, and MMIO is 64-bit addressing.
// So PIO will use IoRegion<u16>, and MMIO will use IoRegion<u64>.
// There might be some other weirdo addressing modes (probably not even simple integers) of I/O in other architectures, so use generics to reduce problems.
pub struct IoRegion<T>
{
	pub name:StaticString<32>,
	pub input_handler:Option<IoInputFilterHandler<T>>,
	pub output_handler:IoOutputFilterHandler<T>,
	pub addr:T,
	pub size:T
}

impl<T:PartialOrd+Add<Output=T>+Copy> IoRegion<T>
{
	/// ## `new` method
	/// This method will create a new I/O region. \
	/// You must also use `add_region` method from `IoAddressSpace` to bind the new region to a specific I/O address space.
	pub fn new(name:&str,input_handler:Option<IoInputFilterHandler<T>>,output_handler:IoOutputFilterHandler<T>,addr:T,size:T)->Self
	{
		Self
		{
			name:StaticString::from(name),
			input_handler,
			output_handler,
			addr,
			size
		}
	}

	/// ## `try_dispatch` method
	/// This is an internal method which helps binary search when dispatching I/O.
	fn try_dispatch(&self,addr:T)->Ordering
	{
		if addr<self.addr
		{
			Ordering::Less
		}
		else if addr>=self.addr+self.size
		{
			Ordering::Greater
		}
		else
		{
			Ordering::Equal
		}
	}
}

impl<T:PartialEq> PartialEq for IoRegion<T>
{
	fn eq(&self, other: &Self) -> bool
	{
		self.addr==other.addr
	}
}

impl<T:PartialOrd> PartialOrd for IoRegion<T>
{
	fn partial_cmp(&self, other: &Self) -> Option<Ordering>
	{
		if self.addr<other.addr
		{
			Some(Ordering::Less)
		}
		else if self.addr>other.addr
		{
			Some(Ordering::Greater)
		}
		else
		{
			Some(Ordering::Equal)
		}
	}
}

pub struct IoAddressSpace<T>
{
	pub regions:Vec<IoRegion<T>>
}

impl<T:PartialOrd+Add<Output=T>+Copy> IoAddressSpace<T>
{
	/// ## `add_region` method
	/// This method binds a region to this I/O address space. 
	pub fn add_region(&mut self,region:IoRegion<T>)
	{
		if let Err(i)=self.regions.binary_search_by(|r| r.try_dispatch(region.addr))
		{
			info!("Inserting region to index {i}...");
			self.regions.insert(i,region);
		}
	}

	/// ## `try_dispatch` method
	/// This is an internal method which uses binary search to dispatch I/O.
	fn try_dispatch(&self,addr:T)->Option<&IoRegion<T>>
	{
		// Use binary search.
		match self.regions.binary_search_by(|r| r.try_dispatch(addr))
		{
			Ok(i)=>Some(&self.regions[i]),
			Err(_)=>None
		}
	}

	/// ## `dispatch_input` method
	/// This method dispatches an input operation to the corresponding handler.
	pub fn dispatch_input(&self,addr:T,size:T,value:*mut c_void,context:*mut c_void)->Result<(),Status>
	{
		let io_region=self.try_dispatch(addr);
		match io_region
		{
			Some(r)=>
			{
				match r.input_handler
				{
					Some(f)=>
					{
						f(r,addr,size,value,context);
						Ok(())
					}
					None=>Err(Status::DISPATCH_FAILURE)
				}
			}
			None=>Err(Status::DISPATCH_FAILURE)
		}
	}

	/// ## `dispatch_output` method
	/// This method dispatches an output operation to the corresponding handler.
	pub fn dispatch_output(&self,addr:T,size:T,value:*const c_void,context:*mut c_void)->Result<(),Status>
	{
		let io_region=self.try_dispatch(addr);
		match io_region
		{
			Some(r)=>
			{
				(r.output_handler)(r,addr,size,value,context);
				Ok(())
			}
			None=>Err(Status::DISPATCH_FAILURE)
		}
	}
}