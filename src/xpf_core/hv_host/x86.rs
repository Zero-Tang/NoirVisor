/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file lists builds x86 host environment for NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::ffi::c_void;

use crate::xpf_core::{asm::{crdr::*, seg::*, misc::get_rsp}, nvbdk::*, dlalloc::{alloc_contd_pages, free_contd_pages}, x86::{descriptors::*, interrupts::*, paging::*}};
use crate::*;

pub struct HostSystem
{
	pub paging:HostPaging,
	pub idt:HostIDT,
}

impl HostSystem
{
	pub fn build()->Self
	{
		let cs=read_cs();
		println!("Default CS Selector: 0x{:04X}",cs);
		Self
		{
			paging:HostPaging::default(),
			idt:HostIDT::build(cs),
		}
	}
}

#[derive(Default)]
#[repr(C)] pub struct HostProcessor
{
	pub gdt:HostGDT,
	pub tss:TaskSegmentState64,
	pub tr_sel:u16
}

impl HostProcessor
{
	pub fn build(&mut self,ist:&[*mut c_void])
	{
		self.tss.ist1=ist[1] as u64;
		self.tss.ist2=ist[2] as u64;
		self.tss.ist3=ist[3] as u64;
		self.tss.ist4=ist[4] as u64;
		self.tss.ist5=ist[5] as u64;
		self.tss.ist6=ist[6] as u64;
		self.tss.ist7=ist[7] as u64;
		let tss_d=SystemSegmentDescriptor::new(size_of::<TaskSegmentState64>() as u32 -1,&self.tss as *const TaskSegmentState64 as u64,GATE_DESCRIPTOR_AVAILABLE_TSS,3,true);
		let tr_sel=self.gdt.allocated;
		self.gdt.allocated+=16;
		self.gdt.write_sys_seg(tr_sel,tss_d);
		self.tr_sel=tr_sel;
	}
}

pub struct HostPaging
{
	pub cr3:MemoryDescriptor,
	pdpt:MemoryDescriptor
}

impl Default for HostPaging
{
	/// # `HostPaging::default`
	/// This method copies the current paging structure and creates a paging structure that maps lowest 512GiB with identity mapping.
	/// It is your responsibility to write CR3.
	fn default()->Self
	{
		let mut r=HostPaging
		{
			cr3:MemoryDescriptor::null(),
			pdpt:MemoryDescriptor::null()
		};
		match alloc_contd_pages(PAGE_SIZE*2)
		{
			Some(md)=>
			{
				r.cr3=md;
				r.pdpt=md.add(PAGE_SIZE);
				let pdpte_p=r.pdpt.virt as *mut HugePdpte;
				for i in 0..PAGE_TABLE_ENTRIES
				{
					unsafe
					{
						let pdpte_v=HugePdpte::new(true,true,false,false,false,page_1gb_mult(i) as u64);
						pdpte_p.add(i).write(pdpte_v);
					}
				}
			}
			None=>panic!("Failed to allocate host paging base!")
		}
		let pml4e_p=r.cr3.virt as *mut Pml4e;
		unsafe
		{
			let scr3_phys=read_cr3();
			let scr3_virt=noir_find_virt_by_phys(scr3_phys);
			println!("System CR3 Virt: {scr3_virt:p}, Phys: 0x{scr3_phys:016X}");
			memcpy(r.cr3.virt,scr3_virt,PAGE_SIZE);
			let pml4e_v=Pml4e::new(true,true,false,false,false,r.pdpt.phys);
			println!("PML4E Pointer: {:p}, PML4E value 0x{:016X}",pml4e_p,pml4e_v.0);
			pml4e_p.write(pml4e_v);
		}
		r
	}
}

impl Drop for HostPaging
{
	fn drop(&mut self)
	{
		println!("Dropping Host Paging...");
		if !self.cr3.virt.is_null()
		{
			free_contd_pages(self.cr3.virt,PAGE_SIZE*2);
		}
	}
}

#[repr(C,align(16))] pub struct HostGDT
{
	allocated:u16,
	raw:[u8;256]
}

