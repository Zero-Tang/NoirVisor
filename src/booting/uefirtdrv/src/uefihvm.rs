/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2026, Zero Tang. All rights reserved.

  This file is the HyperVisor Invoker on UEFI Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).
*/

use core::{arch::{asm, naked_asm}, ffi::c_void, mem::offset_of, ptr::null_mut, slice, sync::atomic::Ordering};
#[cfg(target_arch="x86_64")]
use core::arch::x86_64::__cpuid;
#[cfg(target_arch="x86")]
use core::arch::x86::__cpuid;

use r_efi::efi::{ACPI_10_TABLE_GUID, ACPI_20_TABLE_GUID, EVENT_GROUP_EXIT_BOOT_SERVICES, EVT_NOTIFY_SIGNAL, Event, EventNotify, TPL_NOTIFY};

use crate::{cfgmgr::{ConfigRecord, ConfigurationList}, host::{BS_TABLE, IMAGE_INFO, ST_TABLE}, pe::*, println};

unsafe extern "C"
{
	fn noir_configure_qemu_debug_console(port:u16)->u32;
	fn noir_configure_serial_port_debugger(port_number:u8,port_base:u16,baud_rate:u32)->u32;
	fn noir_add_section_to_ci(base:*mut c_void,size:u32,delay:bool)->bool;
	fn noir_activate_ci()->bool;
	fn nvc_logger_initialize(level:u32)->bool;
	fn nvc_build_hypervisor()->u32;
	fn NoirInitializeDisassembler();
	fn strlen(string:*const i8)->usize;
}

#[unsafe(no_mangle)] extern "C" fn nvc_store_image_info(base:*mut *mut c_void,size:*mut u32)
{
	let img=IMAGE_INFO.load(Ordering::Relaxed);
	unsafe
	{
		*base=(*img).image_base;
		*size=(*img).image_size as u32;
	}
}

pub fn init_internal_debugger()
{
	let configs=ConfigurationList::ref_global();
	match configs.query("DebugPort")
	{
		Some(port_type)=>
		{
			if let ConfigRecord::String(type_name)=port_type
			{
				match type_name
				{
					"qemu_debugcon"=>
					{
						let port=configs.query("QemuDebugConPortNumber").map(|record| record.unwrap_integer(0x402)).unwrap_or(0x402);
						println!("NoirVisor will use QEMU ISA Debug Console at Port 0x{port:04X}!");
						unsafe
						{
							noir_configure_qemu_debug_console(port as u16);
						}
						println!("Make sure you see a message on your debug console!");
					}
					"serial"=>
					{
						let baud_rate=configs.query("SerialBaudRate").map(|record| record.unwrap_integer(115200)).unwrap_or(115200);
						let port_number=configs.query("SerialPortNumber").map(|record| record.unwrap_integer(2)).unwrap_or(2);
						let port_base=configs.query("SerialPortBase").map(|record| record.unwrap_integer(0x2F8)).unwrap_or(0x2F8);
						println!("NoirVisor will use Serial connection (COM{port_number}) at Port 0x{port_base:04X} with baud-rate {baud_rate} Hz!");
						unsafe
						{
							noir_configure_serial_port_debugger(port_number as u8,port_base as u16,baud_rate);
						}
						println!("Make sure you see a message on your debug console!");
					}
					_=>println!("Warning: Unknown Debugger Medium: {type_name}")
				}
			}
			else
			{
				println!("Warning: Port Type is not string! Specified with: {port_type:?}")
			}
		}
		None=>println!("Warning: Missing Internal Debugger configuration!")
	}
}

pub fn init_logger()->bool
{
	let configs=ConfigurationList::ref_global();
	let level=configs.query("LogLevel").map(|record| record.unwrap_integer(5)).unwrap_or(5);
	unsafe
	{
		nvc_logger_initialize(level)
	}
}

pub fn init_disasm()
{
	unsafe
	{
		NoirInitializeDisassembler();
	}
}

