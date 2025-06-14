#![no_main]
#![no_std]

use core::{arch::x86_64::__cpuid, ffi::c_void, mem::MaybeUninit, ptr::null_mut};
use uefi::{allocator::Allocator, boot::image_handle, proto::{device_path::build::DevicePathBuilder, BootPolicy}, *};
use proto::{console::text::*,loaded_image::LoadedImage,ProtocolPointer,device_path::*};
use system::{with_stdout,with_stdin};
use table::system_table_raw;

#[global_allocator] static EFI_ALLOC:Allocator=Allocator;

fn check_keystroke(key:char,stdin:&mut Input)->bool
{
	let uk=Char16::try_from(key).unwrap();
	let mut evt=[stdin.wait_for_key_event().unwrap()];
	let _=boot::wait_for_event(&mut evt).discard_errdata();
	if let Some(k)=stdin.read_key().unwrap()
	{
		return k==Key::Printable(uk);
	}
	false
}

fn block_until_keystroke(key:char)
{
	loop
	{
		let r=with_stdin(|stdin| check_keystroke(key, stdin));
		if r {break;}
	}
}

fn print_cpu_info()
{
	let mut vstr:[u8;12]=[0;12];
	let mut pstr:[u8;0x31]=[0;0x31];
	let r=unsafe{__cpuid(0)};
	vstr[0..4].copy_from_slice(&r.ebx.to_le_bytes());
	vstr[4..8].copy_from_slice(&r.edx.to_le_bytes());
	vstr[8..12].copy_from_slice(&r.ecx.to_le_bytes());
	let s=unsafe{str::from_utf8_unchecked(&vstr)};
	println!("Processor Vendor: {s}");
	let r=unsafe{__cpuid(0x80000002)};
	pstr[0x00..0x04].copy_from_slice(&r.eax.to_le_bytes());
	pstr[0x04..0x08].copy_from_slice(&r.ebx.to_le_bytes());
	pstr[0x08..0x0C].copy_from_slice(&r.ecx.to_le_bytes());
	pstr[0x0C..0x10].copy_from_slice(&r.edx.to_le_bytes());
	let r=unsafe{__cpuid(0x80000003)};
	pstr[0x10..0x14].copy_from_slice(&r.eax.to_le_bytes());
	pstr[0x14..0x18].copy_from_slice(&r.ebx.to_le_bytes());
	pstr[0x18..0x1C].copy_from_slice(&r.ecx.to_le_bytes());
	pstr[0x1C..0x20].copy_from_slice(&r.edx.to_le_bytes());
	let r=unsafe{__cpuid(0x80000004)};
	pstr[0x20..0x24].copy_from_slice(&r.eax.to_le_bytes());
	pstr[0x24..0x28].copy_from_slice(&r.ebx.to_le_bytes());
	pstr[0x28..0x2C].copy_from_slice(&r.ecx.to_le_bytes());
	pstr[0x2C..0x30].copy_from_slice(&r.edx.to_le_bytes());
	let l=pstr.iter().position(|&v| v==0).unwrap();
	let s=unsafe{str::from_utf8_unchecked(&pstr[..l])};
	println!("Processor Brand Name: {s}");
}

fn load_hypervisor_driver()->Handle
{
	// The `gBS->HandleProtocol` is not implemented in the uefi crate.
	let handle_protocol=unsafe
	{
		let bs=system_table_raw().unwrap().as_ref().boot_services.as_ref().unwrap();
		bs.handle_protocol
	};
	// Locate the loaded image protocol.
	let loaded_image=unsafe
	{
		let mut p:*mut c_void=null_mut();
		let g=LoadedImage::GUID;
		match handle_protocol(boot::image_handle().as_ptr(),&raw const g,&raw mut p)
		{
			Status::SUCCESS=>&mut *LoadedImage::mut_ptr_from_ffi(p),
			st=>panic!("HandleProtocol failed! Status: {st}")
		}
	};
	// Make Device Path Root
	let root_path=unsafe
	{
		let mut p:*mut c_void=null_mut();
		let g=DevicePath::GUID;
		match handle_protocol(loaded_image.device().unwrap().as_ptr(),&raw const g,&raw mut p)
		{
			Status::SUCCESS=>&mut *DevicePath::mut_ptr_from_ffi(p),
			st=>panic!("HandleProtocol failed! Status: {st}")
		}
	};
	let mut buf = [0u16; 256];
	// let dev_path=DevicePathBuilder::with_buf(&mut buf).push(&build::hardware::);
	let file_path=build::media::FilePath{path_name:CStr16::from_str_with_buf("\\NoirVisor.efi",&mut buf).unwrap()};
	let mut buf = [MaybeUninit::uninit(); 256];
	let x=DevicePathBuilder::with_buf(&mut buf).push(&file_path).unwrap().finalize().unwrap();
	let p=root_path.append_path(x).unwrap();
	match boot::load_image(image_handle(),boot::LoadImageSource::FromDevicePath{device_path:&p,boot_policy:BootPolicy::default()})
	{
		Ok(h)=>h,
		Err(e)=>panic!("Failed to load NoirVisor! Reason: {e}")
	}
}

#[entry] fn main()->Status
{
	let systab=unsafe{system_table_raw().unwrap().as_ref()};
	// Print some boring initialization stuff.
	let _=with_stdout(|s| s.clear());
	println!("{}",include_str!("banner.txt"));
	println!("Welcome to NoirVisor Loader!");
	println!("Firmware Vendor: {}, Revision: {}",unsafe{CStr16::from_ptr(systab.firmware_vendor.cast())},systab.firmware_revision);
	println!("Firmware UEFI Specification: {}.{}.{}",systab.header.revision.major(),systab.header.revision.minor()/10,systab.header.revision.minor()%10);
	print_cpu_info();
	// Load the driver.
	let h=load_hypervisor_driver();
	if let Err(e)=boot::start_image(h)
	{
		println!("Failed to start image! Reason: {e}");
	}
	println!("Press Enter key to continue...");
	block_until_keystroke('\r');
	Status::SUCCESS
}

// Panic handler is required for no_std crates. But it is NOT FOR tests!
// Put it under non-test conditional-compilation, or otherwise
// the rust-analyzer of VSCode will report duplicate panic_impl.

#[cfg(not(test))]
mod panicking
{
	use uefi::println;
	use core::panic::PanicInfo;

	#[panic_handler] fn panic(panic: &PanicInfo)->!
	{
		println!("[PANIC] NoirVisor Loader {}",panic);
		loop{}
	}
}