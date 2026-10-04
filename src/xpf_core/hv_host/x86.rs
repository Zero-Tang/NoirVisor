/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file lists builds x86 host environment for NoirVisor Core in Rust.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use alloc::vec::Vec;
use core::{arch::global_asm, cmp::Ordering, ffi::c_void, mem::offset_of};

use log::*;
use pastey::paste;

use crate::xpf_core::{
	asm::{crdr::*, misc::read_gs_ptr, seg::*},
	nvbdk::*,
	x86::{descriptors::*, interrupts::*, paging::*},
};

pub struct HostSystem
{
	pub paging: HostPaging,
	pub idt: HostIDT,
}

impl HostSystem
{
	pub fn build() -> Self
	{
		let cs = read_cs();
		debug!("Default CS Selector: 0x{cs:04X}");
		Self { paging: HostPaging::default(), idt: HostIDT::build(cs) }
	}
}

#[derive(Default)]
#[repr(C)]
pub struct HostProcessor
{
	pub gdt: HostGDT,
	pub tss: TaskSegmentState64,
	pub tr_sel: u16,
}

impl HostProcessor
{
	pub fn build(&mut self, ist: &[*mut c_void; 8])
	{
		self.tss.ist1 = ist[1] as u64;
		self.tss.ist2 = ist[2] as u64;
		self.tss.ist3 = ist[3] as u64;
		self.tss.ist4 = ist[4] as u64;
		self.tss.ist5 = ist[5] as u64;
		self.tss.ist6 = ist[6] as u64;
		self.tss.ist7 = ist[7] as u64;
		let tss_d = SystemSegmentDescriptor::new(
			size_of::<TaskSegmentState64>() as u32 - 1,
			&self.tss as *const TaskSegmentState64 as u64,
			SegmentFlags::AVAILABLE_TSS,
			3,
			true,
		);
		let tr_sel = self.gdt.allocated;
		self.gdt.allocated += 16;
		self.gdt.write_sys_seg(tr_sel, tss_d);
		self.tr_sel = tr_sel;
	}
}

macro_rules! build_host_pxe_descriptor {
	($name:tt, $type:ty, $size:tt) => {
		paste!
		{
			struct [<Host $name Descriptor>]
			{
				descriptor:MemoryDescriptor<1,$type>,
				gpa_start:u64
			}

			impl [<Host $name Descriptor>]
			{
				const DESCRIPTOR_RANGE:u64=([<PAGE_ $size:upper _SIZE>]<<PAGE_SHIFT_DIFF64) as u64;
				const DESCRIPTOR_MASK_LO:u64=Self::DESCRIPTOR_RANGE-1;
				const DESCRIPTOR_MASK_HI:u64=!Self::DESCRIPTOR_MASK_LO;

				fn new(gpa_start:u64,identity_map:bool)->Self
				{
					let r=Self
					{
						descriptor:MemoryDescriptor::alloc().expect(concat!("Failed to allocate ",stringify!([<$name:upper>]," for host paging!"))),
						gpa_start
					};
					let s=unsafe{&mut *(r.descriptor.virt as *mut [$type;PAGE_TABLE_ENTRIES64])};
					for (i,x) in s.iter_mut().enumerate()
					{
						*x=if identity_map {$type::construct(true,true,false,r.gpa_start+[<page_ $size _mult>](i) as u64,false)} else {$type::new()};
					}
					r
				}

				fn compare_pa(&self,va:u64)->Ordering
				{
					if va<self.gpa_start
					{
						Ordering::Greater
					}
					else if va>=self.gpa_start+Self::DESCRIPTOR_RANGE
					{
						Ordering::Less
					}
					else
					{
						Ordering::Equal
					}
				}
			}
		}
	};
}

build_host_pxe_descriptor!(Pdpte, HugePdpte, 1gb);
build_host_pxe_descriptor!(Pde, LargePde, 2mb);
build_host_pxe_descriptor!(Pte, Pte, 4kb);

pub struct HostPaging
{
	pub cr3: MemoryDescriptor<1, Pml4e>,
	pdpt: Vec<HostPdpteDescriptor>,
	pde: Vec<HostPdeDescriptor>,
	pte: Vec<HostPteDescriptor>,
}

