/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file is the entry point for the NoirVisor UEFI test loader.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

#![no_main]
#![no_std]

extern crate alloc;

use alloc::vec::Vec;
use core::{arch::x86_64::__cpuid, mem::MaybeUninit, ptr::null_mut, slice, sync::atomic::Ordering};

use efi_helpers::{BS_TABLE, CStr16, EfiAllocator, IMAGE_INFO, clear_screen, handle_protocol, println};
use r_efi::{
	efi::{Boolean, Handle, LOADER_DATA, Status, SystemTable},
	protocols::device_path::{self, End, Media, TYPE_END, TYPE_MEDIA},
};

use crate::dma::test_dma;

#[cfg(target_os = "uefi")]
mod cvm;
mod dma;
#[cfg(not(feature = "skip_block"))]
mod select_boot;

#[global_allocator]
static EFI_ALLOC: EfiAllocator = EfiAllocator(LOADER_DATA);

fn print_cpu_info()
{
	let mut vstr: [u8; 12] = [0; 12];
	let mut pstr: [u8; 0x30] = [0; 0x30];
	let r = __cpuid(0);
	vstr[0..4].copy_from_slice(&r.ebx.to_le_bytes());
	vstr[4..8].copy_from_slice(&r.edx.to_le_bytes());
	vstr[8..12].copy_from_slice(&r.ecx.to_le_bytes());
	let s = unsafe { str::from_utf8_unchecked(&vstr) };
	println!("Processor Vendor: {s}");
	for i in 0..3
	{
		let r = __cpuid(0x80000002 + i as u32);
		pstr[(i << 4)..((i << 4) + 4)].copy_from_slice(&r.eax.to_le_bytes());
		pstr[((i << 4) + 0x4)..((i << 4) + 0x8)].copy_from_slice(&r.ebx.to_le_bytes());
		pstr[((i << 4) + 0x8)..((i << 4) + 0xC)].copy_from_slice(&r.ecx.to_le_bytes());
		pstr[((i << 4) + 0xC)..((i << 4) + 0x10)].copy_from_slice(&r.edx.to_le_bytes());
	}
	let l = pstr.iter().position(|&v| v == 0).unwrap_or(pstr.len());
	let s = unsafe { str::from_utf8_unchecked(&pstr[..l]) };
	println!("Processor Brand Name: {s}");
}

fn load_hypervisor_driver(source_image: Handle, file_path: &str) -> Option<Handle>
{
	// Locate the loaded image protocol.
	let loaded_image = unsafe { &*IMAGE_INFO.load(Ordering::Relaxed) };
	// Get Device Path Root
	let root_path: *mut device_path::Protocol = handle_protocol(loaded_image.device_handle, device_path::PROTOCOL_GUID).unwrap();
	// Build Path.
	let mut full_path_raw: Vec<u8> = Vec::new();
	// Copy Root Device Path.
	let mut cur_node = unsafe { &*root_path };
	while cur_node.r#type != TYPE_END
	{
		let len = u16::from_ne_bytes(cur_node.length) as usize;
		let p = &raw const *cur_node;
		full_path_raw.extend_from_slice(unsafe { slice::from_raw_parts(p.cast(), len) });
		cur_node = unsafe { &*p.byte_add(len) };
	}
	// Append File Path into Root Device Path.
	let mut file_name_len: usize = 4;
	full_path_raw.push(TYPE_MEDIA);
	full_path_raw.push(Media::SUBTYPE_FILE_PATH);
	full_path_raw.extend_from_slice(&0u16.to_ne_bytes());
	for c in file_path.encode_utf16()
	{
		full_path_raw.extend_from_slice(&c.to_ne_bytes());
		file_name_len += 2;
	}
	// Must be null-terminated.
	full_path_raw.extend_from_slice(&0u16.to_ne_bytes());
	file_name_len += 2;
	// Set the size.
	unsafe {
		let fn_len: *mut u16 = full_path_raw.as_mut_ptr().byte_add(full_path_raw.len() - file_name_len).cast();
		*fn_len.add(1) = file_name_len as u16;
	}
	// Terminate the file path.
	full_path_raw.push(TYPE_END);
	full_path_raw.push(End::SUBTYPE_ENTIRE);
	full_path_raw.extend_from_slice(&4u16.to_ne_bytes());
	unsafe {
		let mut handle: MaybeUninit<Handle> = MaybeUninit::uninit();
		let bs = &*BS_TABLE.load(Ordering::Relaxed);
		let st = (bs.load_image)(Boolean::FALSE, source_image, full_path_raw.as_mut_ptr().cast(), null_mut(), 0, handle.as_mut_ptr());
		if st.is_error()
		{
			println!("Failed to load image! Reason: {st}");
			None
		}
		else
		{
			Some(handle.assume_init())
		}
	}
}

