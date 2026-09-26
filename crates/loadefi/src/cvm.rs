// CVM Test

#[repr(C,align(4096))] struct Page(UnsafeCell<[u8;0x1000]>);

use core::{cell::UnsafeCell, ptr::null_mut};

use nvcvm::interface::{CvmHandle, CvmMappingFlags, InterceptCode, Vpcb, c_api::*};
use efi_helpers::println;

unsafe impl Send for Page {}
unsafe impl Sync for Page {}

impl Default for Page
{
	fn default()->Self
	{
		Self::new()
	}
}

impl Page
{
	const fn as_mut_ptr(&self)->*mut u8
	{
		self.0.get().cast()
	}

	const fn new()->Self
	{
		Self(UnsafeCell::new([0xF4;size_of::<Self>()]))
	}
}

static GUEST_PAGE:Page=Page::new();

pub fn test_cvm()
{
	println!("Running CVM cases...");
	let mut cap:[u32;7]=[0;7];
	let st=unsafe{ncv_get_capability(0,cap.as_mut_ptr())};
	println!("ncv_get_capability: {st}");
	let mut vm=CvmHandle(u32::MAX);
	let st=unsafe{ncv_create_vm(&raw mut vm)};
	println!("ncv_create_vm: {st}, handle: {}",vm.0);
	let mut vpcb:*mut Vpcb=null_mut();
	let st=unsafe{ncv_create_vcpu(vm,0,&raw mut vpcb)};
	println!("ncv_create_vcpu: {st}, vpcb={vpcb:p}");
	println!("vcpu-rip=0x{:X}",unsafe{(*vpcb).sync_regs.rip});
	let st=unsafe{ncv_set_mapping(vm,0xFFFFF000,GUEST_PAGE.as_mut_ptr().cast(),1,0,CvmMappingFlags::new().with_read(true).with_write(true).with_execute(true))};
	println!("ncv_set_mapping: {st}");
	let mut keep_running=true;
	let mut hlt_count:usize=0;
	/*let guest_mem:&mut [u8;size_of::<Page>()]=unsafe{&mut *GUEST_PAGE.as_mut_ptr().cast()};
	guest_mem[0xFF0]=0xEB;
	guest_mem[0xFF1]=0xFE;*/
	while keep_running
	{
		let st=unsafe{ncv_run_vcpu(vm,0)};
		assert_eq!(st.0,0);
		let vpcb=unsafe{&mut *vpcb};
		match vpcb.intercept_code
		{
			InterceptCode::SCHEDULER_EXIT=>continue,
			InterceptCode::HLT_INSTRUCTION=>
			{
				vpcb.sync_regs.rip=vpcb.exit_context.next_rip;
				vpcb.sync_flags.set_gpr(true);
				hlt_count+=1;
				if hlt_count==5
				{
					keep_running=false;
				}
			}
			code=>
			{
				println!("Unexpected Interception-Code: 0x{:X}!",code.0);
				keep_running=false;
			}
		}
	}
}