const FEATURE_CPUID_PRESENCE:u64=0x4;
const FEATURE_NESTED_VIRTUALIZATION:u64=0x10;
const FEATURE_ENABLE_IOMMU:u64=0x200;

#[unsafe(no_mangle)] extern "C" fn noir_query_enabled_features_in_system()->u64
{
	let configs=ConfigurationList::ref_global();
	let mut features:u64=0;
	if configs.query("CpuidPresence").map(|record| record.unwrap_bool(true)).unwrap_or(true)
	{
		features|=FEATURE_CPUID_PRESENCE;
	}
	if configs.query("NestedVirtualization").map(|record| record.unwrap_bool(false)).unwrap_or(false)
	{
		features|=FEATURE_NESTED_VIRTUALIZATION;
	}
	if configs.query("EnableIommu").map(|record| record.unwrap_bool(true)).unwrap_or(true)
	{
		features|=FEATURE_ENABLE_IOMMU;
	}
	features
}

// There is no point to import an ACPI library just for this definition.
#[repr(C,packed)] struct Acpi20RootSystemDescriptionPointer
{
	signature:u64,
	checksum:u8,
	oem_id:[u8;6],
	revision:u8,
	rsdt_address:u32,
	length:u32,
	xsdt_address:u64,
	extended_checksum:u8,
	reserved:[u8;3]
}
#[repr(C,packed)] struct Acpi10RootSystemDescriptionPointer
{
	signature:u64,
	checksum:u8,
	oem_id:[u8;6],
	reserved:u8,
	rsdt_address:u32,
}

#[unsafe(no_mangle)] extern "C" fn noir_locate_acpi_rsdt(_length:*mut usize)->*mut c_void
{
	let st=unsafe{&*ST_TABLE.load(Ordering::Relaxed)};
	let mut xsdt:*mut c_void=null_mut();
	let mut rsdt:*mut c_void=null_mut();
	let config_table=unsafe{slice::from_raw_parts(st.configuration_table,st.number_of_table_entries)};
	for config in config_table
	{
		if config.vendor_guid==ACPI_20_TABLE_GUID
		{
			let rsdp:*mut Acpi20RootSystemDescriptionPointer=config.vendor_table.cast();
			let rsdt_addr=unsafe{rsdp.byte_add(offset_of!(Acpi20RootSystemDescriptionPointer,rsdt_address)).cast::<u32>().read_unaligned()};
			let xsdt_addr=unsafe{rsdp.byte_add(offset_of!(Acpi20RootSystemDescriptionPointer,xsdt_address)).cast::<u64>().read_unaligned()};
			xsdt=xsdt_addr as *mut c_void;
			rsdt=rsdt_addr as *mut c_void;
			break;
		}
		else if config.vendor_guid==ACPI_10_TABLE_GUID
		{
			let rsdp:*mut Acpi10RootSystemDescriptionPointer=config.vendor_table.cast();
			let rsdt_addr=unsafe{rsdp.byte_add(offset_of!(Acpi10RootSystemDescriptionPointer,rsdt_address)).cast::<u32>().read_unaligned()};
			rsdt=rsdt_addr as *mut c_void;
		}
	}
	if xsdt.is_null()
	{
		rsdt
	}
	else
	{
		xsdt
	}
}

