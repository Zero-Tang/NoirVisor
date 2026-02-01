#![no_main]
#![no_std]

use core::{arch::x86_64::__cpuid, mem::MaybeUninit};
use uefi::*;
use allocator::Allocator;
use boot::{image_handle, open_protocol, OpenProtocolAttributes, OpenProtocolParams, LoadImageSource};
use proto::{console::text::*,loaded_image::LoadedImage,device_path::{self,DevicePath,build::DevicePathBuilder}, BootPolicy};
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
	let r=__cpuid(0);
	vstr[0..4].copy_from_slice(&r.ebx.to_le_bytes());
	vstr[4..8].copy_from_slice(&r.edx.to_le_bytes());
	vstr[8..12].copy_from_slice(&r.ecx.to_le_bytes());
	let s=unsafe{str::from_utf8_unchecked(&vstr)};
	println!("Processor Vendor: {s}");
	for i in 0..3
	{
		let r=__cpuid(0x80000002+i as u32);
		pstr[(i<<4)..((i<<4)+4)].copy_from_slice(&r.eax.to_le_bytes());
		pstr[((i<<4)+0x4)..((i<<4)+0x8)].copy_from_slice(&r.ebx.to_le_bytes());
		pstr[((i<<4)+0x8)..((i<<4)+0xC)].copy_from_slice(&r.ecx.to_le_bytes());
		pstr[((i<<4)+0xC)..((i<<4)+0x10)].copy_from_slice(&r.edx.to_le_bytes());
	}
	let l=pstr.iter().position(|&v| v==0).unwrap();
	let s=unsafe{str::from_utf8_unchecked(&pstr[..l])};
	println!("Processor Brand Name: {s}");
}

fn load_hypervisor_driver()->Option<Handle>
{
	// Locate the loaded image protocol.
	let loaded_image=unsafe
	{
		let proto_params=OpenProtocolParams
		{
			handle:image_handle(),
			agent:image_handle(),
			controller:None
		};
		match open_protocol::<LoadedImage>(proto_params,OpenProtocolAttributes::GetProtocol)
		{
			Ok(proto)=>proto,
			Err(e)=>
			{
				println!("OpenProtocol failed! Reason: {e}");
				return None;
			}
		}
	};
	// Make Device Path Root
	let root_path=unsafe
	{
		let proto_params=OpenProtocolParams
		{
			handle:loaded_image.device().unwrap(),
			agent:image_handle(),
			controller:None
		};
		match open_protocol::<DevicePath>(proto_params,OpenProtocolAttributes::GetProtocol)
		{
			Ok(proto)=>proto,
			Err(e)=>
			{
				println!("OpenProtocol failed! Reason: {e}");
				return None;
			}
		}
	};
	let mut buf = [0u16; 256];
	let file_path=device_path::build::media::FilePath{path_name:CStr16::from_str_with_buf("\\NoirVisor.efi",&mut buf).unwrap()};
	let mut buf = [MaybeUninit::uninit(); 256];
	let x=DevicePathBuilder::with_buf(&mut buf).push(&file_path).unwrap().finalize().unwrap();
	let p=root_path.append_path(x).unwrap();
	match boot::load_image(image_handle(),LoadImageSource::FromDevicePath{device_path:&p,boot_policy:BootPolicy::default()})
	{
		Ok(h)=>Some(h),
		Err(e)=>
		{
			println!("Failed to load NoirVisor! Reason: {e}");
			None
		}
	}
}

#[allow(dead_code)]
fn test_exit_bs()->!
{
	use boot::{exit_boot_services,MemoryType};
	use runtime::{reset,ResetType};
	println!("Calling ExitBootServices... If the system didn't shutdown, then it's unexpected behavior!");
	let _=unsafe{exit_boot_services(Some(MemoryType::LOADER_DATA))};
	reset(ResetType::SHUTDOWN,Status::SUCCESS,None);
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
	match load_hypervisor_driver()
	{
		Some(h)=>
		{
			if let Err(e)=boot::start_image(h)
			{
				println!("Failed to start image! Reason: {e}");
			}
			// Uncomment the next line to test ExitBootServices Event. If successful, the machine will shutdown.
			// test_exit_bs();
		}
		None=>println!("Failed to load image!")
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