impl Default for HostGDT
{
	fn default() -> Self
	{
		let mut r=HostGDT{allocated:0,raw:[0;256]};
		let gdtr=read_gdtr();
		unsafe
		{
			// Copy from current system.
			let l=if gdtr.limit<r.raw.len() as u16 {gdtr.limit as usize} else {r.raw.len()};
			memcpy(r.raw.as_mut_ptr().cast(),gdtr.base as *mut c_void,l);
		}
		r.allocated=gdtr.limit+1;
		r
	}
}

impl HostGDT
{
	pub fn write_user_seg(&mut self,selector:u16,segment:UserSegmentDescriptor)
	{
		unsafe
		{
			let d=self.raw.as_mut_ptr().byte_add(selector as usize) as *mut c_void;
			let s=&raw const segment as *const c_void;
			memcpy(d,s,size_of::<UserSegmentDescriptor>());
		}
	}

	pub fn write_sys_seg(&mut self,selector:u16,segment:SystemSegmentDescriptor)
	{
		unsafe
		{
			let d=self.raw.as_mut_ptr().byte_add(selector as usize) as *mut c_void;
			let s=&raw const segment as *const c_void;
			memcpy(d,s,size_of::<SystemSegmentDescriptor>());
		}
	}

	pub fn get_reg(&self)->DescriptorTable
	{
		DescriptorTable
		{
			limit:self.raw.len() as u16 - 1,
			base:self.raw.as_ptr() as u64
		}
	}
}

#[repr(C,align(16))] pub struct HostIDT
{
	idt:[GateDescriptor;256]
}

impl HostIDT
{
	/// ## `HostIDT::set_idt_entry`
	/// Sets an IDT entry for specified `vector` with `selector` and `entry`
	pub fn set_idt_entry(&mut self,vector:u8,selector:u16,entry:AsmInterruptHandler,ist:u16)
	{
		// The `unwrap` shouldn't fail since `DPL` and `IST` are zero.
		self.idt[vector as usize]=GateDescriptor::new_intgate(entry,selector,0,ist).unwrap();
	}

	/// ## `HostIDT::build`
	/// Builds IDT on heap.
	pub fn build(selector:u16)->HostIDT
	{
		let mut r=HostIDT{idt:[GateDescriptor::default();256]};
		r.set_idt_entry(DIVIDE_ERROR_FAULT,selector,noir_divide_error_fault_handler_a,0);
		r.set_idt_entry(DEBUG_FAULT_OR_TRAP,selector,noir_debug_fault_trap_handler_a,0);
		r.set_idt_entry(BREAKPOINT_TRAP,selector,noir_breakpoint_trap_handler_a,0);
		r.set_idt_entry(OVERFLOW_TRAP,selector,noir_overflow_trap_handler_a,0);
		r.set_idt_entry(EXCEED_BOUND_RANGE_FAULT,selector,noir_bound_range_fault_handler_a,0);
		r.set_idt_entry(INVALID_OPCODE_FAULT,selector,noir_invalid_opcode_fault_handler_a,1);
		r.set_idt_entry(NO_MATH_COPROCESSOR_FAULT,selector,noir_device_not_available_fault_handler_a,0);
		r.set_idt_entry(DOUBLE_FAULT_ABORT,selector,noir_double_fault_abort_handler_a,1);
		r.set_idt_entry(INVALID_TSS_FAULT,selector,noir_invalid_tss_fault_handler_a,0);
		r.set_idt_entry(SEGMENT_ABSENT_FAULT,selector,noir_segment_not_present_fault_handler_a,0);
		r.set_idt_entry(STACK_FAULT,selector,noir_stack_fault_handler_a,1);
		r.set_idt_entry(GENERAL_PROTECTION_FAULT,selector,noir_general_protection_fault_handler_a,1);
		r.set_idt_entry(PAGE_FAULT,selector,noir_page_fault_handler_a,1);
		r.set_idt_entry(X87_FP_EXCEPTION_FAULT,selector,noir_x87_floating_point_fault_handler_a,0);
		r.set_idt_entry(ALIGNMENT_CHECK_FAULT,selector,noir_alignment_check_fault_handler_a,0);
		r.set_idt_entry(MACHINE_CHECK_ABORT,selector,noir_machine_check_abort_handler_a,0);
		r.set_idt_entry(SIMD_FP_EXCEPTION_FAULT,selector,noir_simd_floating_point_fault_handler_a,0);
		r.set_idt_entry(CONTROL_PROTECTION_FAULT,selector,noir_control_protection_fault_handler_a,0);
		r
	}

