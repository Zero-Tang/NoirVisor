/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file selects boot options.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::{
	fmt::{self, Write},
	ptr::null_mut,
	slice,
	sync::atomic::Ordering,
};

use alloc::{boxed::Box, string::String, vec::Vec};
use efi_helpers::{
	BS_TABLE, RT_TABLE, block_until_keystroke2, clear_screen, convert_device_path_to_text, handle_protocol, print, println,
};
use r_efi::{
	efi::{BY_PROTOCOL, Boolean, GLOBAL_VARIABLE, Handle, Status},
	protocols::{
		device_path::{self, Media, TYPE_END, TYPE_MEDIA},
		loaded_image,
	},
};

use crate::EFI_ALLOC;

// EFI_LOAD_OPTION is not implemented in uefi crate.
#[repr(C, packed)]
struct BootOption
{
	attributes: u32,
	file_path_list_length: u16,
	// description:[u16],
	// file_path_list:[DevicePathProtocol],
	// optional_data:[u8]
}

impl BootOption
{
	fn get_description(&self, limit: usize) -> String
	{
		let p: *const u16 = unsafe { (&raw const *self).byte_add(size_of::<Self>()).cast() };
		let max_buff = unsafe { slice::from_raw_parts(p, (limit - size_of::<Self>()) >> 1) };
		let len = max_buff.iter().position(|v| *v == 0).unwrap_or(max_buff.len());
		String::from_utf16_lossy(&max_buff[..len])
	}

	fn get_optional_data(&self, limit: usize) -> Vec<u8>
	{
		let p: *const u16 = unsafe { (&raw const *self).byte_add(size_of::<Self>()).cast() };
		let max_buff = unsafe { slice::from_raw_parts(p, (limit - size_of::<Self>()) >> 1) };
		let len = max_buff.iter().position(|v| *v == 0).unwrap_or(max_buff.len());
		unsafe {
			let q: *const u8 = p.add(len + 1).byte_add(self.file_path_list_length as usize).cast();
			slice::from_raw_parts(q, limit - q.offset_from_unsigned((&raw const *self).cast())).to_vec()
		}
	}

	fn get_device_path(&self, limit: usize) -> Vec<u8>
	{
		let p: *const u16 = unsafe { (&raw const *self).byte_add(size_of::<Self>()).cast() };
		let max_buff = unsafe { slice::from_raw_parts(p, (limit - size_of::<Self>()) >> 1) };
		let len = max_buff.iter().position(|v| *v == 0).unwrap_or(max_buff.len());
		unsafe {
			let q: *const u8 = p.add(len + 1).cast();
			slice::from_raw_parts(q, self.file_path_list_length as usize).to_vec()
		}
	}
}

fn device_path_size(path: &[u8]) -> Option<usize>
{
	let mut offset = 0;
	while offset + 4 <= path.len()
	{
		let node_len = u16::from_ne_bytes([path[offset + 2], path[offset + 3]]) as usize;
		if node_len < 4 || offset + node_len > path.len()
		{
			return None;
		}
		if path[offset] == TYPE_END
		{
			return Some(offset + node_len);
		}
		offset += node_len;
	}
	None
}

fn expand_short_device_path(path: Vec<u8>) -> Vec<u8>
{
	let Some(path_len) = device_path_size(&path)
	else
	{
		return path;
	};
	let path = &path[..path_len];
	// Boot options may contain a short-form path such as HD(...)/File(...).
	// Find the media suffix on a handle path so its PCI/SATA ancestry can be restored.
	let mut short_len = 0;
	while short_len + 4 <= path.len()
	{
		if path[short_len] == TYPE_MEDIA && path[short_len + 1] == Media::SUBTYPE_FILE_PATH
		{
			break;
		}
		let node_len = u16::from_ne_bytes([path[short_len + 2], path[short_len + 3]]) as usize;
		if node_len < 4 || short_len + node_len > path.len() || path[short_len] == TYPE_END
		{
			return path.to_vec();
		}
		short_len += node_len;
	}
	if short_len == 0 || short_len >= path.len()
	{
		return path.to_vec();
	}

	unsafe {
		let bs = &*BS_TABLE.load(Ordering::Relaxed);
		let mut guid = device_path::PROTOCOL_GUID;
		let mut handle_count = 0;
		let mut handles: *mut Handle = null_mut();
		let st = (bs.locate_handle_buffer)(BY_PROTOCOL, &raw mut guid, null_mut(), &raw mut handle_count, &raw mut handles);
		if st.is_error()
		{
			return path.to_vec();
		}
		if handle_count == 0
		{
			return path.to_vec();
		}
		let handle_slice = slice::from_raw_parts(handles, handle_count);
		for &handle in handle_slice
		{
			let Ok(handle_path) = handle_protocol::<device_path::Protocol>(handle, device_path::PROTOCOL_GUID)
			else
			{
				continue;
			};
			let mut size = 0;
			let mut node = handle_path;
			while (*node).r#type != TYPE_END
			{
				let len = u16::from_ne_bytes((*node).length) as usize;
				if len < 4
				{
					break;
				}
				size += len;
				node = (node.cast::<u8>().byte_add(len)).cast();
			}
			let end_len = u16::from_ne_bytes((*node).length) as usize;
			if size == 0 || end_len < 4
			{
				continue;
			}
			let handle_raw = slice::from_raw_parts(handle_path.cast::<u8>(), size);
			let mut offset = 0;
			while offset < size
			{
				if size - offset == short_len && handle_raw[offset..] == path[..short_len]
				{
					let mut expanded = Vec::with_capacity(offset + path.len());
					expanded.extend_from_slice(&handle_raw[..offset]);
					expanded.extend_from_slice(path);
					(bs.free_pool)(handles.cast());
					return expanded;
				}
				let len = u16::from_ne_bytes([handle_raw[offset + 2], handle_raw[offset + 3]]) as usize;
				if len < 4 || offset + len > size
				{
					break;
				}
				offset += len;
			}
		}
		(bs.free_pool)(handles.cast());
	}
	path.to_vec()
}