impl Default for HostPaging
{
	/// # `HostPaging::default`
	/// This method copies the current paging structure and creates a paging structure that maps lowest 512GiB with identity mapping.
	/// It is your responsibility to write CR3.
	fn default() -> Self
	{
		let mut r = HostPaging {
			cr3: MemoryDescriptor::null(),
			pdpt: Vec::with_capacity(8),
			pde: Vec::with_capacity(8),
			pte: Vec::with_capacity(8),
		};
		r.pdpt.push(HostPdpteDescriptor::new(0, true));
		match MemoryDescriptor::alloc()
		{
			Some(md) => r.cr3 = md,
			None => panic!("Failed to allocate host paging base!"),
		}
		let pml4e_p = r.cr3.virt;
		unsafe {
			// CR3 might contain PCID. Clear it.
			let scr3_phys = page_4kb_base(read_cr3());
			let scr3_virt = noir_find_virt_by_phys(scr3_phys);
			debug!("System CR3 Virt: {scr3_virt:p}, Phys: 0x{scr3_phys:016X}");
			memcpy(r.cr3.virt.cast(), scr3_virt, PAGE_SIZE);
			let pml4e_v = Pml4e::construct(true, true, false, r.pdpt[0].descriptor.phys, false);
			debug!("PML4E Pointer: {:p}, PML4E value 0x{:016X}", pml4e_p, pml4e_v.into_bits());
			pml4e_p.write(pml4e_v);
		}
		r
	}
}

impl HostPaging
{
	fn split_pml4e(&mut self, gpa: u64) -> usize
	{
		match self.pdpt.binary_search_by(|d| d.compare_pa(gpa))
		{
			Ok(i) => i,
			Err(i) =>
			{
				// No descriptor found! Create one.
				let r = HostPdpteDescriptor::new(gpa & HostPdpteDescriptor::DESCRIPTOR_MASK_HI, false);
				unsafe {
					self.cr3.virt.add(pml4e_index(gpa)).write(Pml4e::construct(true, true, false, r.descriptor.phys, false));
				}
				self.pdpt.insert(i, r);
				i
			}
		}
	}

	fn split_pdpte(&mut self, gpa: u64) -> usize
	{
		match self.pde.binary_search_by(|d| d.compare_pa(gpa))
		{
			Ok(i) => i,
			Err(i) =>
			{
				// No descriptor found! Create one.
				let j = self.split_pml4e(gpa);
				let r = HostPdeDescriptor::new(gpa & HostPdeDescriptor::DESCRIPTOR_MASK_HI, false);
				unsafe {
					self.pdpt[j]
						.descriptor
						.virt
						.add(pdpte_index(gpa))
						.write(HugePdpte::from_bits(Pdpte::construct(true, true, false, r.descriptor.phys, false).into_bits()));
				}
				self.pde.insert(i, r);
				i
			}
		}
	}

	fn split_pde(&mut self, gpa: u64) -> usize
	{
		match self.pte.binary_search_by(|d| d.compare_pa(gpa))
		{
			Ok(i) => i,
			Err(i) =>
			{
				// No descriptor found! Create one.
				let j = self.split_pdpte(gpa);
				let r = HostPteDescriptor::new(gpa & HostPteDescriptor::DESCRIPTOR_MASK_HI, false);
				unsafe {
					self.pde[j]
						.descriptor
						.virt
						.add(pde_index(gpa))
						.write(LargePde::from_bits(Pdpte::construct(true, true, false, r.descriptor.phys, false).into_bits()));
				}
				self.pte.insert(i, r);
				i
			}
		}
	}

	fn update_pte(&mut self, va: u64, pa: u64, p: bool, w: bool, nx: bool)
	{
		let i = self.split_pde(va);
		unsafe {
			*self.pte[i].descriptor.virt.add(pte_index(va)) = Pte::construct(p, w, false, pa, nx);
		}
	}

	pub fn map_4kb_ranges(&mut self, start_va: u64, start_pa: u64, pages: usize, p: bool, w: bool, nx: bool)
	{
		for i in 0..pages as u64
		{
			let j = page_4kb_mult(i);
			self.update_pte(start_va + j, start_pa + j, p, w, nx);
		}
	}
}

#[repr(C, align(16))]
pub struct HostGDT
{
	allocated: u16,
	raw: [u8; 256],
}

impl Default for HostGDT
{
	fn default() -> Self
	{
		let mut r = HostGDT { allocated: 0, raw: [0; 256] };
		let gdtr = read_gdtr();
		unsafe {
			// Copy from current system.
			let l = if gdtr.limit < r.raw.len() as u16 { gdtr.limit as usize } else { r.raw.len() };
			memcpy(r.raw.as_mut_ptr().cast(), gdtr.base as *mut c_void, l);
		}
		r.allocated = gdtr.limit + 1;
		r
	}
}