pub fn init_ci()->bool
{
	let image_info=IMAGE_INFO.load(Ordering::Relaxed);
	let image_base=unsafe{(*image_info).image_base};
	let dos_head:&IMAGE_DOS_HEADER=unsafe{&*image_base.cast()};
	if dos_head.e_magic==IMAGE_DOS_SIGNATURE
	{
		let nt_head:&IMAGE_NT_HEADERS=unsafe{&*image_base.byte_add(dos_head.e_lfanew as usize).cast()};
		if nt_head.Signature==IMAGE_NT_SIGNATURE
		{
			let section_headers:&[IMAGE_SECTION_HEADER]=unsafe{slice::from_raw_parts(image_base.byte_add(dos_head.e_lfanew as usize+size_of::<IMAGE_NT_HEADERS>()).cast(),nt_head.FileHeader.NumberOfSections as usize)};
			for section in section_headers
			{
				let section_name:&str=unsafe{str::from_utf8_unchecked(slice::from_raw_parts(section.Name.as_ptr(),core::cmp::min(strlen(section.Name.as_ptr().cast()),IMAGE_SIZEOF_SHORT_NAME)))};
				let base=unsafe{image_base.byte_add(section.VirtualAddress as usize)};
				let size=section.SizeOfRawData;
				if matches!(section_name,".text"|".eh_fram"|".rdata"|".pdata")
				{
					println!("Adding section {section_name} (Base: {base:p}, Size: 0x{size:X}) to CI...");
					if !unsafe{noir_add_section_to_ci(base,size,false)}
					{
						println!("Failed to add section {section_name} to CI!");
						return false;
					}
				}
				else if section_name==".data"
				{
					let base=unsafe{image_base.byte_add(section.VirtualAddress as usize)};
					let size=section.SizeOfRawData;
					println!("Delaying section {section_name} (Base: {base:p}, Size: 0x{size:X}) to CI...");
					if !unsafe{noir_add_section_to_ci(base,size,true)}
					{
						println!("Failed to add section {section_name} to CI!");
						return false;
					}
				}
			}
			unsafe
			{
				return noir_activate_ci();
			}
		}
	}
	false
}

pub fn test_ci()
{
	let image_info=IMAGE_INFO.load(Ordering::Relaxed);
	let image_base=unsafe{(*image_info).image_base};
	let dos_head:&IMAGE_DOS_HEADER=unsafe{&*image_base.cast()};
	if dos_head.e_magic==IMAGE_DOS_SIGNATURE
	{
		let nt_head:&IMAGE_NT_HEADERS=unsafe{&*image_base.byte_add(dos_head.e_lfanew as usize).cast()};
		if nt_head.Signature==IMAGE_NT_SIGNATURE
		{
			let section_headers:&[IMAGE_SECTION_HEADER]=unsafe{slice::from_raw_parts(image_base.byte_add(dos_head.e_lfanew as usize+size_of::<IMAGE_NT_HEADERS>()).cast(),nt_head.FileHeader.NumberOfSections as usize)};
			println!("[CI Test] Searching for .text section (has {} sections)...",section_headers.len());
			for section in section_headers
			{
				let section_name:&str=unsafe{str::from_utf8_unchecked(slice::from_raw_parts(section.Name.as_ptr(),strlen(section.Name.as_ptr().cast())))};
				if section_name==".text"
				{
					unsafe
					{
						let p:*mut u8=image_base.byte_add(section.VirtualAddress as usize).cast();
						let old_byte=*p;
						println!("[CI Test] Located .text section! Writing stuff...");
						*p=old_byte;
					}
					println!("[CI Test] It seems nothing happened?");
					break;
				}
			}
		}
	}
}

#[allow(dead_code)]
pub fn test_init()
{
	unsafe
	{
		let bs=&*BS_TABLE.load(Ordering::Relaxed);
		let p=&mut *(0x1000 as *mut [u8;2]);
		p[0]=0xEB;
		p[1]=0xFE;
		let apic_base:*mut u32;
		asm!
		(
			"rdmsr",
			"shl rdx,32",
			"and eax,0xFFFFF000",
			"or rax,rdx",
			in("ecx") 0x1b,
			out("rax") apic_base
		);
		println!("Sending INIT (APIC-Base={apic_base:p}...");
		apic_base.byte_add(0x310).write_volatile(1);
		apic_base.byte_add(0x300).write_volatile(0xC4500);
		println!("Sent INIT!");
		(bs.stall)(1000000);
		apic_base.byte_add(0x310).write_volatile(1);
		apic_base.byte_add(0x300).write_volatile(0xC4601);
		println!("Sent SIPI!");
		(bs.stall)(1000000);
		apic_base.byte_add(0x310).write_volatile(1);
		apic_base.byte_add(0x300).write_volatile(0xC4601);
		println!("Sent SIPI!");
	}
}

