/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file is the AMD-Vi driver of NoirVisor Core in Rust.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use alloc::{sync::Arc, vec, vec::Vec};
use core::{
	ffi::c_void,
	hint::spin_loop,
	sync::atomic::{AtomicU64, AtomicUsize, Ordering},
};

use log::*;
use pastey::paste;

use super::{IOMMU_FAULT_EVENT_VECTOR, IommuOps};
use crate::{
	drv_core::{acpi::tables::IoVirtualizationReportingStructure, lapic::lapic_id, pci::*},
	xpf_core::{
		intflt::{InterruptDisposition, InterruptOps},
		x86::apic::MsiAddress,
	},
	*,
};
use acpi::*;
use cmdbuf::*;
use devtbl::*;
use paging::*;
use registers::*;
use xpf_core::{ioflt::IoRegionOps, nvbdk::*};

#[allow(dead_code)]
mod acpi;
#[allow(dead_code)]
mod cmdbuf;
#[allow(dead_code)]
mod devtbl;
#[allow(dead_code)]
mod paging;
#[allow(dead_code)]
mod registers;

pub struct AmdIommuManager
{
	pub ivrs_units: Vec<Arc<AmdIommuBar>>,
	pub device_table: MemoryDescriptor<PAGE_TABLE_ENTRIES64, AmdIommuDeviceTableEntry>,
	pub pml4e: MemoryDescriptor<1, AmdIommuPde>,
	pub pml3e: Vec<AmdIommuPmlDescriptor<2>>,
	pub pml2e: Vec<AmdIommuPmlDescriptor<1>>,
	pub pml1e: Vec<AmdIommuPmlDescriptor<0>>,
}

macro_rules! derive_pml_get_methods {
	($name:tt,$size:tt) => {
		paste! {
			fn [<get_ $name:lower _mut>](&mut self,gpa:u64)->Option<&mut AmdIommuPxe>
			{
				match self.$name.binary_search_by(|d| d.compare_gpa(gpa))
				{
					Ok(i)=>
					{
						let d=&mut self.$name[i];
						let j=page_entry_index([<page_ $size _count>](gpa as usize));
						Some(unsafe{&mut *d.pxe.virt.add(j)})
					}
					Err(_)=>None
				}
			}
		}
	};
}

impl AmdIommuManager
{
	fn push_bar(&mut self, bar: u64, cap_offset: u16, device_id: u16, segment: u16)
	{
		#[allow(clippy::arc_with_non_send_sync)]
		match self.ivrs_units.binary_search_by(|iommu| iommu.bar.phys.cmp(&bar))
		{
			Ok(i) => warn!("Base 0x{bar:X} already exist! Index={i}"),
			Err(i) => self.ivrs_units.insert(
				i,
				Arc::new(AmdIommuBar {
					bar: MappedDescriptor::map(bar, 4 << PAGE_SHIFT),
					cap_offset: cap_offset as usize,
					device_id,
					segment,
					size: 4 << PAGE_SHIFT,
					iotlb_sup: true,
					cmd_base: MemoryDescriptor::alloc().unwrap(),
					cmd_index: AtomicUsize::new(0),
					log_base: MemoryDescriptor::alloc().unwrap(),
				}),
			),
		}
	}

	fn split_pml4e(&mut self, gpa: u64)
	{
		if let Err(i) = self.pml3e.binary_search_by(|d| d.compare_gpa(gpa))
		{
			match MemoryDescriptor::alloc()
			{
				Some(md) =>
				{
					// Initialize descriptor.
					let d: AmdIommuPmlDescriptor<2> = AmdIommuPmlDescriptor { gpa_start: page_512gb_base(gpa), pxe: md };
					trace!("IOMMU PML3E: 0x{:X}", d.pxe.phys);
					// Initialize PML3E Page.
					let pml3s: &mut [AmdIommuPte; PAGE_TABLE_ENTRIES64] = unsafe { &mut *d.pxe.virt.cast() };
					// Set all to RWX since PML3E supports huge-page allocation.
					for (i, pml3e) in pml3s.iter_mut().enumerate()
					{
						*pml3e = AmdIommuPte::new_pte(d.gpa_start + (page_1gb_mult(i) as u64), true, true);
					}
					// Update PML4 Entry.
					self.get_pml4e_mut(gpa).unwrap().pde = AmdIommuPde::new_pde(d.pxe.phys, 3, true, true);
					// Insert to IOMMU Paging Manager.
					self.pml3e.insert(i, d);
				}
				None => panic!("Failed to allocate PML3E to split PML4E!"),
			}
		}
	}

