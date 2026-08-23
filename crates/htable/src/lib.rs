#![no_std]
#![feature(allocator_api)]

extern crate alloc;

use core::ptr::null_mut;

use alloc::alloc::{AllocError,Layout};
use static_collections::bitmap::RefBitmap;

/// The `HandleTable<T>` data structure.
/// 
/// ## Structure
/// The `base` points to the array of data. After the array, \
/// there's the bitmap which tracks valid data entries in the handle table.
/// 
/// ## Allocation Strategy
/// The allocation must guarantee that the capacity is divisible by `usize::BITS`. \
/// No panicking is allowed even if allocation fails. \
/// When expanding the bitmap, the bitmap's newly-expanded area must be cleared to zero.
pub struct HandleTable<T>
{
	/// The maximum number of items this handle table can handle without reallocation.
	capacity:usize,
	/// The pointer to the array of data followed by a bitmap that tracks occupancy.
	base:*mut T
}

impl<T> HandleTable<T>
{
	pub const fn new()->Self
	{
		Self
		{
			capacity:0,
			base:null_mut()
		}
	}

	const fn bitmap_words(capacity:usize)->usize
	{
		capacity/(usize::BITS as usize)
	}

	fn allocation_layout(capacity:usize)->Result<(Layout,usize),AllocError>
	{
		let base_layout=Layout::array::<T>(capacity).map_err(|_| AllocError)?;
		let bitmap_layout=Layout::array::<usize>(Self::bitmap_words(capacity)).map_err(|_| AllocError)?;
		base_layout.extend(bitmap_layout).map_err(|_|AllocError)
	}

	fn bitmap(&self)->*mut usize
	{
		let (_,offset)=match Self::allocation_layout(self.capacity)
		{
			Ok(layout)=>layout,
			Err(_)=>return null_mut()
		};
		unsafe
		{
			self.base.cast::<u8>().add(offset).cast()
		}
	}

	fn bitmap_word(&self,word:usize)->Option<&RefBitmap<{usize::BITS as usize}>>
	{
		if word>=Self::bitmap_words(self.capacity)
		{
			return None;
		}
		unsafe
		{
			Some(RefBitmap::from_raw_ptr(self.bitmap().add(word)))
		}
	}

	fn bitmap_word_mut(&mut self,word:usize)->Option<&mut RefBitmap<{usize::BITS as usize}>>
	{
		if word>=Self::bitmap_words(self.capacity)
		{
			return None;
		}
		unsafe
		{
			Some(RefBitmap::from_raw_mut_ptr(self.bitmap().add(word)))
		}
	}

	/// Reserves more space for the handle table. \
	/// It should follow the allocation strategy defined by the handle table.
	pub fn try_reserve(&mut self)->Result<(),AllocError>
	{
		let new_capacity=if self.capacity==0
		{
			usize::BITS as usize
		}
		else
		{
			self.capacity.checked_mul(2).ok_or(AllocError)?
		};
		let (new_layout,new_bitmap_offset)=Self::allocation_layout(new_capacity)?;
		let old_layout=if self.capacity!=0
		{
			Some(Self::allocation_layout(self.capacity)?.0)
		}
		else
		{
			None
		};
		let new_base=unsafe{alloc::alloc::alloc_zeroed(new_layout)};
		if new_base.is_null()
		{
			return Err(AllocError);
		}
		let new_base=new_base.cast::<T>();
		if self.capacity!=0
		{
			let old_bitmap_offset=Self::allocation_layout(self.capacity)?.1;
			let old_bitmap=self.base.cast::<u8>().wrapping_add(old_bitmap_offset).cast::<usize>();
			let new_bitmap=unsafe{new_base.cast::<u8>().add(new_bitmap_offset).cast::<usize>()};
			for word in 0..Self::bitmap_words(self.capacity)
			{
				unsafe
				{
					new_bitmap.add(word).write(old_bitmap.add(word).read());
				}
			}
			for handle in 0..self.capacity
			{
				let word=handle/(usize::BITS as usize);
				let bit=handle%(usize::BITS as usize);
				let occupied=match self.bitmap_word(word)
				{
					Some(bitmap)=>bitmap.test(bit).unwrap_or(false),
					None=>false
				};
				if occupied
				{
					unsafe
					{
						new_base.add(handle).write(self.base.add(handle).read());
					}
				}
			}
			if let Some(old_layout)=old_layout
			{
				unsafe
				{
					alloc::alloc::dealloc(self.base.cast(),old_layout);
				}
			}
		}
		self.base=new_base;
		self.capacity=new_capacity;
		Ok(())
	}

	/// Creates a handle in the table and place the data there.
	pub fn create_handle(&mut self,data:T)->Result<usize,AllocError>
	{
		if self.capacity==0
		{
			self.try_reserve()?;
		}
		let mut handle=0;
		let mut found=false;
		for word_index in 0..Self::bitmap_words(self.capacity)
		{
			let bitmap=match self.bitmap_word_mut(word_index)
			{
				Some(bitmap)=>bitmap,
				None=>return Err(AllocError)
			};
			if let Some(bit)=bitmap.search_cleared_forward()
			{
				if bitmap.set(bit).unwrap_or(true)
				{
					continue;
				}
				handle=word_index*(usize::BITS as usize)+bit;
				found=true;
			}
			if found
			{
				break;
			}
		}
		if !found
		{
			self.try_reserve()?;
			return self.create_handle(data);
		}
		unsafe
		{
			self.base.add(handle).write(data);
		}
		Ok(handle)
	}