pub fn build_hypervisor()->u32
{
	let st=unsafe{nvc_build_hypervisor()};
	// Test CPUID.
	let r=__cpuid(1);
	if (r.ecx&0x80000000)!=0
	{
		let r=__cpuid(0x40000001);
		let interface_id=unsafe{str::from_utf8_unchecked(slice::from_raw_parts((&raw const r.eax).cast(),4))};
		let r=__cpuid(0x40000000);
		let vendor_id=unsafe{str::from_utf8_unchecked(slice::from_raw_parts((&raw const r.ebx).cast(),12))};
		println!("Hypervisor is detected! Maximum Leaf: 0x{:X}, Vendor: {vendor_id}, Signature: {interface_id}",r.eax);
	}
	st
}

static mut EXIT_BOOT_SERVICES_EVENT:Event=null_mut();
const NOIR_HYPERCALL_EXIT_BOOT_SERVICES:u32=3;

// In order to avoid relocation while handling the event, use naked assembly to guarantee location-independency.
#[unsafe(naked)] extern "efiapi" fn noir_exit_boot_services_notification_intel_fn(event:Event,context:*mut c_void)
{
	naked_asm!
	(
		"mov ecx,{hvc_index}",
		"vmcall",
		"ret",
		hvc_index=const NOIR_HYPERCALL_EXIT_BOOT_SERVICES
	)
}

// In order to avoid relocation while handling the event, use naked assembly to guarantee location-independency.
#[unsafe(naked)] extern "efiapi" fn noir_exit_boot_services_notification_amd_fn(event:Event,context:*mut c_void)
{
	naked_asm!
	(
		"mov ecx,{hvc_index}",
		"vmmcall",
		"ret",
		hvc_index=const NOIR_HYPERCALL_EXIT_BOOT_SERVICES
	)
}

pub fn register_exit_boot_services_event()
{
	let bs=BS_TABLE.load(Ordering::Relaxed);
	let exit_bs_guid=EVENT_GROUP_EXIT_BOOT_SERVICES;
	// Determine the CPU vendor name and confirm what hypercall instruction will be used.
	let r=__cpuid(0);
	let mut vstr:[u8;12]=[0;12];
	vstr[..4].copy_from_slice(&r.ebx.to_le_bytes());
	vstr[4..8].copy_from_slice(&r.edx.to_le_bytes());
	vstr[8..].copy_from_slice(&r.ecx.to_le_bytes());
	let vstr=unsafe{str::from_utf8_unchecked(&vstr)};
	let notify_fn=match vstr
	{
		"GenuineIntel"|"VIA VIA VIA "|"  Shanghai  "|"CentaurHauls"=>Some(noir_exit_boot_services_notification_intel_fn as EventNotify),
		"AuthenticAMD"|"HygonGenuine"=>Some(noir_exit_boot_services_notification_amd_fn as EventNotify),
		_=>None
	};
	let st=unsafe{((*bs).create_event_ex)(EVT_NOTIFY_SIGNAL,TPL_NOTIFY,notify_fn,null_mut(),&raw const exit_bs_guid,&raw mut EXIT_BOOT_SERVICES_EVENT)};
	println!("Register ExitBootServices Event Status=0x{:X}",st.as_usize());
}

pub fn suppress_image_relocation()
{
	let img_info=unsafe{&*IMAGE_INFO.load(Ordering::Relaxed)};
	let dos_head:&IMAGE_DOS_HEADER=unsafe{&*img_info.image_base.cast()};
	if dos_head.e_magic==IMAGE_DOS_SIGNATURE
	{
		let nt_head:&mut IMAGE_NT_HEADERS64=unsafe{&mut *img_info.image_base.byte_add(dos_head.e_lfanew as usize).cast()};
		if nt_head.Signature==IMAGE_NT_SIGNATURE
		{
			// Locate the Relocation Directory.
			let reloc_base=&mut nt_head.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
			// Mark the relocation directory as invalid by setting them to zero.
			reloc_base.VirtualAddress=0;
			reloc_base.Size=0;
		}
	}
}