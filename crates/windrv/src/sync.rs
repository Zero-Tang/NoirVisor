// NoirVisor CVM Synchronization Primitives Wrapper Library for Windows Kernel

use core::{cell::UnsafeCell, mem::MaybeUninit, ops::{Deref, DerefMut}};

use windows_sys::{Wdk::{Foundation::{ERESOURCE, FAST_MUTEX}, System::SystemServices::{ExAcquireFastMutex, ExAcquireResourceExclusiveLite, ExAcquireResourceSharedLite, ExDeleteResourceLite, ExInitializeResourceLite, ExReleaseFastMutex, ExReleaseResourceLite, KeEnterCriticalRegion, KeLeaveCriticalRegion}}, Win32::Foundation::STATUS_SUCCESS};

#[link(name="ntoskrnl.exe",kind="raw-dylib",modifiers="+verbatim")]
unsafe extern "system"
{
	fn ExInitializeFastMutex(fastmutex:*mut FAST_MUTEX);
}

/// ## Resource Lock
/// Resource Lock is a read-write lock which supports recursive acquisition.
pub struct RwLock<T>
{
	lock:ERESOURCE,
	cell:UnsafeCell<T>
}

unsafe impl<T:Send> Send for RwLock<T> {}
unsafe impl<T:Send+Sync> Sync for RwLock<T> {}

impl<T> RwLock<T>
{
	pub const fn new(data:T)->Self
	{
		Self
		{
			lock:unsafe{MaybeUninit::zeroed().assume_init()},
			cell:UnsafeCell::new(data)
		}
	}

	/// The `init` method initializes the RwLock.
	/// 
	/// ## Safety
	/// RwLock cannot be initialized twice. \
	/// An RwLock created by `new` cannot be used before `init`.
	pub unsafe fn init(&self)->bool
	{
		unsafe
		{
			ExInitializeResourceLite(self.lock_ptr())==STATUS_SUCCESS
		}
	}

	/// The `deinit` method drops the resource lock.
	/// 
	/// ## Safety
	/// After `deinit`, the lock is still accessible. \
	/// However, you must `init` it again in order to use it safely. \
	/// Any attempt to use a dropped resource lock may cause runtime panic.
	pub unsafe fn deinit(&self)
	{
		unsafe
		{
			ExDeleteResourceLite(self.lock_ptr());
		}
	}

	const fn lock_ptr(&self)->*mut ERESOURCE
	{
		&raw const self.lock as *mut ERESOURCE
	}

	pub fn read(&self)->RwLockSharedGuard<'_,T>
	{
		unsafe
		{
			KeEnterCriticalRegion();
			ExAcquireResourceSharedLite(self.lock_ptr(),true);
		}
		RwLockSharedGuard(self)
	}

	pub fn write(&self)->RwLockExclusiveGuard<'_,T>
	{
		unsafe
		{
			KeEnterCriticalRegion();
			ExAcquireResourceExclusiveLite(self.lock_ptr(),true);
		}
		RwLockExclusiveGuard(self)
	}
}

impl<T> Drop for RwLock<T>
{
	fn drop(&mut self)
	{
		unsafe
		{
			self.deinit();
		}
	}
}

pub struct RwLockSharedGuard<'a,T>(&'a RwLock<T>);

impl<T> Drop for RwLockSharedGuard<'_,T>
{
	fn drop(&mut self)
	{
		unsafe
		{
			ExReleaseResourceLite(self.0.lock_ptr());
			KeLeaveCriticalRegion();
		}
	}
}

impl<T> Deref for RwLockSharedGuard<'_,T>
{
	type Target = T;
	fn deref(&self) -> &Self::Target
	{
		unsafe
		{
			&*self.0.cell.get()
		}
	}
}

pub struct RwLockExclusiveGuard<'a,T>(&'a RwLock<T>);

impl<T> Drop for RwLockExclusiveGuard<'_,T>
{
	fn drop(&mut self)
	{
		unsafe
		{
			ExReleaseResourceLite(self.0.lock_ptr());
			KeLeaveCriticalRegion();
		}
	}
}

impl<T> Deref for RwLockExclusiveGuard<'_,T>
{
	type Target = T;
	fn deref(&self) -> &Self::Target
	{
		unsafe
		{
			&*self.0.cell.get()
		}
	}
}

impl<T> DerefMut for RwLockExclusiveGuard<'_,T>
{
	fn deref_mut(&mut self) -> &mut Self::Target
	{
		unsafe
		{
			&mut *self.0.cell.get()
		}
	}
}

pub struct Mutex<T>
{
	lock:FAST_MUTEX,
	data:UnsafeCell<T>
}

unsafe impl<T:Send> Send for Mutex<T> {}
unsafe impl<T:Send> Sync for Mutex<T> {}

impl<T> Mutex<T>
{
	pub const fn new(data:T)->Self
	{
		Self
		{
			lock:unsafe{MaybeUninit::zeroed().assume_init()},
			data:UnsafeCell::new(data)
		}
	}

	/// The `init` method initializes the mutex.
	/// 
	/// ## Safety
	/// Mutex cannot be initialized twice. \
	/// A mutex created by `new` cannot be used before `init`.
	pub unsafe fn init(&self)->bool
	{
		unsafe
		{
			ExInitializeFastMutex(self.lock_ptr());
		}
		true
	}

	const fn lock_ptr(&self)->*mut FAST_MUTEX
	{
		&raw const self.lock as *mut FAST_MUTEX
	}
	
	pub fn lock(&self)->MutexGuard<'_,T>
	{
		unsafe
		{
			ExAcquireFastMutex(self.lock_ptr());
			MutexGuard(self)
		}
	}
}

pub struct MutexGuard<'a,T>(&'a Mutex<T>);

impl<T> Drop for MutexGuard<'_,T>
{
	fn drop(&mut self)
	{
		unsafe
		{
			ExReleaseFastMutex(self.0.lock_ptr());
		}
	}
}

impl<T> Deref for MutexGuard<'_,T>
{
	type Target = T;
	fn deref(&self) -> &Self::Target
	{
		unsafe
		{
			&*self.0.data.get()
		}
	}
}

impl<T> DerefMut for MutexGuard<'_,T>
{
	fn deref_mut(&mut self) -> &mut Self::Target
	{
		unsafe
		{
			&mut *self.0.data.get()
		}
	}
}