	fn split_pml3e(&mut self, gpa: u64)
	{
		if let Err(i) = self.pml2e.binary_search_by(|d| d.compare_gpa(gpa))
		{
			match MemoryDescriptor::alloc()
			{
				Some(md) =>
				{
					// Create PML3E first.
					self.split_pml4e(gpa);
					// Initialize descriptor.
					let d: AmdIommuPmlDescriptor<1> = AmdIommuPmlDescriptor { gpa_start: page_1gb_base(gpa), pxe: md };
					trace!("IOMMU PML2E: 0x{:X}", d.pxe.phys);
					// Initialize PML2E Page.
					let pml2s: &mut [AmdIommuPte; PAGE_TABLE_ENTRIES64] = unsafe { &mut *d.pxe.virt.cast() };
					// Set all to RWX since PML2E supports large-page allocation.
					for (i, pml2e) in pml2s.iter_mut().enumerate()
					{
						*pml2e = AmdIommuPte::new_pte(d.gpa_start + (page_2mb_mult(i) as u64), true, true);
					}
					// Update PML3 Entry.
					self.get_pml3e_mut(gpa).unwrap().pde = AmdIommuPde::new_pde(d.pxe.phys, 2, true, true);
					// Insert to IOMMU Paging Manager.
					self.pml2e.insert(i, d);
				}
				None => panic!("Failed to allocate PML2E to split PML3E!"),
			}
		}
	}

	fn split_pml2e(&mut self, gpa: u64)
	{
		if let Err(i) = self.pml1e.binary_search_by(|d| d.compare_gpa(gpa))
		{
			match MemoryDescriptor::alloc()
			{
				Some(md) =>
				{
					// Create PML2E first.
					self.split_pml3e(gpa);
					// Initialize descriptor.
					let d: AmdIommuPmlDescriptor<0> = AmdIommuPmlDescriptor { gpa_start: page_2mb_base(gpa), pxe: md };
					trace!("IOMMU PML1E: 0x{:X}", d.pxe.phys);
					// Initialize PML1E Page.
					let pml1s: &mut [AmdIommuPte; PAGE_TABLE_ENTRIES64] = unsafe { &mut *d.pxe.virt.cast() };
					// Set all to RWX since PML1E is the last level of paging.
					for (i, pml1e) in pml1s.iter_mut().enumerate()
					{
						*pml1e = AmdIommuPte::new_pte(d.gpa_start + (page_4kb_mult(i) as u64), true, true);
					}
					// Update PML2 Entry.
					self.get_pml2e_mut(gpa).unwrap().pde = AmdIommuPde::new_pde(d.pxe.phys, 1, true, true);
					// Insert to IOMMU Paging Manager.
					self.pml1e.insert(i, d);
				}
				None => panic!("Failed to allocate PML1E to split PML2E!"),
			}
		}
	}

	fn get_pml4e_mut(&mut self, gpa: u64) -> Option<&mut AmdIommuPxe>
	{
		let i = page_512gb_count(gpa as usize);
		if i < PAGE_TABLE_ENTRIES64 { Some(unsafe { &mut *self.pml4e.virt.add(i).cast() }) } else { None }
	}

	derive_pml_get_methods!(pml3e, 1gb);
	derive_pml_get_methods!(pml2e, 2mb);
	derive_pml_get_methods!(pml1e, 4kb);
}

impl IommuOps for AmdIommuManager
{
	unsafe fn init(&mut self)
	{
		unsafe {
			use core::ptr::write;
			write(&raw mut self.ivrs_units, Vec::new());
			write(&raw mut self.device_table, MemoryDescriptor::alloc_2mb_page().unwrap());
			write(&raw mut self.pml4e, MemoryDescriptor::alloc().unwrap());
			write(&raw mut self.pml3e, Vec::new());
			write(&raw mut self.pml2e, Vec::new());
			write(&raw mut self.pml1e, Vec::new());
		}
	}