impl HostGDT
{
	pub fn write_user_seg(&mut self, selector: u16, segment: UserSegmentDescriptor)
	{
		unsafe {
			*self.raw.as_mut_ptr().byte_add(selector as usize).cast() = segment;
		}
	}

	pub fn write_sys_seg(&mut self, selector: u16, segment: SystemSegmentDescriptor)
	{
		unsafe {
			*self.raw.as_mut_ptr().byte_add(selector as usize).cast() = segment;
		}
	}

	pub fn get_reg(&self) -> DescriptorTable
	{
		DescriptorTable { limit: self.raw.len() as u16 - 1, base: self.raw.as_ptr() as u64 }
	}
}

#[repr(C, align(16))]
pub struct HostIDT
{
	idt: [GateDescriptor; 256],
}

impl HostIDT
{
	/// ## `HostIDT::set_idt_entry`
	/// Sets an IDT entry for specified `vector` with `selector` and `entry`
	pub fn set_idt_entry(&mut self, vector: u8, selector: u16, entry: AsmInterruptHandler, ist: u16)
	{
		// The `unwrap` shouldn't fail since `DPL` and `IST` are zero.
		self.idt[vector as usize] = GateDescriptor::new_intgate(entry, selector, 0, ist).unwrap();
	}

	/// ## `HostIDT::build`
	/// Builds IDT on heap.
	pub fn build(selector: u16) -> HostIDT
	{
		let mut r = HostIDT { idt: [GateDescriptor::default(); 256] };
		r.set_idt_entry(DIVIDE_ERROR_FAULT, selector, noir_divide_error_fault_handler_a, 0);
		r.set_idt_entry(DEBUG_FAULT_OR_TRAP, selector, noir_debug_fault_trap_handler_a, 0);
		r.set_idt_entry(BREAKPOINT_TRAP, selector, noir_breakpoint_trap_handler_a, 0);
		r.set_idt_entry(OVERFLOW_TRAP, selector, noir_overflow_trap_handler_a, 0);
		r.set_idt_entry(EXCEED_BOUND_RANGE_FAULT, selector, noir_bound_range_fault_handler_a, 0);
		r.set_idt_entry(INVALID_OPCODE_FAULT, selector, noir_invalid_opcode_fault_handler_a, 1);
		r.set_idt_entry(NO_MATH_COPROCESSOR_FAULT, selector, noir_device_not_available_fault_handler_a, 0);
		r.set_idt_entry(DOUBLE_FAULT_ABORT, selector, noir_double_fault_abort_handler_a, 1);
		r.set_idt_entry(INVALID_TSS_FAULT, selector, noir_invalid_tss_fault_handler_a, 0);
		r.set_idt_entry(SEGMENT_ABSENT_FAULT, selector, noir_segment_not_present_fault_handler_a, 0);
		r.set_idt_entry(STACK_FAULT, selector, noir_stack_fault_handler_a, 1);
		r.set_idt_entry(GENERAL_PROTECTION_FAULT, selector, noir_general_protection_fault_handler_a, 1);
		r.set_idt_entry(PAGE_FAULT, selector, noir_page_fault_handler_a, 1);
		r.set_idt_entry(X87_FP_EXCEPTION_FAULT, selector, noir_x87_floating_point_fault_handler_a, 0);
		r.set_idt_entry(ALIGNMENT_CHECK_FAULT, selector, noir_alignment_check_fault_handler_a, 0);
		r.set_idt_entry(MACHINE_CHECK_ABORT, selector, noir_machine_check_abort_handler_a, 0);
		r.set_idt_entry(SIMD_FP_EXCEPTION_FAULT, selector, noir_simd_floating_point_fault_handler_a, 0);
		r.set_idt_entry(CONTROL_PROTECTION_FAULT, selector, noir_control_protection_fault_handler_a, 0);
		r
	}

	/// ## `HostIDT::get_reg`
	/// Returns a register which is supposed to be passed to the `lidt` instruction.
	pub fn get_reg(&self) -> DescriptorTable
	{
		DescriptorTable { limit: PAGE_SIZE as u16 - 1, base: self.idt.as_ptr() as u64 }
	}
}