	/// Deletes the handle and returns the corresponding data there.
	pub fn delete_handle(&mut self,handle:usize)->Option<T>
	{
		if handle>=self.capacity
		{
			return None;
		}
		let word_index=handle/(usize::BITS as usize);
		let bit=handle%(usize::BITS as usize);
		let bitmap=match self.bitmap_word_mut(word_index)
		{
			Some(bitmap)=>bitmap,
			None=>return None
		};
		if !bitmap.reset(bit).unwrap_or(false)
		{
			return None;
		}
		unsafe
		{
			Some(self.base.add(handle).read())
		}
	}

	pub fn get(&self,handle:usize)->Option<&T>
	{
		if handle>=self.capacity
		{
			return None;
		}
		let word_index=handle/(usize::BITS as usize);
		let bit=handle%(usize::BITS as usize);
		let bitmap=match self.bitmap_word(word_index)
		{
			Some(bitmap)=>bitmap,
			None=>return None
		};
		if !bitmap.test(bit).unwrap_or(false)
		{
			return None;
		}
		unsafe
		{
			Some(&*self.base.add(handle))
		}
	}

	pub fn get_mut(&mut self,handle:usize)->Option<&mut T>
	{
		if handle>=self.capacity
		{
			return None;
		}
		let word_index=handle/(usize::BITS as usize);
		let bit=handle%(usize::BITS as usize);
		let bitmap=match self.bitmap_word_mut(word_index)
		{
			Some(bitmap)=>bitmap,
			None=>return None
		};
		if !bitmap.test(bit).unwrap_or(false)
		{
			return None;
		}
		unsafe
		{
			Some(&mut *self.base.add(handle))
		}
	}

	pub fn iter(&self)->HTableIter<'_,T>
	{
		HTableIter
		{
			source:self,
			index:0
		}
	}

	pub fn iter_mut(&mut self)->HTableIterMut<'_,T>
	{
		HTableIterMut
		{
			source:self,
			index:0
		}
	}
}

impl<T> Default for HandleTable<T>
{
	fn default() -> Self
	{
		Self::new()
	}
}

impl<T> Drop for HandleTable<T>
{
	fn drop(&mut self)
	{
		if self.base.is_null()
		{
			return;
		}
		if self.bitmap().is_null()
		{
			return;
		}
		for handle in 0..self.capacity
		{
			let word_index=handle/(usize::BITS as usize);
			let bit=handle%(usize::BITS as usize);
			let occupied=match self.bitmap_word(word_index)
			{
				Some(bitmap)=>bitmap.test(bit).unwrap_or(false),
				None=>false
			};
			if occupied
			{
				unsafe
				{
					core::ptr::drop_in_place(self.base.add(handle));
				}
			}
		}
		if let Ok((layout,_))=Self::allocation_layout(self.capacity)
		{
			unsafe
			{
				alloc::alloc::dealloc(self.base.cast(),layout);
			}
		}
	}
}

unsafe impl<T:Send> Send for HandleTable<T> {}
unsafe impl<T:Sync> Sync for HandleTable<T> {}

pub struct HTableIter<'a,T>
{
	source:&'a HandleTable<T>,
	index:usize
}

impl<'a,T> Iterator for HTableIter<'a,T>
{
	type Item = (usize,&'a T);

	fn next(&mut self) -> Option<Self::Item>
	{
		while self.index<self.source.capacity
		{
			let handle=self.index;
			self.index+=1;
			if let Some(data)=self.source.get(handle)
			{
				return Some((handle,data));
			}
		}
		None
	}
}

pub struct HTableIterMut<'a,T>
{
	source:&'a mut HandleTable<T>,
	index:usize
}

impl<'a,T> Iterator for HTableIterMut<'a,T>
{
	type Item = (usize,&'a mut T);

	fn next(&mut self) -> Option<Self::Item>
	{
		while self.index<self.source.capacity
		{
			let handle=self.index;
			self.index+=1;
			if let Some(data)=self.source.get_mut(handle)
			{
				let data:&'a mut T=unsafe{core::mem::transmute(data)};
				return Some((handle,data));
			}
		}
		None
	}
}

#[cfg(test)] mod test
{
    use core::sync::atomic::{AtomicUsize, Ordering};

	use crate::HandleTable;

	struct DropCounted<'a>(&'a AtomicUsize);

	impl<'a> Drop for DropCounted<'_>
	{
		fn drop(&mut self)
		{
			self.0.fetch_add(1,Ordering::Relaxed);
		}
	}

	#[test] fn drop_test()
	{
		let count=AtomicUsize::new(0);
		let mut ht:HandleTable<DropCounted>=HandleTable::new();
		let new_dc=|| DropCounted(&count);
		assert_eq!(ht.create_handle(new_dc()).unwrap(),0);
		assert_eq!(ht.create_handle(new_dc()).unwrap(),1);
		assert_eq!(ht.capacity,usize::BITS as usize);
		assert_eq!(ht.delete_handle(0).unwrap().0.load(Ordering::Relaxed),0);
		assert_eq!(count.load(Ordering::Relaxed),1);
		assert_eq!(ht.create_handle(new_dc()).unwrap(),0);
		drop(ht);
		assert_eq!(count.load(Ordering::Relaxed),3);
	}
}
