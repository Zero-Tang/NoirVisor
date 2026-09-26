// DMA Test

use core::{arch::x86_64::__cpuid, ffi::c_void, ptr::null_mut, slice, sync::atomic::Ordering};

use alloc::vec::Vec;
use efi_helpers::{BS_TABLE, ProtocolBuffer, ata_passthru::{self, CommandBlock, CommandPacket, StatusBlock}, block_until_keystroke, handle_protocol, pe::{IMAGE_DOS_HEADER, IMAGE_DOS_SIGNATURE, IMAGE_NT_HEADERS, IMAGE_NT_SIGNATURE, IMAGE_SECTION_HEADER, IMAGE_SIZEOF_SHORT_NAME}, println};
use r_efi::{efi::{Handle, Status}, protocols::loaded_image};

fn do_test_dma(address:*mut c_void)
{
	let ata_protocols:ProtocolBuffer<ata_passthru::Protocol>=ProtocolBuffer::locate_by_protocol(ata_passthru::PROTOCOL_GUID).unwrap();
	for &protocol in ata_protocols.as_slice()
	{
		let mut v:Vec<(u16,Vec<u16>)>=Vec::new();
		println!("Found ATA Protocol at {protocol:p}!");
		let ata=unsafe{&mut *protocol};
		println!("I/O Alignment: {}",unsafe{(*ata.mode).io_align});
		let mut port=u16::MAX;
		loop
		{
			let st=unsafe{(ata.get_next_port)(ata,&raw mut port)};
			match st
			{
				Status::SUCCESS=>
				{
					let mut multiplier=u16::MAX;
					let mut u:Vec<u16>=Vec::new();
					loop
					{
						let st=unsafe{(ata.get_next_device)(ata,port,&raw mut multiplier)};
						match st
						{
							Status::SUCCESS=>
							{
								let mut dev_path=null_mut();
								let st=unsafe{(ata.build_device_path)(ata,port,multiplier,&raw mut dev_path)};
								println!("BuildDevicePath returned {st}");
								if st!=Status::SUCCESS
								{
									panic!("ATA Controller failed to build device path for Port 0x{port:X} Multiplier 0x{multiplier:X}!");
								}
								let path=unsafe{&*dev_path};
								assert_eq!(path.r#type,3);
								assert_eq!(path.sub_type,18);
								assert_eq!(path.length[0],10);
								let hba_port=unsafe{dev_path.byte_add(4).cast::<u16>().read_unaligned()};
								let mult_port=unsafe{dev_path.byte_add(6).cast::<u16>().read_unaligned()};
								let logical=unsafe{dev_path.byte_add(8).cast::<u16>().read_unaligned()};
								println!("HBA Port: 0x{hba_port:X}, Port Multiplier Port: 0x{mult_port:X}, Logical Unit: 0x{logical:X}");
								unsafe
								{
									let bs=&*BS_TABLE.load(Ordering::Relaxed);
									(bs.free_pool)(dev_path.cast());
								}
								u.push(multiplier);
							}
							Status::NOT_FOUND=>break,
							_=>panic!("Unexpected Status={st}!")
						}
					}
					v.push((port,u));
				}
				Status::NOT_FOUND=>break,
				_=>panic!("Unexpected Status={st}!")
			};
		}
		println!("ATA Devices: {v:X?}");
		let mut acb=CommandBlock
		{
			reserved1:[0;2],
			command:0x25,
			features:0,
			sector_number:0,
			cylinder_low:0,
			cylinder_high:0,
			device_head:0,
			sector_number_exp:0,
			cylinder_low_exp:0,
			cylinder_high_exp:0,
			features_exp:0,
			sector_count:1,
			sector_count_exp:0,
			reserved2:[0;6]
		};
		let mut asb=StatusBlock::new(unsafe{(*ata.mode).io_align} as usize);
		let cmd=CommandPacket
		{
			asb:&raw mut *asb,
			acb:&raw mut acb,
			timeout:60*1000*1000*10,
			in_data_buffer:address,
			out_data_buffer:null_mut(),
			in_transfer_length:1,
			out_transfer_length:0,
			protocol:CommandPacket::PROTOCOL_UDMA_DATA_IN,
			length:CommandPacket::LENGTH_SECTOR_COUNT
		};
		println!("ASB: {:p}, ACB: {:p}",cmd.asb,cmd.acb);
		let st=unsafe{(ata.passthru)(ata,0,0xFFFF,&raw const cmd,null_mut())};
		println!("PassThru Status={st}");
		if st==Status::SUCCESS
		{
			let bytes:&[u8;4096]=unsafe{&*address.cast()};
			println!("{:02X} {:02X}",bytes[511],bytes[510])
		}
	}
	println!("Press any key to continue...");
	block_until_keystroke();
}

pub fn test_dma(hv_img:Handle)
{
	let r=__cpuid(0x40000006);
	if (r.eax & (1<<7))!=0
	{
		let img:*mut loaded_image::Protocol=handle_protocol(hv_img,loaded_image::PROTOCOL_GUID).unwrap();
		let dos_head:&IMAGE_DOS_HEADER=unsafe{&*(*img).image_base.cast()};
		if dos_head.e_magic==IMAGE_DOS_SIGNATURE
		{
			let nt_head:&IMAGE_NT_HEADERS=unsafe{&*(*img).image_base.byte_add(dos_head.e_lfanew as usize).cast()};
			if nt_head.Signature==IMAGE_NT_SIGNATURE
			{
				let section_headers:&[IMAGE_SECTION_HEADER]=unsafe{slice::from_raw_parts((nt_head as *const IMAGE_NT_HEADERS).add(1).cast(),nt_head.FileHeader.NumberOfSections as usize)};
				for section in section_headers
				{
					let name=unsafe{str::from_utf8_unchecked(&section.Name[..section.Name.iter().position(|&c| c==0).unwrap_or(IMAGE_SIZEOF_SHORT_NAME)])};
					if name==".dma_atk"
					{
						let base=unsafe{(*img).image_base.byte_add(section.VirtualAddress as usize)};
						println!("Found DMA-attack target section! Base={base:p}");
						do_test_dma(base);
					}
				}
			}
		}
	}
}