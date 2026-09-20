// NoirVisor CVM virtual machine scheduler

use core::{ffi::c_void, hint::cold_path, ptr, slice};

use alloc::{alloc::{AllocError, Layout}, sync::Arc, vec::Vec};
use htable::HandleTable;
use log::*;
use nvcvm::{hvcall::*, interface::{CvmMappingFlags, CvmRequestedEvent, Vpcb}, status::{Status, unwrap_status}};

use crate::{hvcall::*, platform::{sync::{Mutex, RwLock}, kmap::{Kmap, KmapIter, UniversalPage}}};

struct HvaRangeManager(Vec<Kmap>);

unsafe impl Send for HvaRangeManager {}
unsafe impl Sync for HvaRangeManager {}

impl HvaRangeManager
{
	fn new()->Self
	{
		Self(Vec::new())
	}

	/// Add a range into the manager.
	/// 
	/// This method enumerates the existing HVA ranges and:
	/// - If the whole range is not listed, create one `Kmap` object and add it into the list.
	/// - If the range overlaps with existing ranges, the created `Kmap` objects must not overlap with existing `Kmap`s.
	/// - If the range fully overlaps with existing ranges, no `Kmap` objects will be created.
	/// 
	/// The list should maintain sorted by HVA.
	fn insert(&mut self,hva:*mut c_void,size:usize)->Result<(),AllocError>
	{
		if size==0
		{
			return Ok(());
		}
		let start=hva as usize;
		let end=start.checked_add(size).ok_or(AllocError)?;
		let mut gaps:Vec<(usize,usize)>=Vec::new();
		let mut cursor=start;
		for kmap in &self.0
		{
			let kstart=kmap.uva() as usize;
			let kend=kstart.checked_add(kmap.size()).ok_or(AllocError)?;
			if kend<=start
			{
				continue;
			}
			if kstart>=end
			{
				break;
			}
			if cursor<kstart
			{
				let gap_start=cursor;
				let gap_end=kstart.min(end);
				let gap_size=gap_end.checked_sub(gap_start).ok_or(AllocError)?;
				if gap_size!=0
				{
					gaps.push((gap_start,gap_size));
				}
			}
			cursor=kend.max(cursor);
		}
		if cursor<end
		{
			let gap_size=end.checked_sub(cursor).ok_or(AllocError)?;
			if gap_size!=0
			{
				gaps.push((cursor,gap_size));
			}
		}
		if gaps.is_empty()
		{
			return Ok(());
		}
		let mut created=Vec::with_capacity(gaps.len());
		for (gap_start,gap_size) in gaps
		{
			match Kmap::new(gap_start as u64,gap_size)
			{
				Ok(kmap)=>created.push(kmap),
				Err(_)=>return Err(AllocError)
			}
		}
		self.0.extend(created);
		self.0.sort_unstable_by_key(|kmap| kmap.uva() as usize);
		Ok(())
	}

	/// Returns an iterator that enumerates all ranges involved in the range of `hva` and `size`.
	/// 
	/// It would binary-search the list then check if all pages are covered in the HVA range manager. \
	/// If the range of `hva` and `size` is not fully covered in the manager, returns `None`.
	fn partial_iter(&self,hva:*mut c_void,size:usize)->Option<HvaRangePartialIter<'_>>
	{
		let start=hva as usize;
		let end=start.checked_add(size)?;
		HvaRangePartialIter::new(&self.0,start,end)
	}

	// Note: this routine should only be used for test/debug purposes.
	#[allow(dead_code)]
	fn as_slice(&self)->&[Kmap]
	{
		&self.0
	}
}

struct HvaRangePartialIter<'a>
{
	ranges:&'a [Kmap],
	index:usize,
	cursor:usize,
	end:usize,
	current_end:usize,
	iter:Option<KmapIter<'a>>
}

