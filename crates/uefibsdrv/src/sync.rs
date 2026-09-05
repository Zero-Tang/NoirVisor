// NoirVisor CVM Scheduler as UEFI Boot-Service Driver

use core::{cell::UnsafeCell, hint::spin_loop, ops::{Deref, DerefMut}, sync::atomic::{AtomicBool, AtomicU32, Ordering}};

pub struct RwLock<T>
{
	counter:AtomicU32,
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
			counter:AtomicU32::new(0),
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
		self.counter.store(0,Ordering::Relaxed);
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

	pub fn read(&self)->RwLockSharedGuard<'_,T>
	{
		loop
		{
			let counter=self.counter.load(Ordering::SeqCst);
			if counter>=u32::MAX-1
			{
				core::hint::spin_loop();
				continue;
			}
			if self.counter.compare_exchange_weak(counter,counter+1,Ordering::SeqCst,Ordering::SeqCst).is_ok()
			{
				return RwLockSharedGuard(self);
			}
			spin_loop();
		}
	}

	pub fn write(&self)->RwLockExclusiveGuard<'_,T>
	{
		loop
		{
			if self.counter.compare_exchange_weak(0,u32::MAX,Ordering::SeqCst,Ordering::SeqCst).is_ok()
			{
				return RwLockExclusiveGuard(self);
			}
			spin_loop();
		}
	}
}

pub struct RwLockSharedGuard<'a,T>(&'a RwLock<T>);

impl<T> Drop for RwLockSharedGuard<'_,T>
{
	fn drop(&mut self)
	{
		self.0.counter.fetch_sub(1,Ordering::SeqCst);
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
		self.0.counter.store(0,Ordering::SeqCst);
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
	lock:AtomicBool,
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
			lock:AtomicBool::new(false),
			data:UnsafeCell::new(data)
		}
	}

	/// The `init` method initializes the Mutex.
	/// 
	/// ## Safety
	/// Mutex cannot be initialized twice. \
	/// A mutex created by `new` cannot be used before `init`.
	pub unsafe fn init(&self)->bool
	{
		self.lock.store(false,Ordering::Relaxed);
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
		while self.lock.compare_exchange(false,true,Ordering::SeqCst,Ordering::SeqCst).is_err()
		{
			spin_loop();
		}
		MutexGuard(self)
	}
}

pub struct MutexGuard<'a,T>(&'a Mutex<T>);

impl<T> Drop for MutexGuard<'_,T>
{
	fn drop(&mut self)
	{
		self.0.lock.store(false,Ordering::SeqCst);
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