	fn subvert(&mut self)
	{
		// Initialize Device Table Entries.
		let devtbl_vec: &mut [AmdIommuDeviceTableEntry; 65536] = unsafe { &mut *self.device_table.virt.cast() };
		for (i, dt) in devtbl_vec.iter_mut().enumerate()
		{
			dt.p1 = AmdIommuDteP1::from_bits(self.pml4e.phys + page_4kb_count(i) as u64)
				.with_v(true)
				.with_tv(true)
				.with_ha(true)
				.with_hd(true)
				.with_mode(4)
				.with_ir(true)
				.with_iw(true);
			// Don't suppress I/O page fault events (SE and SA), so that NoirVisor can keep track of DMA faults.
			dt.p2 = AmdIommuDteP2::new().with_did(1).with_i(true);
			dt.p3 = AmdIommuDteP3::new();
			dt.p4 = AmdIommuDteP4::new();
		}
		// Setup all IOMMUs.
		for ivrs in &mut self.ivrs_units
		{
			let virt = ivrs.bar.virt.load(Ordering::Relaxed);
			let devtbl_base = DeviceTableBaseRegister::from_bits(self.device_table.phys).with_size(0x1FF);
			let cmd_buff_base = CommandBufferBaseRegister::from_bits(ivrs.cmd_base.phys).with_com_len(0b1000);
			let cmd_head = CommandBufferHeadPointerRegister::new();
			let cmd_tail = CommandBufferTailPointerRegister::new();
			let log_buff_base = EventLogBaseRegister::from_bits(ivrs.log_base.phys).with_event_len(0b1000);
			let log_head = EventBufferHeadPointerRegister::new();
			let log_tail = EventBufferTailPointerRegister::new();
			let iommu_ctrl =
				IommuControlRegister::new().with_iommu_en(true).with_gt_en(true).with_cmd_buff_en(true).with_event_log_en(true);
			unsafe {
				// Set Device Table Base.
				devtbl_base.write(virt);
				// Set Command Buffer.
				cmd_buff_base.write(virt);
				cmd_head.write(virt);
				cmd_tail.write(virt);
				// Set Event Log Buffer.
				log_buff_base.write(virt);
				log_head.write(virt);
				log_tail.write(virt);
				// Enable IOMMU.
				iommu_ctrl.write(virt);
			}
			// Invalidate all device IDs.
			for i in 0..=u16::MAX
			{
				let inv_dte_cmd = InvalidateDevtabEntryCommand::new().with_device_id(i);
				let j = ivrs.issue_cmd(inv_dte_cmd);
				if j == 0xFE
				{
					ivrs.wait();
				}
			}
			ivrs.wait();
			info!("AMD-Vi IOMMU completed subversion! Log-Base: 0x{:X}", ivrs.log_base.phys);
		}
	}

	fn restore(&mut self) {}

	fn get_interrupt_handlers(&self) -> Vec<Box<dyn InterruptOps>>
	{
		let units = self.ivrs_units.iter().map(Arc::clone).collect();
		vec![Box::new(AmdIommuEventHandler { units, trailing_interrupt: false })]
	}

	fn enable_fault_events(&self)
	{
		let Some(apic_id) = lapic_id()
		else
		{
			error!("The Local APIC is disabled! IOMMU fault events remain masked!");
			return;
		};
		for ivrs in &self.ivrs_units
		{
			let virt = ivrs.virt();
			let efr = unsafe { IommuExtendedFeatureRegister::read(virt) };
			let mut ctrl = unsafe { IommuControlRegister::read(virt) };
			if let Ok(dest) = u8::try_from(apic_id)
			{
				if !ivrs.setup_msi(dest, efr.msi_cap_mmio_sup())
				{
					continue;
				}
			}
			else if efr.xt_sup()
			{
				// MSI can't address x2APIC IDs beyond 255. Generate x2APIC interrupts instead.
				let xt_ctrl = XtGeneralInterruptControlRegister::new()
					.with_xt_int_vector(IOMMU_FAULT_EVENT_VECTOR)
					.with_xt_int_dest_lo(apic_id & 0xFF_FFFF)
					.with_xt_int_dest_hi((apic_id >> 24) as u8);
				unsafe {
					xt_ctrl.write(virt);
				}
				ctrl.set_int_cap_xt_en(true);
			}
			else
			{
				error!("IOMMU 0x{:X} can't deliver fault events to APIC ID 0x{apic_id:X}! They remain masked!", ivrs.bar.phys);
				continue;
			}
			// Unmask the event log interrupt. Pending events raise the interrupt right now.
			ctrl.set_event_int_en(true);
			unsafe {
				ctrl.write(virt);
			}
			info!(
				"Fault events of IOMMU 0x{:X} are enabled! Vector: 0x{IOMMU_FAULT_EVENT_VECTOR:02X}, APIC ID: 0x{apic_id:X}",
				ivrs.bar.phys
			);
		}
	}

