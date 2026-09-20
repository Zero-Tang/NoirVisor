// NoirVisor CVM Kernel Memory-Mapping Wrapper Library for Windows Kernel

use core::{ffi::c_void, ptr::{null, null_mut}, slice};

use alloc::alloc::AllocError;
use nvcvm::{interface::Vpcb, status::Status};
use windows_sys::{Wdk::{Foundation::MDL, System::SystemServices::{ExFreePool, IoAllocateMdl, IoFreeMdl, IoWriteAccess, KernelMode, MmAllocatePagesForMdl, MmCached, MmFreePagesFromMdl, MmMapLockedPagesSpecifyCache, MmProbeAndLockPages, MmUnlockPages, MmUnmapLockedPages, NormalPagePriority, UserMode}}, Win32::Foundation::{NTSTATUS, STATUS_SUCCESS}};

unsafe extern "system"
{
	fn call_within_seh(function:unsafe extern "system" fn (context:*mut c_void),context:*mut c_void)->NTSTATUS;
}

pub struct Kmap
{
	mdl:*mut MDL
}

unsafe impl Send for Kmap {}
unsafe impl Sync for Kmap {}

impl Kmap
{
	pub fn new(uva:u64,size:usize)->Result<Self,Status>
	{
		let mdl=unsafe{IoAllocateMdl(uva as *const c_void,size as u32,false,false,null_mut())};
		if mdl.is_null()
		{
			Err(Status::INSUFFICIENT_RESOURCES)
		}
		else
		{
			// MmProbeAndLockPages should be wrapped inside SEH block in order to capture errors.
			// However, Rust does not have innate SEH syntaxes. Use assembly codes to capture the SEH error status.
			unsafe extern "system" fn call_probe_and_lock(context:*mut c_void)
			{
				let mdl:*mut MDL=context.cast();
				unsafe
				{
					MmProbeAndLockPages(mdl,UserMode as i8,IoWriteAccess);
				}
			}
			let st:NTSTATUS=unsafe{call_within_seh(call_probe_and_lock,mdl.cast())};
			if st==STATUS_SUCCESS
			{
				Ok(Self{mdl})
			}
			else
			{
				unsafe
				{
					IoFreeMdl(mdl);
				}
				// MmProbeAndLockPages would only raise STATUS_ACCESS_DENIED as exception code.
				Err(Status::ACCESS_DENIED)
			}
		}
	}

	pub fn uva(&self)->*mut c_void
	{
		unsafe
		{
			(*self.mdl).StartVa
		}
	}

	pub fn size(&self)->usize
	{
		unsafe
		{
			(*self.mdl).ByteCount as usize
		}
	}

	fn pfn_array(&self)->&[u64]
	{
		unsafe
		{
			let mdl=&*self.mdl;
			let p:*const u64=self.mdl.add(1).cast();
			let bytes=mdl.ByteCount as usize;
			let offset=(mdl.StartVa as usize)&0xfff;
			let span=bytes+offset+0xfff;
			let pages=span>>12;
			slice::from_raw_parts(p,pages)
		}
	}

	pub fn iter(&self)->KmapIter<'_>
	{
		KmapIter
		{
			source:self.pfn_array(),
			index:0
		}
	}
}

impl Drop for Kmap
{
	fn drop(&mut self)
	{
		unsafe
		{
			MmUnlockPages(self.mdl);
			IoFreeMdl(self.mdl);
		}
	}
}

pub struct KmapIter<'a>
{
	source:&'a [u64],
	index:usize
}

impl<'a> Iterator for KmapIter<'a>
{
	type Item = u64;

	fn next(&mut self)->Option<Self::Item>
	{
		let i=self.index;
		self.index+=1;
		self.source.get(i).map(|&x| x<<12)
	}
}

pub struct UniversalPage
{
	mdl:*mut MDL,
	uva:*mut c_void,
	kva:*mut c_void
}

impl UniversalPage
{
	pub fn new()->Result<Self,AllocError>
	{
		let mut r:Self=unsafe{core::mem::zeroed()};
		r.mdl=unsafe{MmAllocatePagesForMdl(0,i64::MAX,0,0x1000)};
		if r.mdl.is_null()
		{
			return Err(AllocError);
		}
		r.kva=unsafe{MmMapLockedPagesSpecifyCache(r.mdl,KernelMode as i8,MmCached,null(),0,NormalPagePriority as u32)};
		if r.kva.is_null()
		{
			return Err(AllocError);
		}
		r.uva=unsafe{MmMapLockedPagesSpecifyCache(r.mdl,UserMode as i8,MmCached,null(),0,NormalPagePriority as u32)};
		if r.uva.is_null()
		{
			return Err(AllocError);
		}
		Ok(r)
	}

	pub fn uva(&self)->*mut Vpcb
	{
		self.uva.cast()
	}

	pub fn kva(&self)->*mut Vpcb
	{
		self.kva.cast()
	}

	pub fn hpa(&self)->u64
	{
		let x:u64=unsafe{*self.mdl.add(1).cast()};
		x<<12
	}
}

unsafe impl Send for UniversalPage {}
unsafe impl Sync for UniversalPage {}

impl Drop for UniversalPage
{
	fn drop(&mut self)
	{
		unsafe
		{
			if !self.uva.is_null()
			{
				MmUnmapLockedPages(self.uva,self.mdl);
			}
			if !self.kva.is_null()
			{
				MmUnmapLockedPages(self.kva,self.mdl);
			}
			if !self.mdl.is_null()
			{
				MmFreePagesFromMdl(self.mdl);
				ExFreePool(self.mdl.cast());
			}
		}
	}
}