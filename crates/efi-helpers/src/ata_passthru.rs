// The r-efi crate does not contain ATA PassThru Protocol definitions.
// This module is subject to deletion once r-efi crate defines ATA PassThru Protocol.

use core::{alloc::Layout,ffi::c_void};

use alloc::boxed::Box;
use r_efi::{efi::{Event, Guid, Status}, protocols::device_path};

pub const PROTOCOL_GUID:Guid=Guid::from_fields
(
	0x1d3de7f0,
	0x0807,
	0x424f,
	0xaa,
	0x69,
	&[0x11,0xa5,0x4e,0x19,0xa4,0x6f]
);

#[repr(C)] pub struct Mode
{
	pub attributes:u32,
	pub io_align:u32
}

impl Mode
{
	pub const ATTRIBUTE_PHYSICAL:u32=0x0001;
	pub const ATTRIBUTE_LOGICAL:u32=0x0002;
	pub const ATTRIBUTE_NONBLOCKIO:u32=0x0004;
}

#[repr(C)] pub struct CommandBlock
{
	pub reserved1:[u8;2],
	pub command:u8,
	pub features:u8,
	pub sector_number:u8,
	pub cylinder_low:u8,
	pub cylinder_high:u8,
	pub device_head:u8,
	pub sector_number_exp:u8,
	pub cylinder_low_exp:u8,
	pub cylinder_high_exp:u8,
	pub features_exp:u8,
	pub sector_count:u8,
	pub sector_count_exp:u8,
	pub reserved2:[u8;6]
}

#[derive(Default)]
#[repr(C)] pub struct StatusBlock
{
	pub reserved1:[u8;2],
	pub status:u8,
	pub error:u8,
	pub sector_number:u8,
	pub cylinder_low:u8,
	pub cylinder_high:u8,
	pub device_head:u8,
	pub sector_number_exp:u8,
	pub cylinder_low_exp:u8,
	pub cylinder_high_exp:u8,
	pub reserved2:u8,
	pub sector_count:u8,
	pub sector_count_exp:u8,
	pub reserved3:[u8;6]
}

impl StatusBlock
{
	// PassThru() requires Asb to be aligned to Mode->IoAlign, or it returns EFI_INVALID_PARAMETER;
	// the struct's own (1-byte) alignment isn't enough, so allocate it by hand at the required alignment.
	pub fn new(alignment:usize)->Box<Self>
	{
		let layout=Layout::from_size_align(size_of::<Self>(),alignment.max(1)).unwrap();
		unsafe
		{
			let ptr=alloc::alloc::alloc(layout).cast::<Self>();
			if ptr.is_null()
			{
				alloc::alloc::handle_alloc_error(layout);
			}
			Box::from_raw(ptr)
		}
	}
}

#[repr(C)] pub struct CommandPacket
{
	pub asb:*mut StatusBlock,
	pub acb:*mut CommandBlock,
	pub timeout:u64,
	pub in_data_buffer:*mut c_void,
	pub out_data_buffer:*mut c_void,
	pub in_transfer_length:u32,
	pub out_transfer_length:u32,
	pub protocol:u8,
	pub length:u8
}

impl CommandPacket
{
	pub const PROTOCOL_ATA_HARDWARE_RESET:u8=0x00;
	pub const PROTOCOL_ATA_SOFTWARE_RESET:u8=0x01;
	pub const PROTOCOL_ATA_NON_DATA:u8=0x02;
	pub const PROTOCOL_ATA_PIO_DATA_IN:u8=0x04;
	pub const PROTOCOL_ATA_PIO_DATA_OUT:u8=0x05;
	pub const PROTOCOL_ATA_DMA:u8=0x06;
	pub const PROTOCOL_ATA_DMA_QUEUED:u8=0x07;
	pub const PROTOCOL_ATA_DEVICE_DIAGNOSTIC:u8=0x08;
	pub const PROTOCOL_ATA_DEVICE_RESET:u8=0x09;
	pub const PROTOCOL_UDMA_DATA_IN:u8=0x0A;
	pub const PROTOCOL_UDMA_DATA_OUT:u8=0x0B;
	pub const PROTOCOL_FPDMA:u8=0x0C;
	pub const PROTOCOL_RETURN_RESPONSE:u8=0xFF;

	pub const LENGTH_BYTES:u8=0x80;
	pub const LENGTH_MASK:u8=0x70;
	pub const LENGTH_NO_DATA_TRANSFER:u8=0x00;
	pub const LENGTH_FEATURES:u8=0x10;
	pub const LENGTH_SECTOR_COUNT:u8=0x20;
	pub const LENGTH_TPSIU:u8=0x30;
	pub const LENGTH_COUNT:u8=0x0F;
}

pub type ProtocolPassThru=unsafe extern "efiapi" fn
(
	this:*mut Protocol,
	port:u16,
	port_multiplier_port:u16,
	packet:*const CommandPacket,
	event:Event
)->Status;

pub type ProtocolGetNextPort=unsafe extern "efiapi" fn
(
	this:*mut Protocol,
	port:*mut u16
)->Status;

pub type ProtocolGetNextDevice=unsafe extern "efiapi" fn
(
	this:*mut Protocol,
	port:u16,
	port_multiplier_port:*mut u16
)->Status;

pub type ProtocolBuildDevicePath=unsafe extern "efiapi" fn
(
	this:*mut Protocol,
	port:u16,
	port_multiplier_port:u16,
	device_path:*mut *mut device_path::Protocol
)->Status;

pub type ProtocolGetDevice=unsafe extern "efiapi" fn
(
	this:*mut Protocol,
	device_path:*const device_path::Protocol,
	port:*mut u16,
	port_multiplier_port:*mut u16
)->Status;

pub type ProtocolResetPort=unsafe extern "efiapi" fn
(
	this:*mut Protocol,
	port:*const u16
)->Status;

pub type ProtocolResetDevice=unsafe extern "efiapi" fn
(
	this:*mut Protocol,
	port:u16,
	port_multiplier_port:u16
)->Status;

#[repr(C)] pub struct Protocol
{
	pub mode:*mut Mode,
	pub passthru:ProtocolPassThru,
	pub get_next_port:ProtocolGetNextPort,
	pub get_next_device:ProtocolGetNextDevice,
	pub build_device_path:ProtocolBuildDevicePath,
	pub get_device:ProtocolGetDevice,
	pub reset_port:ProtocolResetPort,
	pub reset_device:ProtocolResetDevice
}