	/// ## `HostIDT::get_reg`
	/// Returns a register which is supposed to be passed to the `lidt` instruction.
	pub fn get_reg(&self)->DescriptorTable
	{
		DescriptorTable
		{
			limit:PAGE_SIZE as u16 - 1,
			base:self.idt.as_ptr() as u64
		}
	}
}

pub type InterruptHandler=unsafe extern "C" fn(stack_frame:*mut InterruptStackFrame,gpr_state:*mut GprState);
pub type InterruptHandlerWithErrorCode=unsafe extern "C" fn(stack_frame:*mut InterruptStackFrameWithErrorCode,gpr_state:*mut GprState);
pub type AsmInterruptHandler=unsafe extern "C" fn()->!;

/// ## Vector 0 #DE - Divide-by-Zero Error Fault
/// This function handles `#DE` (Divide-by-Zero Error) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_divide_error_fault_handler(exception_frame:*mut InterruptStackFrame,gpr_state:*mut GprState)
{
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	panic!("Divide-Error Fault happened!");
}

/// ## Vector 1 #DB - Debug Fault or Trap
/// This function handles `#DB` (Debug) fault/trap.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_debug_fault_trap_handler(exception_frame:*mut InterruptStackFrame,gpr_state:*mut GprState)
{
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	panic!("Debug Fault/Trap happened!");
}

/// ## Vector 3 #BP - Breakpoint Trap
/// This function handles `#BP` (Breakpoint) trap.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_breakpoint_trap_handler(exception_frame:*mut InterruptStackFrame,gpr_state:*mut GprState)
{
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	panic!("Breakpoint Trap happened!");
}

/// ## Vector 4 #OF - Overflow Trap
/// This function handles `#OF` (Overflow) trap.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_overflow_trap_handler(exception_frame:*mut InterruptStackFrame,gpr_state:*mut GprState)
{
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	panic!("Overflow Trap happened!");
}

/// Vector 5 #BR - Bound-Range Fault
/// This function handles `#BR` (Bound-Range Exceeded) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_bound_range_fault_handler(exception_frame:*mut InterruptStackFrame,gpr_state:*mut GprState)
{
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	panic!("Bound-Range Fault happened!");
}

/// ## Vector 6 #UD - Invalid-Opcode Fault
/// This function handles `#UD` (Invalid-Opcode) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_invalid_opcode_fault_handler(exception_frame:*mut InterruptStackFrame,gpr_state:*mut GprState)
{
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	println!("Current rsp: 0x{:016X}",get_rsp());
	panic!("Invalid-Opcode Fault happened!");
}

/// ## Vector 7 #NM - Device-Not-Available Fault
/// This function handles `#NM` (Device-Not-Available) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_device_not_available_fault_handler(exception_frame:*mut InterruptStackFrame,gpr_state:*mut GprState)
{
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	panic!("Device-Not-Available Fault happened!\n{}Dumping GPR State...\n{}",*exception_frame,*gpr_state);
}

/// ## Vector 8 #DF - Double-Fault Abort
/// This function handles `#DF` (Double-Fault) abort.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_double_fault_abort_handler(exception_frame:*mut InterruptStackFrameWithErrorCode,gpr_state:*mut GprState)
{
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	panic!("Double-Fault Abort happened!");
}

/// ## Vector 10 #TS - Invalid TSS Fault
/// This function handles `#TS` (Invalid-TSS) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_invalid_tss_fault_handler(exception_frame:*mut InterruptStackFrameWithErrorCode,gpr_state:*mut GprState)
{
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	panic!("Invalid-TSS Fault happened!");
}

/// ## Vector 11 #NP - Segment-Not-Present Fault
/// This function handles `#NP` (Segment-Not-Present) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_segment_not_present_fault_handler(exception_frame:*mut InterruptStackFrameWithErrorCode,gpr_state:*mut GprState)
{
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	panic!("Segment-Not-Present Fault happened!");
}

