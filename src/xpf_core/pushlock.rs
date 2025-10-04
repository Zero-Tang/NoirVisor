/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file defines the PushLock for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{cell::UnsafeCell, ops::{Deref, DerefMut}, sync::atomic::AtomicUsize};
use crate::xpf_core::nvbdk::{noir_acquire_pushlock_exclusive, noir_acquire_pushlock_shared, noir_release_pushlock_exclusive, noir_release_pushlock_shared};

pub struct PushLock<T>
{
	push_lock:AtomicUsize,
	cell:UnsafeCell<T>
}

unsafe impl<T> Send for PushLock<T> {}
unsafe impl<T> Sync for PushLock<T> {}

impl<T> PushLock<T>
{
	pub const fn new(data:T)->Self
	{
		Self
		{
			push_lock:AtomicUsize::new(0),
			cell:UnsafeCell::new(data)
		}
	}

	pub fn read(&self)->PushLockSharedGuard<'_,T>
	{
		unsafe
		{
			noir_acquire_pushlock_shared(self.push_lock.as_ptr());
		}
		PushLockSharedGuard
		{
			lock:self
		}
	}

	pub fn write(&self)->PushLockExclusiveGuard<'_,T>
	{
		unsafe
		{
			noir_acquire_pushlock_exclusive(self.push_lock.as_ptr());
		}
		PushLockExclusiveGuard
		{
			lock:self
		}
	}

	fn de_read(&self)
	{
		unsafe
		{
			noir_release_pushlock_shared(self.push_lock.as_ptr());
		}
	}

	fn de_write(&self)
	{
		unsafe
		{
			noir_release_pushlock_exclusive(self.push_lock.as_ptr());
		}
	}
}

pub struct PushLockSharedGuard<'a,T>
{
	lock:&'a PushLock<T>
}

impl<T> Drop for PushLockSharedGuard<'_,T>
{
	fn drop(&mut self)
	{
		self.lock.de_read();
	}
}

impl<T> Deref for PushLockSharedGuard<'_,T>
{
	type Target = T;
	fn deref(&self) -> &Self::Target
	{
		unsafe
		{
			&*self.lock.cell.get()
		}
	}
}

pub struct PushLockExclusiveGuard<'a,T>
{
	lock:&'a PushLock<T>
}

impl<T> Drop for PushLockExclusiveGuard<'_,T>
{
	fn drop(&mut self)
	{
		self.lock.de_write();
	}
}

impl<T> Deref for PushLockExclusiveGuard<'_,T>
{
	type Target = T;
	fn deref(&self) -> &Self::Target
	{
		unsafe
		{
			&*self.lock.cell.get()
		}
	}
}

impl<T> DerefMut for PushLockExclusiveGuard<'_,T>
{
	fn deref_mut(&mut self) -> &mut Self::Target
	{
		unsafe
		{
			&mut *self.lock.cell.get()
		}
	}
}