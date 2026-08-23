// NoirVisor CVM Synchronization Primitives

use core::{cell::UnsafeCell, ffi::c_void, ops::{Deref, DerefMut}, ptr::null_mut, sync::atomic::{AtomicPtr, Ordering}};

unsafe extern "system"
{
	// Resource Locks routines.
    fn noir_create_reslock()->*mut c_void;
    fn noir_delete_reslock(reslock:*mut c_void);
    fn noir_acquire_reslock_shared(reslock:*mut c_void);
    fn noir_acquire_reslock_exclusive(reslock:*mut c_void);
    fn noir_release_reslock_shared(reslock:*mut c_void);
    fn noir_release_reslock_exclusive(reslock:*mut c_void);

	// Mutex routines.
	fn noir_create_mutex()->*mut c_void;
	fn noir_delete_mutex(mutex:*mut c_void);
	fn noir_acquire_mutex(mutex:*mut c_void);
	fn noir_release_mutex(mutex:*mut c_void);
}

/// ## Resource Lock
/// Resource Lock is a read-write lock which supports recursive acquisition.
pub struct RwLock<T>
{
	lock:AtomicPtr<c_void>,
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
			lock:AtomicPtr::new(null_mut()),
			cell:UnsafeCell::new(data)
		}
	}

	pub unsafe fn init(&self)->bool
	{
		let p=unsafe{noir_create_reslock()};
		if p.is_null()
		{
			false
		}
		else
		{
			self.lock.store(p,Ordering::Relaxed);
			true
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
		let p=self.lock.swap(null_mut(),Ordering::Relaxed);
		if !p.is_null()
		{
			unsafe
			{
				noir_delete_reslock(p);
			}
		}
	}

	pub fn read(&self)->RwLockSharedGuard<'_,T>
	{
		unsafe
		{
			noir_acquire_reslock_shared(self.lock.load(Ordering::Relaxed));
		}
		RwLockSharedGuard(self)
	}

	pub fn write(&self)->RwLockExclusiveGuard<'_,T>
	{
		unsafe
		{
			noir_acquire_reslock_exclusive(self.lock.load(Ordering::Relaxed));
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
			noir_release_reslock_shared(self.0.lock.load(Ordering::Relaxed));
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
			noir_release_reslock_exclusive(self.0.lock.load(Ordering::Relaxed));
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
	lock:AtomicPtr<c_void>,
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
			lock:AtomicPtr::new(null_mut()),
			data:UnsafeCell::new(data)
		}
	}

	pub unsafe fn init(&mut self)->bool
	{
		let p=unsafe{noir_create_mutex()};
		self.lock.store(p,Ordering::Relaxed);
		!p.is_null()
	}

	pub unsafe fn deinit(&mut self)
	{
		let p=self.lock.swap(null_mut(),Ordering::Relaxed);
		if !p.is_null()
		{
			unsafe
			{
				noir_delete_mutex(p);
			}
		}
	}
	
	pub fn lock(&self)->MutexGuard<'_,T>
	{
		unsafe
		{
			noir_acquire_mutex(self.lock.load(Ordering::Relaxed));
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
			noir_release_mutex(self.0.lock.load(Ordering::Relaxed));
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