#[derive(Debug, Default)]
#[repr(C)]
pub enum PerCpuGsTryExceptionState
{
	#[default]
	NotExpectingException,
	AwaitExecution,
	Failed
	{
		vector: u8,
		error_code: Option<u32>,
	},
	Successful,
}

#[derive(Default, Debug)]
#[repr(C, align(16))]
pub struct PerCpuGsState
{
	pub handler_rsp: u64,
	pub handler_rip: u64,
	pub this: *mut Self,
	pub host_vcpu: *mut c_void,
	pub state: PerCpuGsTryExceptionState,
	pub stack_base: u64,
}

impl PerCpuGsState
{
	pub fn reset(&mut self)
	{
		self.state = PerCpuGsTryExceptionState::AwaitExecution;
	}

	pub fn set(&mut self, vector: u8, error_code: Option<u32>)
	{
		self.state = PerCpuGsTryExceptionState::Failed { vector, error_code };
	}
}

pub type InterruptHandler = unsafe extern "C" fn(stack_frame: *mut InterruptStackFrame, gpr_state: *mut GprState);
pub type InterruptHandlerWithErrorCode =
	unsafe extern "C" fn(stack_frame: *mut InterruptStackFrameWithErrorCode, gpr_state: *mut GprState);
pub type AsmInterruptHandler = unsafe extern "C" fn() -> !;

#[inline(always)]
fn handle_exception_without_error_code(
	exception_frame: *mut InterruptStackFrame,
	gpr_state: *mut GprState,
	vector: u8,
	exception_name: &str,
)
{
	use PerCpuGsTryExceptionState::*;
	let frame = unsafe { &mut *exception_frame };
	let gs_ctxt: &mut PerCpuGsState = unsafe { &mut *read_gs_ptr(offset_of!(PerCpuGsState, this)) };
	error!("{exception_name} happened!");
	error!("Dumping Exception Frame:\n{}", frame);
	error!("Dumping GPR State:\n{}", unsafe { &*gpr_state });
	debug!("Current GS-Base: {:p}", gs_ctxt as *mut PerCpuGsState);
	debug!("Current GS-State: {:?}", gs_ctxt.state);
	match &gs_ctxt.state
	{
		AwaitExecution =>
		{
			frame.return_rip = gs_ctxt.handler_rip;
			frame.return_rsp = gs_ctxt.handler_rsp;
			gs_ctxt.set(vector, None);
			info!("Returning to host with rip=0x{:X}, rsp=0x{:X}...", frame.return_rip, frame.return_rsp);
		}
		_ =>
		unsafe {
			panic!("Host is not expecting for exception! Frame: {}, Vector: {vector} ({exception_name})", *exception_frame);
		},
	}
}

#[inline(always)]
fn handle_exception_with_error_code(
	exception_frame: *mut InterruptStackFrameWithErrorCode,
	gpr_state: *mut GprState,
	vector: u8,
	exception_name: &str,
)
{
	use PerCpuGsTryExceptionState::*;
	let frame = unsafe { &mut *exception_frame };
	let gs_ctxt: &mut PerCpuGsState = unsafe { &mut *read_gs_ptr(offset_of!(PerCpuGsState, this)) };
	error!("{exception_name} happened!");
	error!("Dumping Exception Frame:\n{}", frame);
	error!("Dumping GPR State:\n{}", unsafe { &*gpr_state });
	debug!("Current GS-Base: {:p}", gs_ctxt as *mut PerCpuGsState);
	debug!("Current GS-State: {:?}", gs_ctxt.state);
	match &gs_ctxt.state
	{
		AwaitExecution =>
		{
			frame.return_rip = gs_ctxt.handler_rip;
			frame.return_rsp = gs_ctxt.handler_rsp;
			gs_ctxt.set(vector, Some(frame.error_code));
			info!("Returning to host...");
		}
		_ => panic!("Host is not expecting for exception!"),
	}
}

/// ## Vector 0 #DE - Divide-by-Zero Error Fault
/// This function handles `#DE` (Divide-by-Zero Error) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_divide_error_fault_handler(exception_frame: *mut InterruptStackFrame, gpr_state: *mut GprState)
{
	handle_exception_without_error_code(exception_frame, gpr_state, DIVIDE_ERROR_FAULT, "Divide-Error Fault");
}

