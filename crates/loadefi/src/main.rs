#![no_main]
#![no_std]

extern crate alloc;

use core::{arch::x86_64::__cpuid, char, mem::MaybeUninit, slice};
use alloc::{string::String, vec::Vec};

use uefi::{runtime::VariableVendor, *};
use allocator::Allocator;
use boot::{image_handle, open_protocol, OpenProtocolAttributes, OpenProtocolParams, LoadImageSource};
use proto::{console::text::*,loaded_image::LoadedImage,device_path::{self,DevicePath,build::DevicePathBuilder}, BootPolicy};
use system::with_stdout;
use table::system_table_raw;

#[global_allocator] static EFI_ALLOC:Allocator=Allocator;

fn wait_for_keystroke()->char
{
	uefi::system::with_stdin(|stdin|
	{
		loop
		{
			if let Ok(o)=stdin.read_key() && let Some(k)=o && let Key::Printable(p)=k
			{
				return p.into();
			}
		}
	})
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

// EFI_LOAD_OPTION is not implemented in uefi crate.
#[repr(C,packed)] struct BootOption
{
	attributes:u32,
	file_path_list_length:u16,
	// description:[u16],
	// file_path_list:[DevicePathProtocol],
	// optional_data:[u8]
}

impl BootOption
{
	fn get_description(&self,limit:usize)->String
	{
		let p:*const u16=unsafe{(&raw const *self).byte_add(size_of::<Self>()).cast()};
		let max_buff=unsafe{slice::from_raw_parts(p,(limit-size_of::<Self>())>>1)};
		let len=max_buff.iter().position(|v| *v==0).unwrap_or(max_buff.len());
		String::from_utf16_lossy(&max_buff[..len])
	}

	fn get_optional_data(&self,limit:usize)->Vec<u8>
	{
		let p:*const u16=unsafe{(&raw const *self).byte_add(size_of::<Self>()).cast()};
		let max_buff=unsafe{slice::from_raw_parts(p,(limit-size_of::<Self>())>>1)};
		let len=max_buff.iter().position(|v| *v==0).unwrap_or(max_buff.len());
		unsafe
		{
			let q:*const u8=p.add(len+1).byte_add(self.file_path_list_length as usize).cast();
			slice::from_raw_parts(q,limit-q.offset_from_unsigned((&raw const *self).cast())).to_vec()
		}
	}

	fn get_device_path(&self,limit:usize)->Vec<u8>
	{
		let p:*const u16=unsafe{(&raw const *self).byte_add(size_of::<Self>()).cast()};
		let max_buff=unsafe{slice::from_raw_parts(p,(limit-size_of::<Self>())>>1)};
		let len=max_buff.iter().position(|v| *v==0).unwrap_or(max_buff.len());
		unsafe
		{
			let q:*const u8=p.add(len+1).cast();
			slice::from_raw_parts(q,self.file_path_list_length as usize).to_vec()
		}
	}
}

#[allow(dead_code)]
struct LoadOption
{
	name:String,
	description:String,
	device_path:Vec<u8>,
	optional_data:Vec<u8>
}

fn select_boot_option()
{
	let mut name_buff_raw:MaybeUninit<[u16;100]>=MaybeUninit::uninit();
	let name_buff=unsafe{name_buff_raw.assume_init_mut()}.as_mut_slice();
	name_buff[0]=0;
	let mut vendor=VariableVendor::GLOBAL_VARIABLE;
	let mut options:Vec<LoadOption>=Vec::new();
	loop
	{
		match runtime::get_next_variable_key(name_buff,&mut vendor)
		{
			Ok(_)=>
			{
				let len=name_buff.iter().position(|v| *v==0).unwrap_or(name_buff.len());
				let name=String::from_utf16_lossy(&name_buff[..len]);
				if name.starts_with("Boot") && name.len()==8
				{
					match runtime::get_variable_boxed(unsafe{CStr16::from_u16_with_nul_unchecked(name_buff)},&vendor)
					{
						Ok((raw,_attrib))=>
						{
							let boot_opt:&BootOption=unsafe{&*raw.as_ptr().cast()};
							let description=boot_opt.get_description(raw.len());
							let device_path=boot_opt.get_device_path(raw.len());
							let optional_data=boot_opt.get_optional_data(raw.len());
							options.push(LoadOption{name,description,device_path,optional_data});
						}
						Err(e)=>println!("Failed to GetVariable for {name}! Reason: {e}")
					}
				}
			}
			Err(e)=>
			{
				if e.status()!=Status::NOT_FOUND
				{
					println!("GetNextVariable failed! Reason: {e}");
				}
				break;
			}
		}
	}
	let mut position:usize=0;
	let reprinter_fn=|pos:usize|
	{
		let _=with_stdout(|s| s.clear());
		println!("Select the next boot option. Press Enter key to confirm selection.");
		println!("Press W to move cursor upward. Press S to move cursor downward.");
		for (i,opt) in options.iter().enumerate()
		{
			println!("{} {}: {} (Device-Path: {:02X?}",if i==pos {"----> "} else {""},opt.name,opt.description,opt.device_path);
		}
	};
	reprinter_fn(position);
	loop
	{
		let key=wait_for_keystroke();
		match key
		{
			'w'|'W'=>if position==0
			{
				position=options.len()-1;
			}
			else
			{
				position-=1;
			}
			's'|'S'=>if position<options.len()-1
			{
				position+=1;
			}
			else
			{
				position=0;
			}
			'\r'=>
			{
				// Load the boot option.
				let p:&DevicePath=unsafe{DevicePath::from_ffi_ptr(options[position].device_path.as_ptr().cast())};
				match boot::load_image(image_handle(),LoadImageSource::FromDevicePath{device_path:p,boot_policy:BootPolicy::ExactMatch})
				{
					Ok(h)=>
					{
						if let Err(e)=boot::start_image(h)
						{
							println!("Failed to start image! Reason: {e}");
						}
					}
					Err(e)=>println!("Failed to load image! Reason: {e}")
				}
				break;
			}
			_=>continue
		}
		reprinter_fn(position);
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
	println!("Press Enter key to enter boot selection. Press Space key to leave.");
	loop
	{
		match wait_for_keystroke()
		{
			'\r'=>select_boot_option(),
			' '=>break,
			_=>continue
		}
	}
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