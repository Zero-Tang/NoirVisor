// NoirVisor CVM Scheduler as UEFI Boot-Service Driver

use core::sync::atomic::Ordering;

use alloc::alloc::AllocError;
use nvcvm::{interface::Vpcb, status::Status};
use r_efi::efi::{ALLOCATE_ANY_PAGES, BOOT_SERVICES_DATA, Status as EfiStatus};

use crate::misc::BS_TABLE;

pub struct Kmap
{
	phys:u64,
	size:usize
}

impl Kmap
{
	pub fn new(uva:u64,size:usize)->Result<Self,Status>
	{
		Ok
		(
			Self
			{
				phys:uva,
				size
			}
		)
	}

	pub fn iter(&self)->KmapIter<'_>
	{
		KmapIter
		{
			source:self,
			offset:0
		}
	}
}

pub struct KmapIter<'a>
{
	source:&'a Kmap,
	offset:usize
}

impl<'a> Iterator for KmapIter<'a>
{
	type Item = u64;

	fn next(&mut self) -> Option<Self::Item>
	{
		if self.offset<self.source.size
		{
			let phys=self.source.phys+self.offset as u64;
			self.offset+=0x1000;
			Some(phys)
		}
		else
		{
			None
		}
	}
}

pub struct UniversalPage(u64);

impl UniversalPage
{
	pub fn new()->Result<Self,AllocError>
	{
		let mut x=0;
		let st=unsafe
		{
			let bs=&*BS_TABLE.load(Ordering::Relaxed);
			(bs.allocate_pages)(ALLOCATE_ANY_PAGES,BOOT_SERVICES_DATA,1,&raw mut x)
		};
		if st==EfiStatus::SUCCESS
		{
			Ok(Self(x))
		}
		else
		{
			Err(AllocError)
		}
	}

	pub fn uva(&self)->*mut Vpcb
	{
		self.0 as *mut Vpcb
	}

	pub fn kva(&self)->*mut Vpcb
	{
		self.0 as *mut Vpcb
	}

	pub fn hpa(&self)->u64
	{
		self.0
	}
}

impl Drop for UniversalPage
{
	fn drop(&mut self)
	{
		unsafe
		{
			let bs=&*BS_TABLE.load(Ordering::Relaxed);
			(bs.free_pages)(self.0,1);
		}
	}
}