/// ## Vector 1 #DB - Debug Fault or Trap
/// This function handles `#DB` (Debug) fault/trap.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_debug_fault_trap_handler(exception_frame: *mut InterruptStackFrame, gpr_state: *mut GprState)
{
	handle_exception_without_error_code(exception_frame, gpr_state, DEBUG_FAULT_OR_TRAP, "Debug Fault/Trap");
}

/// ## Vector 3 #BP - Breakpoint Trap
/// This function handles `#BP` (Breakpoint) trap.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_breakpoint_trap_handler(exception_frame: *mut InterruptStackFrame, gpr_state: *mut GprState)
{
	unsafe {
		error!("Dumping Exception Frame:\n{}", *exception_frame);
		error!("Dumping GPR State:\n{}", *gpr_state);
		panic!("Breakpoint Trap happened!");
	}
}

/// ## Vector 4 #OF - Overflow Trap
/// This function handles `#OF` (Overflow) trap.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_overflow_trap_handler(exception_frame: *mut InterruptStackFrame, gpr_state: *mut GprState)
{
	unsafe {
		error!("Dumping Exception Frame:\n{}", *exception_frame);
		error!("Dumping GPR State:\n{}", *gpr_state);
		panic!("Overflow Trap happened!");
	}
}

/// Vector 5 #BR - Bound-Range Fault
/// This function handles `#BR` (Bound-Range Exceeded) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_bound_range_fault_handler(exception_frame: *mut InterruptStackFrame, gpr_state: *mut GprState)
{
	handle_exception_without_error_code(exception_frame, gpr_state, EXCEED_BOUND_RANGE_FAULT, "Bound-Range Fault");
}

/// ## Vector 6 #UD - Invalid-Opcode Fault
/// This function handles `#UD` (Invalid-Opcode) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_invalid_opcode_fault_handler(exception_frame: *mut InterruptStackFrame, gpr_state: *mut GprState)
{
	handle_exception_without_error_code(exception_frame, gpr_state, INVALID_OPCODE_FAULT, "Invalid-Opcode Fault");
}

/// ## Vector 7 #NM - Device-Not-Available Fault
/// This function handles `#NM` (Device-Not-Available) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_device_not_available_fault_handler(exception_frame: *mut InterruptStackFrame, gpr_state: *mut GprState)
{
	handle_exception_without_error_code(exception_frame, gpr_state, NO_MATH_COPROCESSOR_FAULT, "Device-Not-Available Fault");
}

/// ## Vector 8 #DF - Double-Fault Abort
/// This function handles `#DF` (Double-Fault) abort.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_double_fault_abort_handler(exception_frame: *mut InterruptStackFrameWithErrorCode, gpr_state: *mut GprState)
{
	unsafe {
		error!("Dumping Exception Frame:\n{}", *exception_frame);
		error!("Dumping GPR State:\n{}", *gpr_state);
		panic!("Double-Fault Abort happened!");
	}
}

/// ## Vector 10 #TS - Invalid TSS Fault
/// This function handles `#TS` (Invalid-TSS) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_invalid_tss_fault_handler(exception_frame: *mut InterruptStackFrameWithErrorCode, gpr_state: *mut GprState)
{
	handle_exception_with_error_code(exception_frame, gpr_state, INVALID_TSS_FAULT, "Invalid-TSS Fault");
}

/// ## Vector 11 #NP - Segment-Not-Present Fault
/// This function handles `#NP` (Segment-Not-Present) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_segment_not_present_fault_handler(
	exception_frame: *mut InterruptStackFrameWithErrorCode,
	gpr_state: *mut GprState,
)
{
	handle_exception_with_error_code(exception_frame, gpr_state, SEGMENT_ABSENT_FAULT, "Segment-Not-Present Fault");
}

/// ## Vector 12 #SS - Stack Fault
/// This function handles `#SS` (Stack) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_stack_fault_handler(exception_frame: *mut InterruptStackFrameWithErrorCode, gpr_state: *mut GprState)
{
	handle_exception_with_error_code(exception_frame, gpr_state, STACK_FAULT, "Stack Fault");
}

/// ## Vector 13 #GP - General-Protection Fault
/// This function handles `#GP` (General-Protection) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_general_protection_fault_handler(
	exception_frame: *mut InterruptStackFrameWithErrorCode,
	gpr_state: *mut GprState,
)
{
	handle_exception_with_error_code(exception_frame, gpr_state, GENERAL_PROTECTION_FAULT, "General-Protection Fault");
}