impl<'a> HvaRangePartialIter<'a>
{
	fn new(ranges:&'a [Kmap],start:usize,end:usize)->Option<Self>
	{
		if start>=end
		{
			return Some(Self{ranges,index:0,cursor:start,end,current_end:start,iter:None});
		}
		let mut index=ranges.partition_point(|kmap|
		{
			let kstart=kmap.uva() as usize;
			let kend=kstart.saturating_add(kmap.size());
			kend<=start
		});
		while index<ranges.len()
		{
			let kmap=&ranges[index];
			let kstart=kmap.uva() as usize;
			let kend=kstart.checked_add(kmap.size())?;
			if kend<=start
			{
				index+=1;
				continue;
			}
			if kstart>start
			{
				return None;
			}
			let mut iter=kmap.iter();
			let page_offset=(start-kstart)>>12;
			for _ in 0..page_offset
			{
				iter.next()?;
			}
			return Some(Self{ranges,index,cursor:start,end,current_end:kend.min(end),iter:Some(iter)});
		}
		None
	}
}

impl Iterator for HvaRangePartialIter<'_>
{
	type Item=u64;

	fn next(&mut self)->Option<Self::Item>
	{
		loop
		{
			if self.cursor>=self.end
			{
				return None;
			}
			if let Some(iter)=self.iter.as_mut()
			{
				match iter.next()
				{
					Some(page)=>
					{
						self.cursor+=0x1000;
						if self.cursor>=self.current_end
						{
							self.iter=None;
							self.index+=1;
						}
						return Some(page);
					}
					None=>
					{
						self.iter=None;
						self.index+=1;
					}
				}
			}
			let mut index=self.index;
			while index<self.ranges.len()
			{
				let kmap=&self.ranges[index];
				let kstart=kmap.uva() as usize;
				let kend=kstart.checked_add(kmap.size())?;
				if kend<=self.cursor
				{
					index+=1;
					continue;
				}
				if kstart>self.cursor
				{
					return None;
				}
				let mut iter=kmap.iter();
				let page_offset=(self.cursor-kstart)>>12;
				for _ in 0..page_offset
				{
					iter.next()?;
				}
				self.index=index;
				self.current_end=kend.min(self.end);
				self.iter=Some(iter);
				break;
			}
			self.iter.as_ref()?;
		}
	}
}

pub struct VirtualMachine
{
	pub(crate) handle:u32,
	hva_ranges:HvaRangeManager,
	process_id:u32,
	vcpus:Vec<Option<Arc<Mutex<VirtualProcessor>>>>
}

impl VirtualMachine
{
	const VCPU_PER_VM_LIMIT:u32=255;

	pub unsafe fn init(&mut self,handle:u32,process_id:u32)
	{
		self.handle=handle;
		self.process_id=process_id;
		unsafe
		{
			ptr::write(&raw mut self.vcpus,Vec::with_capacity(Self::VCPU_PER_VM_LIMIT as usize));
			ptr::write(&raw mut self.hva_ranges,HvaRangeManager::new());
		}
	}

	pub fn create_vcpu(&mut self,vcpu_id:u32)->Result<*mut Vpcb,Status>
	{
		if vcpu_id>Self::VCPU_PER_VM_LIMIT
		{
			return Err(Status::ACCESS_DENIED);
		}
		let i=vcpu_id as usize;
		while self.vcpus.len()<=i
		{
			self.vcpus.push(None);
		}
		if self.vcpus[i].is_some()
		{
			Err(Status::VCPU_ALREADY_CREATED)
		}
		else
		{
			let arc_vp:Arc<Mutex<VirtualProcessor>>=unsafe{Arc::try_new_uninit()?.assume_init()};
			if unsafe{arc_vp.init()}
			{
				let (vpcb_hpa,vpcb_uva)=
				{
					let mut vp=arc_vp.lock();
					unsafe
					{
						ptr::write(&raw mut vp.vpcb,UniversalPage::new()?);
					}
					vp.vcpu_id=vcpu_id;
					(vp.vpcb.hpa(),vp.vpcb.uva())
					// Mutex must be dropped before doing hypercall.
				};
				let mut ctxt=CvmHypercallCreateVcpuContext
				{
					handle:self.handle,
					vcpu_id,
					vpcb_hpa
				};
				unsafe
				{
					hypercall(CVM_HYPERCALL_CREATE_VCPU,&raw mut ctxt,size_of::<CvmHypercallCreateVcpuContext>())?;
				}
				self.vcpus[i]=Some(arc_vp);
				Ok(vpcb_uva)
			}
			else
			{
				cold_path();
				error!("Failed to initialize vCPU Mutex!");
				Err(Status::UNSUCCESSFUL)
			}
		}
	}

