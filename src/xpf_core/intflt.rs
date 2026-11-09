/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file defines Interrupt Filtering Architecture of NoirVisor Core in Rust.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use alloc::{
	alloc::{Allocator, Global},
	boxed::Box,
	vec::Vec,
};
use core::ffi::c_void;

use log::{error, info};
use nvcvm::status::Status;

use super::x86::interrupts::FIRST_EXTERNAL_INTERRUPT_VECTOR;

/// The number of interrupt vectors.
const VECTOR_COUNT: usize = 256;

/// ## `InterruptDisposition` enum
/// The result of handling an interrupt with a vector owned by the hypervisor.
///
/// As a bluepill hypervisor, NoirVisor can't reserve vectors from the guest. The guest may program its devices
/// with vectors owned by the hypervisor at any time. Therefore, an interrupt with an owned vector isn't necessarily
/// raised by the hypervisor's device. The handler must check its device to tell whether the interrupt is its own.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum InterruptDisposition
{
	/// The interrupt is raised by the hypervisor's device and has been handled. It is filtered out from the guest.
	Handled,
	/// The interrupt isn't raised by the hypervisor's device: its vector collides with a guest's device. \
	/// The interrupt is thrown back to the guest as if the vector weren't owned by the hypervisor.
	Unclaimed,
}

pub trait InterruptOps
{
	/// ## `name` method
	/// Returns the name of the interrupt handler.
	fn name(&self) -> &str;
	/// ## `base_count` method
	/// Returns the first vector and the number of vectors owned by this handler. \
	/// Interrupts with these vectors are dispatched to this handler before they go to the guest.
	fn base_count(&self) -> (u8, usize);
	/// ## `handle` method
	/// Handles an interrupt with a vector owned by this handler. \
	/// The interrupt is still in-service when this method is called. If the handler returns `Handled`, the caller \
	/// signals EOI after this method returns. Otherwise, the interrupt is thrown back to the guest.
	///
	/// Return `Unclaimed` without side effects if the hypervisor's device didn't raise this interrupt. \
	/// A thrown-back interrupt may be put back to the Local APIC if the guest can't take it right now. \
	/// In that case, this method will be called again for the same interrupt.
	fn handle(&mut self, vector: u8, context: *mut c_void) -> InterruptDisposition;
}

pub struct InterruptSpace<A: Allocator = Global>
{
	pub handlers: Vec<Box<dyn InterruptOps, A>, A>,
	/// Maps each vector to the index of its handler in `handlers`. \
	/// `None` indicates the vector is not owned by the hypervisor, so the interrupt belongs to the guest.
	owners: [Option<u8>; VECTOR_COUNT],
}

impl InterruptSpace
{
	pub const fn new() -> Self
	{
		Self { handlers: Vec::new(), owners: [None; VECTOR_COUNT] }
	}
}

impl Default for InterruptSpace
{
	fn default() -> Self
	{
		Self::new()
	}
}

impl<A: Allocator> InterruptSpace<A>
{
	/// ## `add_handler` method
	/// This method binds a handler to the vectors it claims. \
	/// The handler is rejected if it claims exception vectors, or any vector claimed by other handlers.
	pub fn add_handler(&mut self, handler: Box<dyn InterruptOps, A>) -> Result<(), Status>
	{
		let (base, count) = handler.base_count();
		let start = base as usize;
		let end = start + count;
		if count == 0 || base < FIRST_EXTERNAL_INTERRUPT_VECTOR || end > VECTOR_COUNT
		{
			error!("Handler \"{}\" claims invalid vectors (base=0x{base:02X}, count={count})!", handler.name());
			return Err(Status::INVALID_PARAMETER);
		}
		if let Some(v) = (start..end).find(|&v| self.owners[v].is_some())
		{
			error!("Handler \"{}\" claims vector 0x{v:02X}, which is already owned!", handler.name());
			return Err(Status::ACCESS_DENIED);
		}
		let Ok(index) = u8::try_from(self.handlers.len())
		else
		{
			error!("Too many interrupt handlers! Handler \"{}\" is rejected!", handler.name());
			return Err(Status::INSUFFICIENT_RESOURCES);
		};
		info!("Interrupt handler \"{}\" owns vectors 0x{start:02X}-0x{:02X}.", handler.name(), end - 1);
		self.owners[start..end].fill(Some(index));
		self.handlers.push(handler);
		Ok(())
	}

	/// ## `is_owned` method
	/// Check whether `vector` is owned by the hypervisor. \
	/// Interrupts with vectors not owned by the hypervisor belong to the guest.
	pub fn is_owned(&self, vector: u8) -> bool
	{
		self.owners[vector as usize].is_some()
	}

	/// ## `dispatch` method
	/// This method dispatches an interrupt to the corresponding handler. \
	/// Returns `Unclaimed` if `vector` is not owned by the hypervisor, so the interrupt goes to the guest.
	pub fn dispatch(&mut self, vector: u8, context: *mut c_void) -> InterruptDisposition
	{
		match self.owners[vector as usize]
		{
			Some(index) => self.handlers[index as usize].handle(vector, context),
			None => InterruptDisposition::Unclaimed,
		}
	}
}
