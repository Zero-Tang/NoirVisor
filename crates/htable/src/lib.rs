#![no_std]
#![feature(allocator_api)]

extern crate alloc;

use core::ptr::null_mut;

use alloc::alloc::{AllocError,Layout};
use static_collections::bitmap::BitmapSlice;

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
	/// Creates a handle table for type `T`.
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

	fn bitmap_slice(&self)->Option<&BitmapSlice>
	{
		if self.capacity==0
		{
			return None;
		}
		let (_,offset)=Self::allocation_layout(self.capacity).ok()?;
		let len=Self::bitmap_words(self.capacity)*size_of::<usize>();
		unsafe
		{
			Some(BitmapSlice::from_raw_parts(self.base.cast::<u8>().add(offset),len))
		}
	}

	fn bitmap_slice_mut(&mut self)->Option<&mut BitmapSlice>
	{
		if self.capacity==0
		{
			return None;
		}
		let (_,offset)=Self::allocation_layout(self.capacity).ok()?;
		let len=Self::bitmap_words(self.capacity)*size_of::<usize>();
		unsafe
		{
			Some(BitmapSlice::from_raw_parts_mut(self.base.cast::<u8>().add(offset).cast::<u8>(),len))
		}
	}

	/// Reserves more space for the handle table. \
	/// It should follow the allocation strategy defined by the handle table.
	pub fn try_reserve(&mut self)->Result<(),AllocError>
	{
		// Determine the new capacity and layout.
		let new_capacity=if self.capacity==0
		{
			usize::BITS as usize
		}
		else
		{
			self.capacity.checked_shl(1).ok_or(AllocError)?
		};
		let (new_layout,new_bitmap_offset)=Self::allocation_layout(new_capacity)?;
		// Check if we're going to reallocate.
		let old_layout=if self.capacity!=0
		{
			Some(Self::allocation_layout(self.capacity)?.0)
		}
		else
		{
			None
		};
		let old_bitmap_offset=if self.capacity!=0
		{
			Some(Self::allocation_layout(self.capacity)?.1)
		}
		else
		{
			None
		};
		let old_bitmap_len=if self.capacity!=0
		{
			Some(Self::bitmap_words(self.capacity)*size_of::<usize>())
		}
		else
		{
			None
		};
		// Perform reallocation.
		let new_bitmap_len=Self::bitmap_words(new_capacity)*size_of::<usize>();
		let new_base=unsafe
		{
			match old_layout
			{
				Some(old_layout)=>alloc::alloc::realloc(self.base.cast(),old_layout,new_layout.size()),
				None=>alloc::alloc::alloc_zeroed(new_layout)
			}
		};
		if new_base.is_null()
		{
			return Err(AllocError);
		}
		if let (Some(old_bitmap_offset),Some(old_bitmap_len))=(old_bitmap_offset,old_bitmap_len)
		{
			// If we performed reallocation, we need to take care of the bitmap.
			unsafe
			{
				// Copy the bitmap.
				core::ptr::copy(new_base.cast::<u8>().add(old_bitmap_offset),new_base.cast::<u8>().add(new_bitmap_offset),old_bitmap_len);
				// Fill the new bitmap area with zeroes.
				core::ptr::write_bytes(new_base.cast::<u8>().add(new_bitmap_offset+old_bitmap_len),0,new_bitmap_len-old_bitmap_len);
			}
		}
		self.base=new_base.cast();
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
		let bitmap=match self.bitmap_slice_mut()
		{
			Some(bitmap)=>bitmap,
			None=>return Err(AllocError)
		};
		let handle=match bitmap.search_cleared_forward()
		{
			Some(handle)=>handle,
			None=>
			{
				self.try_reserve()?;
				return self.create_handle(data);
			}
		};
		if bitmap.set(handle).unwrap_or(true)
		{
			return Err(AllocError);
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
		let bitmap=self.bitmap_slice_mut()?;
		if !bitmap.reset(handle).unwrap_or(false)
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
		let bitmap=self.bitmap_slice()?;
		if !bitmap.test(handle).unwrap_or(false)
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
		let bitmap=self.bitmap_slice_mut()?;
		if !bitmap.test(handle).unwrap_or(false)
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
		let bitmap=match self.bitmap_slice()
		{
			Some(bitmap)=>bitmap,
			None=>return
		};
		for handle in 0..self.capacity
		{
			if bitmap.test(handle).unwrap_or(false)
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

	#[test] fn capacity_expansion()
	{
		let mut ht:HandleTable<u32>=HandleTable::new();
		for i in 0..=usize::BITS
		{
			assert_eq!(ht.create_handle(i),Ok(i as usize));
		}
		assert_eq!(ht.capacity,(usize::BITS<<1) as usize);
		let bmp=ht.bitmap_slice().unwrap();
		for i in 0..=usize::BITS
		{
			assert_eq!(bmp.test(i as usize),Ok(true));
		}
		for i in usize::BITS+1..usize::BITS<<1
		{
			assert_eq!(bmp.test(i as usize),Ok(false));
		}
	}
}