	fn set_pdpte(&mut self, gpa: u64, hpa: u64, r: bool, w: bool, _x: bool)
	{
		self.split_pml4e(gpa);
		let pte = unsafe { &mut self.get_pml3e_mut(gpa).unwrap().pte };
		*pte = AmdIommuPte::new_pte(hpa, r, w);
	}

	fn set_pde(&mut self, gpa: u64, hpa: u64, r: bool, w: bool, _x: bool)
	{
		self.split_pml3e(gpa);
		let pte = unsafe { &mut self.get_pml2e_mut(gpa).unwrap().pte };
		*pte = AmdIommuPte::new_pte(hpa, r, w);
	}

	fn set_pte(&mut self, gpa: u64, hpa: u64, r: bool, w: bool, _x: bool)
	{
		self.split_pml2e(gpa);
		let pte = unsafe { &mut self.get_pml1e_mut(gpa).unwrap().pte };
		*pte = AmdIommuPte::new_pte(hpa, r, w);
	}

	fn get_bar_pages(&self) -> Vec<Box<dyn IoRegionOps>>
	{
		let mut v: Vec<Box<dyn IoRegionOps>> = Vec::new();
		for x in &self.ivrs_units
		{
			v.push(Box::new(AmdIommuBarRegion(Arc::clone(x))));
		}
		v
	}
}

pub struct AmdIommuBar
{
	pub bar: MappedDescriptor<c_void>,
	pub cap_offset: usize,
	/// The PCI device ID (bus, device and function) of the IOMMU.
	pub device_id: u16,
	/// The PCI segment of the IOMMU.
	pub segment: u16,
	pub size: usize,
	pub iotlb_sup: bool,
	pub cmd_index: AtomicUsize,
	pub cmd_base: MemoryDescriptor<1, u128>,
	pub log_base: MemoryDescriptor<1, u128>,
}

/// Number of entries in the event log. The event log occupies a page with 16-byte entries.
const EVENT_LOG_ENTRIES: usize = PAGE_SIZE / size_of::<u128>();

/// Names of event codes in the event log.
const EVENT_NAMES: [&str; 16] = [
	"Reserved",
	"ILLEGAL_DEV_TABLE_ENTRY",
	"IO_PAGE_FAULT",
	"DEV_TAB_HARDWARE_ERROR",
	"PAGE_TAB_HARDWARE_ERROR",
	"ILLEGAL_COMMAND_ERROR",
	"COMMAND_HARDWARE_ERROR",
	"IOTLB_INV_TIMEOUT",
	"INVALID_DEVICE_REQUEST",
	"INVALID_PPR_REQUEST",
	"EVENT_COUNTER_ZERO",
	"GUEST_EVENT_FAULT",
	"VIOMMU_HARDWARE_ERROR",
	"Unknown",
	"RMP_PAGE_FAULT",
	"RMP_HARDWARE_ERROR",
];

const EVENT_CODE_IO_PAGE_FAULT: u8 = 0b0010;

impl AmdIommuBar
{
	fn virt(&self) -> *mut c_void
	{
		self.bar.virt.load(Ordering::Relaxed)
	}