/// ## Vector 12 #SS - Stack Fault
/// This function handles `#SS` (Stack) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_stack_fault_handler(exception_frame:*mut InterruptStackFrameWithErrorCode,gpr_state:*mut GprState)
{
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	panic!("Stack Fault happened!");
}

/// ## Vector 13 #GP - General-Protection Fault
/// This function handles `#GP` (General-Protection) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_general_protection_fault_handler(exception_frame:*mut InterruptStackFrameWithErrorCode,gpr_state:*mut GprState)
{
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	println!("Current rsp: 0x{:016X}",get_rsp());
	panic!("General-Protection Fault happened!");
}

/// ## Vector 14 #PF - Page Fault
/// This function handles `#PF` (Page) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_page_fault_handler(exception_frame:*mut InterruptStackFrameWithErrorCode,gpr_state:*mut GprState)
{
	let cr2=read_cr2();
	let err_code=PageFaultErrorCode::from_u32((*exception_frame).error_code);
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	panic!("Page Fault happened! Virtual-Address: 0x{:016X}, Error Reason: {}",cr2,err_code);
}

/// ## Vector 16 #MF - x87 Floating-Point Exception-Pending Fault
/// This function handles `#MF` (x87 Floating-Point Exception-Pending) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_x87_floating_point_fault_handler(exception_frame:*mut InterruptStackFrame,gpr_state:*mut GprState)
{
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	panic!("x87 Floating-Point Exception-Pending Fault happened!");
}

/// ## Vector 17 #AM - Alignment-Check Fault
/// This function handles `#AM` (Alignment-Check) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_alignment_check_fault_handler(exception_frame:*mut InterruptStackFrameWithErrorCode,gpr_state:*mut GprState)
{
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	panic!("Alignment-Check Fault happened!");
}

/// ## Vector 18 #MC - Machine-Check Abort
/// This function handles `#MC` (Machine-Check) abort.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_machine_check_abort_handler(exception_frame:*mut InterruptStackFrame,gpr_state:*mut GprState)
{
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	panic!("Machine-Check Abort happened!");
}

/// ## Vector 19 #XF - SIMD Floating-Point Fault
/// This function handles `#XF` (SIMD Floating-Point) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_simd_floating_point_fault_handler(exception_frame:*mut InterruptStackFrame,gpr_state:*mut GprState)
{
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	panic!("SIMD Floating-Point Fault happened!");
}

/// ## Vector 21 #CP - Control-Protection Fault
/// This function handles `#CP` (Control-Protection) fault.
/// # Safety
/// This function is called by assembly. *DO NOT CALL THIS FUNCTION FROM RUST!*
#[no_mangle] pub unsafe extern "C" fn noir_control_protection_fault_handler(exception_frame:*mut InterruptStackFrameWithErrorCode,gpr_state:*mut GprState)
{
	print!("Dumping Exception Frame:\n{}",*exception_frame);
	print!("Dumping GPR State:\n{}",*gpr_state);
	panic!("Control-Protection Fault happened!");
}

extern "C"
{
	fn noir_divide_error_fault_handler_a()->!;
	fn noir_debug_fault_trap_handler_a()->!;
	fn noir_breakpoint_trap_handler_a()->!;
	fn noir_overflow_trap_handler_a()->!;
	fn noir_bound_range_fault_handler_a()->!;
	fn noir_invalid_opcode_fault_handler_a()->!;
	fn noir_device_not_available_fault_handler_a()->!;
	fn noir_double_fault_abort_handler_a()->!;
	fn noir_invalid_tss_fault_handler_a()->!;
	fn noir_segment_not_present_fault_handler_a()->!;
	fn noir_stack_fault_handler_a()->!;
	fn noir_general_protection_fault_handler_a()->!;
	fn noir_page_fault_handler_a()->!;
	fn noir_x87_floating_point_fault_handler_a()->!;
	fn noir_alignment_check_fault_handler_a()->!;
	fn noir_machine_check_abort_handler_a()->!;
	fn noir_simd_floating_point_fault_handler_a()->!;
	fn noir_control_protection_fault_handler_a()->!;
}