#[unsafe(no_mangle)]
unsafe extern "efiapi" fn uefi_entry(image_handle: Handle, system_table: *mut SystemTable) -> Status
{
	let systab = unsafe { &*system_table };
	unsafe {
		efi_helpers::init(image_handle, system_table);
	}
	// Print some boring initialization stuff.
	clear_screen();
	println!("{}", include_str!("banner.txt"));
	println!("Welcome to NoirVisor Loader!");
	println!(
		"Firmware Vendor: {}, Revision: {}",
		unsafe { CStr16::from_ptr(systab.firmware_vendor.cast()) },
		systab.firmware_revision
	);
	println!(
		"Firmware UEFI Specification: {}.{}.{}",
		systab.hdr.revision >> 16,
		(systab.hdr.revision & 0xffff) / 10,
		(systab.hdr.revision & 0xffff) % 10
	);
	print_cpu_info();
	// Load the driver.
	let start_image = unsafe { (*BS_TABLE.load(Ordering::Relaxed)).start_image };
	let hv_img_handle: Handle = match load_hypervisor_driver(image_handle, "\\NoirVisor.efi")
	{
		Some(h) =>
		{
			let st = unsafe { start_image(h, null_mut(), null_mut()) };
			if st.is_error()
			{
				println!("Failed to start hypervisor image! Reason: {st}");
			}
			h
		}
		None =>
		{
			println!("Failed to load hypervisor image!");
			null_mut()
		}
	};
	test_dma(hv_img_handle);
	match load_hypervisor_driver(image_handle, "\\cvsched.efi")
	{
		Some(h) =>
		{
			let st = unsafe { start_image(h, null_mut(), null_mut()) };
			if st.is_error()
			{
				println!("Failed to start scheduler image! Reason: {st}");
			}
			else
			{
				println!("NoirVisor CVM Scheduler image is successfully started!");
				#[cfg(target_os = "uefi")]
				unsafe {
					let st = nvcvm::uefi::init(system_table);
					if st.is_error()
					{
						println!("Failed to initialize nvcvm crate! Status=0x{:X}", st.as_usize());
					}
					else
					{
						cvm::test_cvm();
					}
				}
				let st = unsafe { ((*BS_TABLE.load(Ordering::Relaxed)).unload_image)(h) };
				if st.is_error()
				{
					println!("Failed to unload NoirVisor CVM Scheduler image! Reason: {st}");
				}
			}
		}
		None => println!("Failed to load scheduler image!"),
	}
	#[cfg(feature = "skip_block")]
	unsafe {
		efi_helpers::shutdown();
	}
	#[cfg(not(feature = "skip_block"))]
	{
		use efi_helpers::block_until_keystroke;
		println!("Press Enter key to enter boot selection. Press Space key to leave.");
		loop
		{
			match block_until_keystroke()
			{
				'\r' =>
				{
					select_boot::select_boot_option(image_handle);
					break;
				}
				' ' => break,
				_ => continue,
			}
		}
		Status::SUCCESS
	}
}

// Panic handler is required for no_std crates. But it is NOT FOR tests!
// Put it under non-test conditional-compilation, or otherwise
// the rust-analyzer of VSCode will report duplicate panic_impl.

#[cfg(not(test))]
mod panicking
{
	use core::panic::PanicInfo;
	use efi_helpers::println;

	#[panic_handler]
	fn panic(panic: &PanicInfo) -> !
	{
		println!("[PANIC] NoirVisor Loader {}", panic);
		loop
		{}
	}
}