	pub fn delete_vcpu(&mut self,vcpu_id:u32)->Status
	{
		type Context=CvmHypercallDeleteVcpuContext;
		if vcpu_id>Self::VCPU_PER_VM_LIMIT
		{
			return Status::ACCESS_DENIED;
		}
		let mut ctxt=Context
		{
			handle:self.handle,
			vcpu_id
		};
		if let Err(st)=unsafe{hypercall(CVM_HYPERCALL_DELETE_VCPU,&raw mut ctxt,size_of::<Context>())}
		{
			error!("Failed to delete vCPU in hypervisor! Reason: {st}");
		}
		let i=vcpu_id as usize;
		match self.vcpus.get_mut(i)
		{
			Some(vp)=>
			{
				*vp=None;
				Status::SUCCESS
			}
			_=>Status::VCPU_NOT_EXIST
		}
	}

	pub fn set_mapping(&mut self,as_id:u32,gpa:u64,hva:u64,pages:usize,flags:CvmMappingFlags)->Status
	{
		type Context=CvmHypercallSetMappingContext;
		let size=size_of::<Context>()+(pages<<3);
		let layout=unsafe{Layout::from_size_align_unchecked(size,align_of::<Context>())};
		if self.hva_ranges.insert(hva as *mut c_void,pages<<12).is_err()
		{
			return Status::INSUFFICIENT_RESOURCES;
		}
		let ctxt:*mut Context=unsafe{alloc::alloc::alloc(layout).cast()};
		let st=
		{
			let ctxt=if ctxt.is_null()
			{
				return Status::INSUFFICIENT_RESOURCES;
			}
			else
			{
				unsafe
				{
					&mut *ctxt
				}
			};
			ctxt.handle=self.handle;
			ctxt.flags=flags;
			ctxt.as_id=as_id;
			ctxt.pages=pages as u32;
			ctxt.gpa=gpa;
			let hpa_list=unsafe{slice::from_raw_parts_mut(ctxt.hpa.as_mut_ptr(),pages)};
			for (i,p) in self.hva_ranges.partial_iter(hva as *mut c_void,pages<<12).unwrap().enumerate()
			{
				info!("Map page {i} with physical address: 0x{p:X}");
				hpa_list[i]=p;
			}
			// TODO: iterate the HPAs.
			unsafe
			{
				hypercall(CVM_HYPERCALL_SET_MAPPING,ctxt,size)
			}
		};
		unsafe
		{
			alloc::alloc::dealloc(ctxt.cast(),layout);
		}
		unwrap_status(st)
	}

	pub fn run_vcpu(&self,vcpu_id:u32)->Status
	{
		let i=vcpu_id as usize;
		match self.vcpus.get(i)
		{
			Some(Some(vp))=>
			{
				// Lock must be exclusively acquired even though we do not access the internal contents.
				let _lk=vp.lock();
				let mut ctxt=CvmHypercallRunVcpuContext
				{
					handle:self.handle,
					vcpu_id
				};
				unsafe
				{
					unwrap_status(hypercall(CVM_HYPERCALL_RUN_VCPU,&raw mut ctxt,size_of::<CvmHypercallRunVcpuContext>()))
				}
			}
			_=>Status::VCPU_NOT_EXIST
		}
	}