	/// ## `setup_msi` method
	/// Program the MSI capability of the IOMMU to deliver the fault events to the processor with APIC ID `dest`. \
	/// Returns `false` if the MSI capability is inaccessible.
	fn setup_msi(&self, dest: u8, mmio: bool) -> bool
	{
		// Fixed delivery mode, edge-triggered, physical destination mode.
		let addr = MsiAddress::new().with_base(MsiAddress::BASE).with_destination(dest).into_bits();
		let data = IOMMU_FAULT_EVENT_VECTOR as u16;
		if mmio
		{
			// The MSI capability block is accessible via MMIO.
			let virt = self.virt();
			unsafe {
				MsiAddressLowRegister::from_bits(addr).write(virt);
				MsiAddressHighRegister::new().write(virt);
				MsiDataRegister::new().with_msi_data(data).write(virt);
				// Enable MSI with a single message.
				let header = MsiCapabilityHeaderRegister::read(virt);
				header.with_msi_mult_mess_en(0).with_msi_en(true).write(virt);
			}
			return true;
		}
		// Access the MSI capability block via the PCI configuration space.
		if self.segment != 0
		{
			error!("IOMMU 0x{:X} is in PCI segment {}, which is not supported yet!", self.bar.phys, self.segment);
			return false;
		}
		let bdf = self.device_id;
		let Some(cap) = pci_find_capability(bdf, PCI_CAPABILITY_ID_MSI)
		else
		{
			error!("IOMMU 0x{:X} has no MSI capability!", self.bar.phys);
			return false;
		};
		let header = pci_cfg_read32(bdf, cap);
		let msg_ctrl = header >> 16;
		let addr64 = msg_ctrl & (1 << 7) != 0;
		let per_vector_mask = msg_ctrl & (1 << 8) != 0;
		pci_cfg_write32(bdf, cap + 4, addr);
		let data_offset = if addr64
		{
			pci_cfg_write32(bdf, cap + 8, 0);
			cap + 12
		}
		else
		{
			cap + 8
		};
		pci_cfg_write32(bdf, data_offset, data as u32);
		if per_vector_mask
		{
			// Unmask the first message.
			let mask = pci_cfg_read32(bdf, data_offset + 4);
			pci_cfg_write32(bdf, data_offset + 4, mask & !1);
		}
		// Enable MSI with a single message.
		let msg_ctrl = (msg_ctrl & !0x70) | 1;
		pci_cfg_write32(bdf, cap, (header & 0xFFFF) | (msg_ctrl << 16));
		true
	}

	/// ## `service_events` method
	/// Log and consume the events in the event log. \
	/// Returns `false` without side effects if this IOMMU didn't raise the event log interrupt.
	fn service_events(&self) -> bool
	{
		let virt = self.virt();
		let status = unsafe { IommuStatusRegister::read(virt) };
		if !(status.event_log_int() || status.event_overflow())
		{
			return false;
		}
		// Acknowledge the interrupt (RW1C) before draining the log. Events logged afterwards raise another interrupt.
		unsafe {
			IommuStatusRegister::new().with_event_log_int(status.event_log_int()).write(virt);
		}
		let tail = unsafe { EventBufferTailPointerRegister::read(virt) }.event_tail_ptr();
		let mut head = unsafe { EventBufferHeadPointerRegister::read(virt) }.event_head_ptr();
		while head != tail
		{
			let entry = unsafe { self.log_base.virt.add(head).read_volatile() };
			self.log_event(entry);
			head = (head + 1) % EVENT_LOG_ENTRIES;
		}
		unsafe {
			EventBufferHeadPointerRegister::new().with_event_head_ptr(head).write(virt);
		}
		if status.event_overflow()
		{
			warn!("Event log of IOMMU 0x{:X} overflowed! Some events are discarded!", self.bar.phys);
			self.restart_event_log();
		}
		true
	}

	/// ## `restart_event_log` method
	/// Event logging stops on overflow. Restart it with the procedure specified by AMD-Vi manual.
	fn restart_event_log(&self)
	{
		let virt = self.virt();
		unsafe {
			while IommuStatusRegister::read(virt).event_log_run()
			{
				spin_loop();
			}
			let ctrl = IommuControlRegister::read(virt);
			ctrl.with_event_log_en(false).write(virt);
			IommuStatusRegister::new().with_event_overflow(true).write(virt);
			ctrl.with_event_log_en(true).write(virt);
		}
	}

