#![no_std]

extern crate alloc;

pub fn get_current_process_id()->u32
{
	0
}

pub mod sync
{
	use core::ops::{Deref, DerefMut};

	pub struct RwLock<T>(spin::RwLock<T>);
	unsafe impl<T: Send + Sync> Send for RwLock<T> {}
	unsafe impl<T: Send + Sync> Sync for RwLock<T> {}
	pub struct RwLockReadGuard<'a,T>(spin::RwLockReadGuard<'a,T>);
	pub struct RwLockWriteGuard<'a,T>(spin::RwLockWriteGuard<'a,T>);
	pub struct Mutex<T>(spin::Mutex<T>);
	unsafe impl<T: Send> Send for Mutex<T> {}
	unsafe impl<T: Send> Sync for Mutex<T> {}
	pub struct MutexGuard<'a,T>(spin::MutexGuard<'a,T>);

	impl<T> RwLock<T>
	{
		pub const fn new(data:T)->Self
		{
			Self(spin::RwLock::new(data))
		}

		/// The `init` method initializes the RwLock.
		/// 
		/// ## Safety
		/// RwLock cannot be initialized twice. \
		/// An RwLock created by `new` cannot be used before `init`.
		pub unsafe fn init(&self)->bool
		{
			true
		}

		/// The `deinit` method drops the resource lock.
		/// 
		/// ## Safety
		/// After `deinit`, the lock is still accessible. \
		/// However, you must `init` it again in order to use it safely. \
		/// Any attempt to use a dropped resource lock may cause runtime panic.
		pub unsafe fn deinit(&self)
		{
		}

		pub fn read(&self)->RwLockReadGuard<'_,T>
		{
			RwLockReadGuard(self.0.read())
		}

		pub fn write(&self)->RwLockWriteGuard<'_,T>
		{
			RwLockWriteGuard(self.0.write())
		}
	}

	impl<T> Deref for RwLockReadGuard<'_,T>
	{
		type Target=T;
		fn deref(&self)->&Self::Target
		{
			self.0.deref()
		}
	}

	impl<T> Deref for RwLockWriteGuard<'_,T>
	{
		type Target=T;
		fn deref(&self)->&Self::Target
		{
			self.0.deref()
		}
	}

	impl<T> DerefMut for RwLockWriteGuard<'_,T>
	{
		fn deref_mut(&mut self)->&mut Self::Target
		{
			self.0.deref_mut()
		}
	}

	impl<T> Mutex<T>
	{
		pub const fn new(data:T)->Self
		{
			Self(spin::Mutex::new(data))
		}

		/// The `init` method initializes the Mutex.
		/// 
		/// ## Safety
		/// Mutex cannot be initialized twice. \
		/// A mutex created by `new` cannot be used before `init`.
		pub unsafe fn init(&self)->bool
		{
			true
		}
		
		/// The `deinit` method drops the resource lock.
		/// 
		/// ## Safety
		/// After `deinit`, the lock is still accessible. \
		/// However, you must `init` it again in order to use it safely. \
		/// Any attempt to use a dropped resource lock may cause runtime panic.
		pub unsafe fn deinit(&self)
		{
			// No non-const de-initialization is required for a spin-lock.
		}

		pub fn lock(&self)->MutexGuard<'_,T>
		{
			MutexGuard(self.0.lock())
		}
	}

	impl<T> Deref for MutexGuard<'_,T>
	{
		type Target=T;
		fn deref(&self)->&Self::Target
		{
			self.0.deref()
		}
	}

	impl<T> DerefMut for MutexGuard<'_,T>
	{
		fn deref_mut(&mut self)->&mut Self::Target
		{
			self.0.deref_mut()
		}
	}
}

pub mod kmap
{
    use core::{alloc::{AllocError, Layout}, ffi::c_void};

	use nvcvm::{interface::Vpcb, status::Status};

	pub struct Kmap
	{
		virt:*mut c_void,
		size:usize
	}

	unsafe impl Send for Kmap {}
	unsafe impl Sync for Kmap {}

	impl Kmap
	{
		pub fn new(uva:u64,size:usize)->Result<Self,Status>
		{
			Ok
			(
				Self
				{
					virt:uva as *mut c_void,
					size
				}
			)
		}

		pub fn uva(&self)->*mut c_void
		{
			self.virt
		}

		pub fn size(&self)->usize
		{
			self.size
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
				let phys=self.source.virt as u64+self.offset as u64;
				self.offset+=0x1000;
				Some(phys)
			}
			else
			{
				None
			}
		}
	}

	pub struct UniversalPage(*mut u8);

	unsafe impl Send for UniversalPage {}
	unsafe impl Sync for UniversalPage {}

	impl UniversalPage
	{
		const LAYOUT:Layout=unsafe{Layout::from_size_align_unchecked(0x1000,0x1000)};

		pub fn new()->Result<Self,AllocError>
		{
			let p=unsafe{alloc::alloc::alloc(Self::LAYOUT)};
			if p.is_null()
			{
				Err(AllocError)
			}
			else
			{
				Ok(Self(p))
			}
		}

		pub fn uva(&self)->*mut Vpcb
		{
			self.0.cast()
		}

		pub fn kva(&self)->*mut Vpcb
		{
			self.0.cast()
		}

		pub fn hpa(&self)->u64
		{
			self.0 as u64
		}
	}

	impl Drop for UniversalPage
	{
		fn drop(&mut self)
		{
			unsafe
			{
				alloc::alloc::dealloc(self.0,Self::LAYOUT);
			}
		}
	}
}