#[allow(dead_code)]
struct LoadOption
{
	name: String,
	description: String,
	device_path: Vec<u8>,
	optional_data: Vec<u8>,
}

impl fmt::Display for LoadOption
{
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result
	{
		let path_raw = self.device_path.as_ptr() as *mut device_path::Protocol;
		let path_text = unsafe { convert_device_path_to_text(path_raw) };
		write!(f, "[{}] {} (Path: {path_text}", self.name, self.description)?;
		if self.optional_data.is_empty()
		{
			f.write_char(')')
		}
		else
		{
			write!(f, ", Optional: 0x{:X} Bytes)", self.optional_data.len())
		}
	}
}

pub fn select_boot_option(source_image: Handle)
{
	let rt = unsafe { &*RT_TABLE.load(Ordering::Relaxed) };
	let mut guid = GLOBAL_VARIABLE;
	let mut name_raw: [u16; 0x100] = [0; 0x100];
	// Scan what boot options we have.
	let mut options: Vec<LoadOption> = Vec::new();
	loop
	{
		let mut size = name_raw.len() << 1;
		let st = unsafe { (rt.get_next_variable_name)(&raw mut size, name_raw.as_mut_ptr(), &raw mut guid) };
		if st == Status::NOT_FOUND
		{
			break;
		}
		else if st != Status::SUCCESS
		{
			panic!("GetNextVariableName failed! Reason: {st}, Buffer-Size: {size}, GUID: {guid:X?}");
		}
		let name = String::from_utf16_lossy(&name_raw[..(size >> 1) - 1]);
		if name.starts_with("Boot") && name.len() == 8
		{
			let mut data_size: usize = 0;
			let st = unsafe { (rt.get_variable)(name_raw.as_mut_ptr(), &raw mut guid, null_mut(), &raw mut data_size, null_mut()) };
			assert_eq!(st, Status::BUFFER_TOO_SMALL);
			let mut boot_opt: Box<BootOption> = unsafe { Box::from_raw(EFI_ALLOC.call_alloc(data_size).cast()) };
			let st = unsafe {
				(rt.get_variable)(name_raw.as_mut_ptr(), &raw mut guid, null_mut(), &raw mut data_size, (&raw mut *boot_opt).cast())
			};
			assert_eq!(st, Status::SUCCESS);
			let option = LoadOption {
				name,
				description: boot_opt.get_description(data_size),
				device_path: expand_short_device_path(boot_opt.get_device_path(data_size)),
				optional_data: boot_opt.get_optional_data(data_size),
			};
			options.push(option);
		}
	}
	// Loop until selection.
	let mut selection: usize = 0;
	let mut selected = false;
	while !selected
	{
		clear_screen();
		println!("Press UP/DOWN to select boot option. Press Enter to boot the selection.");
		for (i, opt) in options.iter().enumerate()
		{
			if i == selection
			{
				print!("----> ");
			}
			println!("{opt}");
		}
		// Loop until a valid key is input.
		loop
		{
			let r = block_until_keystroke2();
			if r == Ok('\r')
			{
				// Selected an option.
				selected = true;
				break;
			}
			else if r == Err(1)
			{
				// UP arrow.
				selection = selection.saturating_sub(1);
				break;
			}
			else if r == Err(2)
			{
				// DOWN arrow.
				if selection < options.len() - 1
				{
					selection += 1;
				}
				break;
			}
		}
	}
	unsafe {
		let bs = &*BS_TABLE.load(Ordering::Relaxed);
		let opt = &mut options[selection];
		let mut h: Handle = null_mut();
		let st = (bs.load_image)(Boolean::FALSE, source_image, opt.device_path.as_mut_ptr().cast(), null_mut(), 0, &raw mut h);
		if st.is_error()
		{
			panic!("Failed to load image! Status={st}");
		}
		if !opt.optional_data.is_empty()
		{
			let image: *mut loaded_image::Protocol = handle_protocol(h, loaded_image::PROTOCOL_GUID).unwrap();
			(*image).load_options = EFI_ALLOC.call_alloc(opt.optional_data.len()).cast();
			(*image).load_options_size = opt.optional_data.len() as u32;
			core::ptr::copy::<u8>(opt.optional_data.as_ptr(), (*image).load_options.cast(), opt.optional_data.len());
		}
		let st = (bs.start_image)(h, null_mut(), null_mut());
		if st.is_error()
		{
			panic!("Failed to start image! Status={st}");
		}
	}
}