	/// ## `log_event` method
	/// Log an entry of the event log.
	fn log_event(&self, entry: u128)
	{
		let code = (entry >> 60) as u8 & 0xF;
		let operand1 = entry as u64 & ((1 << 60) - 1);
		let operand2 = (entry >> 64) as u64;
		if code == EVENT_CODE_IO_PAGE_FAULT
		{
			let device_id = entry as u16;
			let flags = (entry >> 32) as u32;
			let present = flags & (1 << 20) != 0;
			let access = if flags & (1 << 19) != 0
			{
				"Interrupt"
			}
			else if flags & (1 << 21) != 0
			{
				"Write"
			}
			else
			{
				"Read"
			};
			warn!(
				"DMA fault is reported by IOMMU 0x{:X}! Source: {:02X}:{:02X}.{}, Address: 0x{operand2:X}, Access: {access}, Present: {present}, Flags: 0x{:03X}",
				self.bar.phys,
				device_id >> 8,
				(device_id >> 3) & 0x1F,
				device_id & 7,
				(flags >> 16) & 0xFFF
			);
		}
		else
		{
			warn!(
				"IOMMU 0x{:X} reported {} event! Operands: 0x{operand1:015X}, 0x{operand2:016X}",
				self.bar.phys, EVENT_NAMES[code as usize]
			);
		}
	}

	fn wait(&self)
	{
		let virt = self.bar.virt.load(Ordering::Relaxed);
		let i = self.cmd_index.load(Ordering::SeqCst);
		let tail = CommandBufferTailPointerRegister::new().with_cmd_tail_ptr(i);
		unsafe {
			tail.write(virt);
		}
		let signal = AtomicU64::new(0);
		let compl_wait_cmd = CompletionWaitCommand::new()
			.with_s(true)
			.with_f(true)
			.with_store_data(1)
			.with_store_address(unsafe { noir_get_physical_address(signal.as_ptr().cast()) } >> 3);
		self.issue_cmd(compl_wait_cmd);
		unsafe {
			tail.with_cmd_tail_ptr(tail.cmd_tail_ptr() + 1).write(virt);
		}
		while signal.load(Ordering::SeqCst) == 0
		{
			spin_loop();
		}
	}

	fn issue_cmd(&self, cmd: impl CommandBufferItem) -> usize
	{
		let i = self.cmd_index.fetch_add(1, Ordering::SeqCst);
		self.cmd_index.fetch_and(0xFF, Ordering::SeqCst);
		unsafe {
			*self.cmd_base.virt.add(i) = cmd.into();
		}
		i
	}
}

/// ## `AmdIommuEventHandler` structure
/// Handles event log interrupts of all AMD-Vi units. All units share the same vector.
pub struct AmdIommuEventHandler
{
	units: Vec<Arc<AmdIommuBar>>,
	/// Set if the last interrupt drained the event log. Another interrupt may be pending for the drained events.
	trailing_interrupt: bool,
}

impl InterruptOps for AmdIommuEventHandler
{
	fn name(&self) -> &str
	{
		"AMD-Vi Event Log"
	}

	fn base_count(&self) -> (u8, usize)
	{
		(IOMMU_FAULT_EVENT_VECTOR, 1)
	}

	fn handle(&mut self, _vector: u8, _context: *mut c_void) -> InterruptDisposition
	{
		// Service every unit: interrupts from several units may be coalesced into one interrupt.
		let mut claimed = false;
		for unit in &self.units
		{
			claimed |= unit.service_events();
		}
		if claimed
		{
			self.trailing_interrupt = true;
			InterruptDisposition::Handled
		}
		else if core::mem::take(&mut self.trailing_interrupt)
		{
			// Some implementations (e.g.: QEMU) send an interrupt for every event, rather than for the 0-to-1 transition of
			// `EventLogInt`. Events logged while the interrupt was in-service are drained already, but their interrupt is
			// still pending in the Local APIC. Absorb it. Otherwise, the guest would receive an interrupt it never expects,
			// and it may never issue EOI for it, which blocks all interrupts with lower priority (e.g.: the timer).
			// Interrupts are coalesced in the Local APIC, so there is one such interrupt at most.
			trace!("Absorbed the trailing interrupt of AMD-Vi event log!");
			InterruptDisposition::Handled
		}
		else
		{
			InterruptDisposition::Unclaimed
		}
	}
}

pub struct AmdIommuBarRegion(Arc<AmdIommuBar>);

impl IoRegionOps for AmdIommuBarRegion
{
	fn name(&self) -> &str
	{
		"AMD-Vi IOMMU BAR"
	}

	fn base_size(&self) -> (u64, usize)
	{
		(self.0.bar.phys, 4 << PAGE_SHIFT)
	}

	fn forward_input(&self) -> bool
	{
		false
	}

	fn forward_output(&self) -> bool
	{
		false
	}