/// ## Vector 14 #PF - Page Fault
/// This function handles `#PF` (Page) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_page_fault_handler(exception_frame: *mut InterruptStackFrameWithErrorCode, gpr_state: *mut GprState)
{
	let cr2 = read_cr2();
	// Check if the #PF is caught in the stack guard page.
	let gs_ctxt: &mut PerCpuGsState = unsafe { &mut *read_gs_ptr(offset_of!(PerCpuGsState, this)) };
	let guard_end = gs_ctxt.stack_base;
	let guard_start = guard_end - PAGE_4KB_SIZE as u64;
	// If the CR2 is in the guard page, then it's a stack-overflow we've caught.
	if (guard_start..guard_end).contains(&cr2)
	{
		panic!("Stack-Overflow is caught! rip=0x{:X}, cr2=0x{cr2:X}", unsafe { (*exception_frame).return_rip });
	}
	error!("Page-Fault CR2=0x{cr2:X}");
	handle_exception_with_error_code(exception_frame, gpr_state, PAGE_FAULT, "Page Fault");
}

/// ## Vector 16 #MF - x87 Floating-Point Exception-Pending Fault
/// This function handles `#MF` (x87 Floating-Point Exception-Pending) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_x87_floating_point_fault_handler(exception_frame: *mut InterruptStackFrame, gpr_state: *mut GprState)
{
	handle_exception_without_error_code(
		exception_frame,
		gpr_state,
		X87_FP_EXCEPTION_FAULT,
		"x87 Floating-Point Exception-Pending Fault",
	);
}

/// ## Vector 17 #AM - Alignment-Check Fault
/// This function handles `#AM` (Alignment-Check) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_alignment_check_fault_handler(
	exception_frame: *mut InterruptStackFrameWithErrorCode,
	gpr_state: *mut GprState,
)
{
	handle_exception_with_error_code(exception_frame, gpr_state, ALIGNMENT_CHECK_FAULT, "Alignment-Check Fault");
}

/// ## Vector 18 #MC - Machine-Check Abort
/// This function handles `#MC` (Machine-Check) abort.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_machine_check_abort_handler(exception_frame: *mut InterruptStackFrame, gpr_state: *mut GprState)
{
	unsafe {
		error!("Dumping Exception Frame:\n{}", *exception_frame);
		error!("Dumping GPR State:\n{}", *gpr_state);
		panic!("Machine-Check Abort happened!");
	}
}

/// ## Vector 19 #XF - SIMD Floating-Point Fault
/// This function handles `#XF` (SIMD Floating-Point) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_simd_floating_point_fault_handler(exception_frame: *mut InterruptStackFrame, gpr_state: *mut GprState)
{
	handle_exception_without_error_code(exception_frame, gpr_state, SIMD_FP_EXCEPTION_FAULT, "SIMD Floating-Point Fault");
}

/// ## Vector 21 #CP - Control-Protection Fault
/// This function handles `#CP` (Control-Protection) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[unsafe(no_mangle)]
unsafe extern "C" fn noir_control_protection_fault_handler(
	exception_frame: *mut InterruptStackFrameWithErrorCode,
	gpr_state: *mut GprState,
)
{
	handle_exception_with_error_code(exception_frame, gpr_state, CONTROL_PROTECTION_FAULT, "Control-Protection Fault");
}

unsafe extern "C" {
	fn noir_divide_error_fault_handler_a() -> !;
	fn noir_debug_fault_trap_handler_a() -> !;
	fn noir_breakpoint_trap_handler_a() -> !;
	fn noir_overflow_trap_handler_a() -> !;
	fn noir_bound_range_fault_handler_a() -> !;
	fn noir_invalid_opcode_fault_handler_a() -> !;
	fn noir_device_not_available_fault_handler_a() -> !;
	fn noir_double_fault_abort_handler_a() -> !;
	fn noir_invalid_tss_fault_handler_a() -> !;
	fn noir_segment_not_present_fault_handler_a() -> !;
	fn noir_stack_fault_handler_a() -> !;
	fn noir_general_protection_fault_handler_a() -> !;
	fn noir_page_fault_handler_a() -> !;
	fn noir_x87_floating_point_fault_handler_a() -> !;
	fn noir_alignment_check_fault_handler_a() -> !;
	fn noir_machine_check_abort_handler_a() -> !;
	fn noir_simd_floating_point_fault_handler_a() -> !;
	fn noir_control_protection_fault_handler_a() -> !;
}

global_asm!(include_str!("x86_interrupt.s"));
