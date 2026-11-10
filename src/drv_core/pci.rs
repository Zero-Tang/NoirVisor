/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file is the driver of the PCI configuration space of NoirVisor Core in Rust.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use crate::xpf_core::asm::io::{in_dword, out_dword};

const PCI_CONFIG_ADDRESS: u16 = 0xCF8;
const PCI_CONFIG_DATA: u16 = 0xCFC;

/// Offset of the Status (high word) and Command (low word) registers.
pub const PCI_CFG_STATUS_COMMAND: u8 = 0x04;
/// Offset of the Capabilities Pointer register.
pub const PCI_CFG_CAPABILITY_POINTER: u8 = 0x34;
/// The Status register indicates the presence of the capabilities list.
const PCI_STATUS_CAPABILITY_LIST: u32 = 1 << 20;

/// Capability ID of Message Signaled Interrupts (MSI).
pub const PCI_CAPABILITY_ID_MSI: u8 = 0x05;

/// ## `pci_cfg_address` function
/// Compose the value of the configuration address port.
/// `bdf` is the PCI device ID in the format of bus (bits 15:8), device (bits 7:3) and function (bits 2:0).
const fn pci_cfg_address(bdf: u16, offset: u8) -> u32
{
	(1 << 31) | ((bdf as u32) << 8) | (offset as u32 & 0xFC)
}

/// ## `with_pci_cfg_address` function
/// Access the configuration space with the configuration address port saved and restored. \
/// The access may interrupt the guest in the middle of its configuration access, which consists of two I/O operations.
/// Restoring the address port makes the access transparent to the guest.
fn with_pci_cfg_address<R>(bdf: u16, offset: u8, f: impl FnOnce() -> R) -> R
{
	unsafe {
		let saved = in_dword(PCI_CONFIG_ADDRESS);
		out_dword(PCI_CONFIG_ADDRESS, pci_cfg_address(bdf, offset));
		let r = f();
		out_dword(PCI_CONFIG_ADDRESS, saved);
		r
	}
}

/// ## `pci_cfg_read32` function
/// Read a dword from the configuration space of segment 0 via the legacy configuration mechanism.
pub fn pci_cfg_read32(bdf: u16, offset: u8) -> u32
{
	with_pci_cfg_address(bdf, offset, || unsafe { in_dword(PCI_CONFIG_DATA) })
}

/// ## `pci_cfg_write32` function
/// Write a dword to the configuration space of segment 0 via the legacy configuration mechanism.
pub fn pci_cfg_write32(bdf: u16, offset: u8, value: u32)
{
	with_pci_cfg_address(bdf, offset, || unsafe { out_dword(PCI_CONFIG_DATA, value) })
}

/// ## `pci_find_capability` function
/// Locate the capability with `id` in the capabilities list. Returns the offset of the capability.
pub fn pci_find_capability(bdf: u16, id: u8) -> Option<u8>
{
	if pci_cfg_read32(bdf, PCI_CFG_STATUS_COMMAND) & PCI_STATUS_CAPABILITY_LIST == 0
	{
		return None;
	}
	let mut ptr = (pci_cfg_read32(bdf, PCI_CFG_CAPABILITY_POINTER) & 0xFC) as u8;
	// At most 48 capabilities fit in the configuration space. Don't loop forever on a malformed list.
	for _ in 0..48
	{
		if ptr == 0
		{
			break;
		}
		let header = pci_cfg_read32(bdf, ptr);
		if header as u8 == id
		{
			return Some(ptr);
		}
		ptr = ((header >> 8) & 0xFC) as u8;
	}
	None
}