	fn input(&mut self, _address: u64, value: &mut [u8], _context: *mut c_void) -> Status
	{
		// Ignore the input. Emulate none device.
		value.fill(u8::MAX);
		Status::SUCCESS
	}

	fn output(&mut self, _address: u64, _value: &[u8], _context: *mut c_void) -> Status
	{
		// Ignore the output.
		Status::SUCCESS
	}
}

pub(super) fn create_iommu(acpi_tables: &[*const IoVirtualizationReportingStructure]) -> Option<Box<dyn IommuOps>>
{
	let mut x: Box<AmdIommuManager> = unsafe {
		let mut x: Box<AmdIommuManager> = Box::new_uninit().assume_init();
		x.init();
		x
	};
	for &t in acpi_tables
	{
		let table = unsafe { &*t };
		let limit = table.header.length.get() as usize - size_of::<IoVirtualizationReportingStructure>();
		let mut cursor = 0;
		unsafe {
			// Destroy the IVRS table so that Guest OS will not try to run AMD-Vi IOMMU.
			let p = (&raw const (*t).header.signature) as *mut u32;
			p.write_unaligned(u32::from_ne_bytes(*b"????"));
		}
		while cursor < limit
		{
			unsafe {
				let ivdb: &Ivdb = &*table.ivdb.as_ptr().add(cursor).cast();
				enum IvdbOption<'a>
				{
					Ivhd10(&'a Ivhd10),
					Ivhd11(&'a Ivhd11),
					Ivhd40(&'a Ivhd40),
					Ivmd(&'a Ivmd),
					Unknown,
				}
				let opt = match ivdb.head.r#type
				{
					0x10 => IvdbOption::Ivhd10(&ivdb.ivhd10),
					0x11 => IvdbOption::Ivhd11(&ivdb.ivhd11),
					0x40 => IvdbOption::Ivhd40(&ivdb.ivhd40),
					0x20..0x22 => IvdbOption::Ivmd(&ivdb.ivmd),
					_ => IvdbOption::Unknown,
				};
				match opt
				{
					IvdbOption::Ivhd10(ivhd) =>
					{
						info!("IVHD-10H Features: {:X?}", ivhd.iommu_feature_reporting.get());
						let (bar, cap) = (ivhd.iommu_base_pa.get(), ivhd.capability_offset.get());
						x.push_bar(bar, cap, ivhd.device_id.get(), ivhd.pci_segment_group.get());
					}
					IvdbOption::Ivhd11(ivhd) =>
					{
						info!("IVHD-11H Attributes: {:X?}", ivhd.iommu_attributes.get());
						let (bar, cap) = (ivhd.iommu_base_pa.get(), ivhd.capability_offset.get());
						x.push_bar(bar, cap, ivhd.device_id.get(), ivhd.pci_segment_group.get());
					}
					IvdbOption::Ivhd40(ivhd) =>
					{
						info!("IVHD-40H Attributes: {:X?}", ivhd.iommu_attributes.get());
						let (bar, cap) = (ivhd.iommu_base_pa.get(), ivhd.capability_offset.get());
						x.push_bar(bar, cap, ivhd.device_id.get(), ivhd.pci_segment_group.get());
					}
					IvdbOption::Ivmd(ivmd) =>
					{
						info!("IVMD Start: 0x{:X}, Length: 0x{:X}", ivmd.start_address.get(), ivmd.block_length.get());
					}
					IvdbOption::Unknown =>
					{
						error!("Unknown IVDB Type: 0x{:X}!", ivdb.head.r#type);
					}
				}
				cursor += ivdb.head.length.get() as usize;
			}
		}
	}
	let mut req_sup = true;
	for iommu in &x.ivrs_units
	{
		let ext_cap = unsafe { IommuExtendedFeatureRegister::read(iommu.bar.virt.load(Ordering::Relaxed)) };
		info!("Extended Capability of IOMMU BAR 0x{:X}, {ext_cap:X?}", iommu.bar.phys);
		// Guest Translation must be supported in order to protect from DMA attacks.
		req_sup &= ext_cap.gt_sup();
		// Hardware Error Registers must be supported in order to keep track of errors.
		req_sup &= ext_cap.he_sup();
	}
	if req_sup
	{
		Some(x)
	}
	else
	{
		error!("The AMD-Vi implementation of this machine does not meet minimum requirement!");
		None
	}
}