	pub fn request_event(&self,vcpu_id:u32,event:CvmRequestedEvent)->Status
	{
		let i=vcpu_id as usize;
		match self.vcpus.get(i)
		{
			Some(Some(vp))=>
			{
				let _lk=vp.lock();
				let mut ctxt=CvmHypercallRequestEventContext
				{
					handle:self.handle,
					vcpu_id,
					event
				};
				unsafe
				{
					unwrap_status(hypercall(CVM_HYPERCALL_REQUEST_EVENT,&raw mut ctxt,size_of::<CvmHypercallRequestEventContext>()))
				}
			}
			_=>Status::VCPU_NOT_EXIST
		}
	}
}

pub static VM_LIST:RwLock<HandleTable<Arc<RwLock<VirtualMachine>>>>=RwLock::new(HandleTable::new());

#[unsafe(no_mangle)] extern "system" fn noir_notify_process_termination(process_id:u32)
{
	// Enumerate VM List and delete all VMs created by the process.
	let mut lk=VM_LIST.write();
	let mut v:Vec<usize>=Vec::new();
	for (h,vm) in lk.iter_mut()
	{
		let vm=vm.read();
		if vm.process_id==process_id
		{
			v.push(h);
		}
	}
	for h in v
	{
		trace!("Terminating VM with Handle {h}...");
		lk.delete_handle(h);
	}
}

#[allow(dead_code)]
pub struct VirtualProcessor
{
	vpcb:UniversalPage,
	vcpu_id:u32
}

impl VirtualProcessor
{
	
}

#[cfg(test)] mod tests
{
    use core::{ffi::c_void, ptr::null_mut};
	use alloc::vec::Vec;

	use crate::vmm::HvaRangeManager;

	#[test] fn new_range_overlap_on_left()
	{
		let mut mgr=HvaRangeManager::new();
		// Create the first range that covers 0x1000..0x2000.
		assert!(mgr.insert(0x1000 as *mut c_void,0x1000).is_ok());
		// Create the second range that covers 0x0..0x2000.
		// Note that this should create a Kmap that covers 0x0..0x1000 instead due to the overlap.
		assert!(mgr.insert(null_mut(),0x2000).is_ok());
		let v:Vec<u64>=mgr.partial_iter(null_mut(),0x2000).unwrap().collect();
		assert_eq!(v.as_slice(),&[0,0x1000]);
		let s=mgr.as_slice();
		assert_eq!(s.len(),2);
		assert_eq!((s[0].uva(),s[0].size()),(null_mut(),0x1000));
		assert_eq!((s[1].uva(),s[1].size()),(0x1000 as *mut c_void,0x1000));
	}

	#[test] fn new_range_overlap_on_right()
	{
		let mut mgr=HvaRangeManager::new();
		// Create the first range that covers 0x0..0x2000.
		assert!(mgr.insert(null_mut(),0x2000).is_ok());
		// Create the second range that covers 0x1000..0x2000.
		// Note that this should create a Kmap that covers 0x1000..0x2000 instead due to the overlap.
		assert!(mgr.insert(0x1000 as *mut c_void,0x2000).is_ok());
		let v:Vec<u64>=mgr.partial_iter(null_mut(),0x3000).unwrap().collect();
		assert_eq!(v.as_slice(),&[0,0x1000,0x2000]);
		let s=mgr.as_slice();
		assert_eq!(s.len(),2);
		assert_eq!((s[0].uva(),s[0].size()),(null_mut(),0x2000));
		assert_eq!((s[1].uva(),s[1].size()),(0x2000 as *mut c_void,0x1000));
	}

	#[test] fn new_range_overlap_in_middle()
	{
		let mut mgr=HvaRangeManager::new();
		// Create the first range that covers 0x1000..0x2000.
		assert!(mgr.insert(0x1000 as *mut c_void,0x1000).is_ok());
		// Create the second range that covers 0x0..0x3000.
		// Note that this should create two Kmap that cover 0x0..0x1000 and 0x2000..0x3000 due to the overlap.
		assert!(mgr.insert(null_mut(),0x3000).is_ok());
		let v:Vec<u64>=mgr.partial_iter(null_mut(),0x3000).unwrap().collect();
		assert_eq!(v.as_slice(),&[0,0x1000,0x2000]);
		let s=mgr.as_slice();
		assert_eq!(s.len(),3);
		assert_eq!((s[0].uva(),s[0].size()),(null_mut(),0x1000));
		assert_eq!((s[1].uva(),s[1].size()),(0x1000 as *mut c_void,0x1000));
		assert_eq!((s[2].uva(),s[2].size()),(0x2000 as *mut c_void,0x1000));
	}

	#[test] fn partial_iteration()
	{
		let mut mgr=HvaRangeManager::new();
		// Create an HVA range between 0x0..0x5000.
		assert!(mgr.insert(null_mut(),0x5000).is_ok());
		// Partially enumerates ranges 0x2000..0x4000.
		let v:Vec<u64>=mgr.partial_iter(0x2000 as *mut c_void,0x2000).unwrap().collect();
		// It should have pages at 0x2000 and 0x3000.
		assert_eq!(v.as_slice(),&[0x2000,0x3000]);
	}
}
