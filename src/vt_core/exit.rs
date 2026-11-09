/*
 * NoirVisor Core in Rust
 *
 * Copyright (c) Zero Tang, 2018-2026. All rights reserved.
 *
 * This file handles VM-Exits in Intel VT-x of NoirVisor Core in Rust.
 *
 * This program is distributed in the hope that it will be useful, but
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

use core::ffi::c_void;

use log::*;
use pastey::paste;

use super::{
	VtStackTop, VtVcpu,
	hvcall::dispatch_hypercall,
	ia32::{cpuid::CPUID_VMX, msr::*},
	vmcs::*,
};
use crate::{
	disasm::emulator::{EmulatorOps, Instruction},
	drv_core::lapic::*,
	mshv_core::{cpuid::MSHV_CPUID_HANDLERS, msr::dispatch_mshv_msr_handler},
	xpf_core::{
		allocator::is_allocated_page,
		asm::{cpuid::cpuid2, crdr::*, misc::wbinvd, msr::rdmsr, vt::*},
		ci::is_ci_phys_page,
		intflt::InterruptDisposition,
		x86::{
			cpuid::*,
			crdr::*,
			descriptors::SegmentFlags,
			interrupts::{EventType, GENERAL_PROTECTION_FAULT},
			msr::MSR_APIC_BASE,
		},
	},
	*,
};

const EVENT_TYPE_SOFTWARE_INTERRUPT: u32 = EventType::SoftwareInterrupt as u32;
const EVENT_TYPE_PRIVILEGED_SOFTWARE_EXCEPTION: u32 = EventType::PrivilegedSoftwareException as u32;
const EVENT_TYPE_SOFTWARE_EXCEPTION: u32 = EventType::SoftwareException as u32;

impl VtVcpu
{
	fn dump_current_vmcs(&mut self)
	{
		macro_rules! print_segment {
			($name:tt) => {
				paste! {
					let [<$name:lower>]=read_guest_segment!($name);
					error!("Guest {} Segment: {:X?}",stringify!($name:upper),[<$name:lower>]);
				}
			};
		}
		print_segment!(cs);
		print_segment!(ds);
		print_segment!(es);
		print_segment!(fs);
		print_segment!(gs);
		print_segment!(ss);
		print_segment!(tr);
		print_segment!(ldtr);
		let cr0 = vmreadptr(GUEST_CR0).unwrap();
		let cr3 = vmreadptr(GUEST_CR3).unwrap();
		let cr4 = vmreadptr(GUEST_CR4).unwrap();
		let dr7 = vmreadptr(GUEST_DR7).unwrap();
		error!("Guest CR0=0x{cr0:X}, CR3=0x{cr3:X}, CR4=0x{cr4:X}, DR7=0x{dr7:X}");
		let ssp = vmreadptr(GUEST_SSP);
		let rsp = vmreadptr(GUEST_RSP).unwrap();
		let rip = vmreadptr(GUEST_RIP).unwrap();
		let rflags = vmreadptr(GUEST_RFLAGS).unwrap();
		error!("Guest rsp: 0x{rsp:X}, rip: 0x{rip:X}, rflags: 0x{rflags:X}, ssp: 0x{ssp:X?}");
		let efer = vmread64(GUEST_MSR_IA32_EFER).unwrap();
		let pat = vmread64(GUEST_MSR_IA32_PAT).unwrap();
		let dbg_ctrl = vmread64(GUEST_MSR_IA32_DEBUG_CTRL).unwrap();
		error!("Guest EFER: 0x{efer:X}, PAT: 0x{pat:X}, Debug-Control: 0x{dbg_ctrl:X}");
	}

	fn handle_extint(&mut self, _context: &mut VtStackTop)
	{
		// The interrupt has been acknowledged on VM-Exit.
		let int_info = VmxExitInterruptionInformation::from_bits(vmread32(VMEXIT_INTERRUPTION_INFORMATION).unwrap());
		if !int_info.valid()
		{
			error!("External interrupt is intercepted without acknowledgement!");
			return;
		}
		let vector = int_info.vector();
		// The VM-Exit might occur during event delivery. Re-inject that event first.
		self.handle_idt_vectoring();
		if self.claim_host_interrupt(vector)
		{
			// This interrupt is raised by the hypervisor's device. Filter it out from the guest.
			return;
		}
		if self.can_inject_extint()
		{
			// The guest can take this interrupt right now.
			// This is exactly what would happen without virtualization.
			self.inject_extint(vector);
		}
		else if lapic_is_in_service(vector)
		{
			// The guest can't take this interrupt now, but it is already in-service in the physical Local APIC.
			// Keeping it in-service is dangerous: the guest may issue EOI for another interrupt in the meantime,
			// which would clear this vector instead and corrupt the in-service state of the Local APIC.
			// Therefore, put it back to the IRR and let the Local APIC hold it pending as if it were never acknowledged.
			// Since the APIC dispensed this vector, it must be the highest in-service vector. EOI clears exactly this vector.
			let level = lapic_is_level_triggered(vector);
			lapic_eoi();
			// For a level-triggered interrupt, the EOI broadcast lets the I/O APIC resend it as the line is still asserted.
			// Otherwise, we have to resend this interrupt by ourselves.
			if !level || lapic_is_eoi_broadcast_suppressed()
			{
				lapic_self_ipi(vector);
			}
			// The Local APIC keeps asserting this interrupt. Stop intercepting external interrupts until the guest can take
			// interrupts. Interrupt-window exits take priority over external interrupts and wake the processor from HLT,
			// so the guest could not receive any interrupts natively in the meantime.
			self.suspend_extint_exiting();
		}
		else if vector != lapic_spurious_vector()
		{
			// This interrupt is not tracked by the Local APIC (e.g.: ExtINT from 8259 PIC). It can't be put back.
			// Hold it pending and inject it when the guest opens the interrupt-window.
			// In fully-nested mode, the 8259 PIC dispenses an interrupt only if it has higher priority than all in-service
			// interrupts. Held interrupts remain in-service in the 8259 PIC as the guest hasn't issued EOI for them.
			// Therefore, every held interrupt has higher priority than those held before it.
			if self.held_irqs.push(vector).is_err()
			{
				error!("Too many held external interrupts! Vector 0x{vector:02X} is dropped!");
			}
			else
			{
				trace!(
					"External interrupt (vector=0x{vector:02X}) is held on processor {}! {} interrupt(s) are held.",
					self.vcpu_id,
					self.held_irqs.len()
				);
			}
			self.cached_ctxt.get_proc_ctrl1_mut().set_interrupt_window_exiting(true);
		}
		// Spurious interrupts don't require EOI. Discard it since the guest can't take it.
	}

	/// ## `claim_host_interrupt` method
	/// Dispatch an interrupt to the hypervisor's handler. Returns `true` if the hypervisor claimed and handled it. \
	/// Otherwise, the interrupt belongs to the guest. Its vector may collide with a guest's device.
	fn claim_host_interrupt(&mut self, vector: u8) -> bool
	{
		let hv: &mut VtHypervisor = unsafe { &mut *self.hypervisor.cast() };
		// The hypervisor's devices deliver interrupts through the Local APIC, so their interrupts must be in-service.
		// Interrupts not tracked by the Local APIC (e.g.: ExtINT from 8259 PIC) belong to the guest.
		if !hv.int_space.is_owned(vector) || !lapic_is_in_service(vector)
		{
			return false;
		}
		match hv.int_space.dispatch(vector, (self as *mut Self).cast())
		{
			InterruptDisposition::Handled =>
			{
				trace!("Hypervisor-owned interrupt (vector=0x{vector:02X}) is handled on processor {}!", self.vcpu_id);
				lapic_eoi();
				true
			}
			InterruptDisposition::Unclaimed =>
			{
				trace!(
					"Interrupt (vector=0x{vector:02X}) collides with the guest's device! It is thrown back to the guest on processor {}!",
					self.vcpu_id
				);
				false
			}
		}
	}

	/// ## `inject_extint` method
	/// Inject an external interrupt into the guest. Make sure the guest can take it before calling this method.
	fn inject_extint(&mut self, vector: u8)
	{
		// The injected interrupt wakes the guest from HLT state.
		vmwrite32(GUEST_ACTIVITY_STATE, ActivityState::ACTIVE);
		unsafe {
			inject_event(vector, EventType::ExternalInterrupt, None, true, 0);
		}
	}

	/// ## `suspend_extint_exiting` method
	/// Clear "external-interrupt exiting" until the guest opens the interrupt-window.
	fn suspend_extint_exiting(&mut self)
	{
		if !self.extint_exiting_suspended
		{
			let mut pin_ctrl = VmxPinBasedControls::from_bits(vmread32(PIN_BASED_VM_EXECUTION_CONTROLS).unwrap());
			pin_ctrl.set_external_interrupt_exiting(false);
			vmwrite32(PIN_BASED_VM_EXECUTION_CONTROLS, pin_ctrl.into_bits());
			self.extint_exiting_suspended = true;
		}
		self.cached_ctxt.get_proc_ctrl1_mut().set_interrupt_window_exiting(true);
	}

	fn handle_triple_fault(&mut self, _context: &mut VtStackTop)
	{
		panic!("Triple-Fault occured!");
	}

	fn handle_init(&mut self, context: &mut VtStackTop)
	{
		let gpr_state = &mut context.gpr_state;
		info!("INIT-signal is intercepted for CPU {}! rip=0x{:X}", self.vcpu_id, self.cached_ctxt.rip);
		unsafe {
			// General-Purpose Registers
			for i in 0..16usize
			{
				gpr_state.write(i, 0);
			}
			gpr_state.rdx = self.cpuid_fms as u64;
			vmwrite_unchecked(GUEST_RSP, 0);
			vmwrite_unchecked(GUEST_RIP, 0xFFF0);
			vmwrite_unchecked(GUEST_RFLAGS, 2);
			// Control Registers
			// CR0.ET is always set during INIT.
			let mut cr0 = Cr0::new().with_et(true);
			// Fix CR0 bits
			cr0 |= rdmsr(MSR_VMX_CR0_FIXED1);
			cr0 &= rdmsr(MSR_VMX_CR0_FIXED0);
			// CR0.PE and CR0.PG are cleared by INIT.
			cr0.set_pe(false);
			cr0.set_pg(false);
			vmwrite_unchecked(GUEST_CR0, cr0.into_bits() as usize);
			vmwrite_unchecked(CR0_READ_SHADOW, cr0.into_bits() as usize);
			write_cr2(0);
			vmwrite_unchecked(GUEST_CR3, 0);
			// CR4 is cleared to 0 upon INIT. But as a guest, CR4.VMXE must be set.
			let mut cr4 = 0;
			cr4 |= rdmsr(MSR_VMX_CR4_FIXED0) as usize;
			cr4 &= rdmsr(MSR_VMX_CR4_FIXED1) as usize;
			vmwrite_unchecked(GUEST_CR4, cr4);
			vmwrite_unchecked(CR4_READ_SHADOW, 0);
			// EFER is cleared to 0 upon INIT.
			vmwrite_unchecked(GUEST_MSR_IA32_EFER, 0);
			// Debug Registers
			write_dr0(0);
			write_dr1(0);
			write_dr2(0);
			write_dr3(0);
			write_dr6(0xffff0ff0);
			vmwrite_unchecked(GUEST_DR7, 0x400);
			// Segment Registers
			macro_rules! vmcs_write_seg {
				($name:tt,$sel:literal,$ar:expr,$lim:literal,$base:literal) => {
					paste! {
						vmwrite_unchecked([<GUEST_ $name:upper _SELECTOR>],$sel as usize);
						vmwrite_unchecked([<GUEST_ $name:upper _ACCESS_RIGHTS>],$ar as usize);
						vmwrite_unchecked([<GUEST_ $name:upper _LIMIT>],$lim as usize);
						vmwrite_unchecked([<GUEST_ $name:upper _BASE>],$base);
					}
				};
			}
			let mut ar = SegmentAccessRights::new();
			ar.set_segment_type(SegmentFlags::CODE_EXECUTE_READ_ACCESSED as u32);
			ar.set_descriptor_type(true);
			ar.set_present(true);
			vmcs_write_seg!(cs, 0xF000, ar.into_bits(), 0xFFFF, 0xFFFF0000);
			ar.set_segment_type(SegmentFlags::DATA_READ_WRITE_ACCESSED as u32);
			vmcs_write_seg!(ds, 0, ar.into_bits(), 0xFFFF, 0);
			vmcs_write_seg!(es, 0, ar.into_bits(), 0xFFFF, 0);
			vmcs_write_seg!(fs, 0, ar.into_bits(), 0xFFFF, 0);
			vmcs_write_seg!(gs, 0, ar.into_bits(), 0xFFFF, 0);
			vmcs_write_seg!(ss, 0, ar.into_bits(), 0xFFFF, 0);
			// LDTR & TR
			ar.set_segment_type(SegmentFlags::LDT as u32);
			ar.set_descriptor_type(false);
			vmcs_write_seg!(ldtr, 0, ar.into_bits(), 0xFFFF, 0);
			ar.set_segment_type(SegmentFlags::BUSY_TSS_16BIT as u32);
			vmcs_write_seg!(tr, 0, ar.into_bits(), 0xFFFF, 0);
			// IDTR & GDTR
			vmwrite_unchecked(GUEST_GDTR_BASE, 0);
			vmwrite_unchecked(GUEST_IDTR_BASE, 0);
			vmwrite_unchecked(GUEST_GDTR_LIMIT, 0xFFFF);
			vmwrite_unchecked(GUEST_IDTR_LIMIT, 0xFFFF);
			// VM-Entry Controls: Guest is definitely not in IA-32e mode.
			let mut entry_ctrl = VmxEntryControls::from_bits(vmread_unchecked(VMENTRY_CONTROLS) as u32);
			entry_ctrl.set_ia32e_mode_guest(false);
			vmwrite_unchecked(VMENTRY_CONTROLS, entry_ctrl.into_bits() as usize);
			// Upon INIT, vCPU enters inactive state to wait for Startup-IPI.
			vmwrite_unchecked(GUEST_ACTIVITY_STATE, ActivityState::WAIT_FOR_SIPI as usize);
		}
		info!("INIT-signal is completed for CPU {}! Awaiting SIPI...", self.vcpu_id);
	}

	fn handle_sipi(&mut self, _context: &mut VtStackTop)
	{
		unsafe {
			let vector = vmread_unchecked(VMEXIT_QUALIFICATION);
			info!("SIPI-signal is intercepted for CPU {}! Vector=0x{vector:X}", self.vcpu_id);
			vmwrite_unchecked(GUEST_CS_SELECTOR, vector << 8);
			vmwrite_unchecked(GUEST_CS_BASE, vector << 12);
			vmwrite_unchecked(GUEST_RIP, 0);
			// Startup-IPI is received. Resume to active state.
			vmwrite_unchecked(GUEST_ACTIVITY_STATE, ActivityState::ACTIVE as usize);
			// Invalid TLB since paging is switched off.
			let ivc = InvvpidContext::Single(vmread_unchecked(GUEST_VPID) as u16);
			invvpid(&ivc);
			// Dump some codes.
			let codes = &*((vector << 12) as *const [u8; 32]);
			info!("SIPI Vector Code Bytes: {codes:02X?}");
		}
	}

	fn handle_interrupt_window(&mut self, _context: &mut VtStackTop)
	{
		// Cancel interrupt-window exiting.
		self.cached_ctxt.get_proc_ctrl1_mut().set_interrupt_window_exiting(false);
		if self.extint_exiting_suspended
		{
			// Resume intercepting external interrupts.
			// The interrupt held pending in the Local APIC will be intercepted again right after VM-Entry.
			let mut pin_ctrl = VmxPinBasedControls::from_bits(vmread32(PIN_BASED_VM_EXECUTION_CONTROLS).unwrap());
			pin_ctrl.set_external_interrupt_exiting(true);
			vmwrite32(PIN_BASED_VM_EXECUTION_CONTROLS, pin_ctrl.into_bits());
			self.extint_exiting_suspended = false;
		}
		// Interrupt-window is quite an opportunity to inject a held external interrupt.
		// The most recently held interrupt has the highest priority.
		if let Some(&irq) = self.held_irqs.as_slice().last()
		{
			if self.can_inject_extint()
			{
				self.inject_extint(irq);
				self.held_irqs.pop();
				trace!(
					"Held external interrupt (vector=0x{irq:02X}) is injected on processor {}! {} interrupt(s) remain held.",
					self.vcpu_id,
					self.held_irqs.len()
				);
			}
			// Wait for another interrupt-window if there are more held interrupts.
			if !self.held_irqs.is_empty()
			{
				self.cached_ctxt.get_proc_ctrl1_mut().set_interrupt_window_exiting(true);
			}
		}
	}

	fn handle_cpuid(&mut self, context: &mut VtStackTop)
	{
		let gpr_state = &mut context.gpr_state;
		let hv: &VtHypervisor = unsafe { &*self.hypervisor.cast() };
		let ia = gpr_state.rax as u32;
		let ic = gpr_state.rcx as u32;
		let (a, b, c, d) = if (ia & 0x40000000) == 0x40000000
		{
			if hv.features.cpuid_hv_presence()
			{
				let leaf_func = (ia & 0x3FFFFFFF) as usize;
				match MSHV_CPUID_HANDLERS.get(leaf_func)
				{
					Some(f) => f(ia, ic),
					None => (0, 0, 0, 0),
				}
			}
			else
			{
				(0, 0, 0, 0)
			}
		}
		else
		{
			// This is processor's CPUID.
			// Execute the original CPUID and filter stuff.
			#[allow(unused_mut)]
			let (mut a, mut b, mut c, mut d) = cpuid2(ia, ic);
			if ia == CPUID_STD_PROCESSOR_FEATURE
			{
				c |= if hv.features.cpuid_hv_presence() { CPUID_UNDER_HYPERVISOR } else { 0 };
				c &= !CPUID_VMX;
				let cr4 = self.get_cr4();
				if !cr4.osxsave()
				{
					// The OSXSAVE bit of CPUID must equal to CR4.OSXSAVE!
					c &= !CPUID_OSXSAVE;
				}
			}
			(a, b, c, d)
		};
		// Write the results back to eax, ebx, ecx and edx and clear the higher 32 bits.
		gpr_state.rax = a as u64;
		gpr_state.rbx = b as u64;
		gpr_state.rcx = c as u64;
		gpr_state.rdx = d as u64;
		// Advance the rip.
		self.advance_rip();
	}

	fn handle_getsec(&mut self, _context: &mut VtStackTop)
	{
		error!("SMX Virtualization is not supported!");
		self.advance_rip();
	}

	fn handle_invd(&mut self, _context: &mut VtStackTop)
	{
		trace!("The invd instruction is executed!");
		// In Hyper-V, it invoked wbinvd at invd exit.
		wbinvd();
		self.advance_rip();
	}

	fn handle_vmcall(&mut self, context: &mut VtStackTop)
	{
		let gpr_state = &mut context.gpr_state;
		let vmcall_func = gpr_state.rcx as u32;
		let handler_fn = dispatch_hypercall(vmcall_func);
		match handler_fn(self, gpr_state.rdx as *mut c_void, gpr_state.r8 as usize)
		{
			Ok(st) =>
			{
				// This hypercall is known.
				gpr_state.rax = st.0 as u64;
				self.advance_rip();
			}
			Err((vector, error_code)) =>
			unsafe {
				// This hypercall is unknown.
				inject_event(vector, EventType::HardwareException, error_code, true, 0);
			},
		}
	}

	fn handle_cr_access(&mut self, context: &mut VtStackTop)
	{
		let gpr_state = &mut context.gpr_state;
		let q = ControlRegisterQualification::read();
		debug!("CR Index: {}, GPR Index: {}, Access: {}", q.cr_index(), q.gpr_index(), q.access_type());
		let mut should_advance: bool = true;
		match q.access_type()
		{
			ControlRegisterQualification::WRITE_CR =>
			{
				gpr_state.rsp = self.cached_ctxt.rsp;
				let new_value = gpr_state.read(q.gpr_index()).unwrap() as usize;
				debug!("New Value: 0x{new_value:X}");
				match q.cr_index()
				{
					0 =>
					{
						// Currently, NoirVisor only intercepts changes to CR0.PG.
						// As a result, TLBs must be flushed.
						let ivc = InvvpidContext::Single(1);
						unsafe {
							invvpid(&ivc);
						}
						// Fix CR0 value.
						let mut new_cr0 = Cr0::from_bits(new_value as u64);
						let mut fixed0_mask = Cr0::from_bits(rdmsr(MSR_VMX_CR0_FIXED0));
						fixed0_mask.set_pe(false);
						fixed0_mask.set_pg(false);
						new_cr0 |= fixed0_mask.into_bits();
						new_cr0 &= rdmsr(MSR_VMX_CR0_FIXED1);
						info!("Changed new cr0 to 0x{:X} (intended to be 0x{new_value:X})!", new_cr0.into_bits());
						vmwriteptr(GUEST_CR0, new_cr0.into_bits() as usize);
						vmwriteptr(CR0_READ_SHADOW, new_cr0.into_bits() as usize);
						// May affect EFER.LMA bit.
						let mut efer = self.get_efer();
						let pg = new_cr0.pg();
						let lme = efer.lme();
						if pg && lme
						{
							efer.set_lma(true);
						}
						vmwrite64(GUEST_MSR_IA32_EFER, efer.into_bits());
						// Also write to VM-Entry Controls.
						let mut entry_ctrl = VmxEntryControls::from_bits(vmread32(VMENTRY_CONTROLS).unwrap());
						entry_ctrl.set_ia32e_mode_guest(pg && lme);
						vmwrite32(VMENTRY_CONTROLS, entry_ctrl.into_bits());
					}
					4 =>
					{
						// Check if new CR4 will require flushing TLB...
						let old_cr4 = vmreadptr(GUEST_CR4).unwrap();
						let old_masked = old_cr4 & Cr4::TLB_FLUSH_MASK as usize;
						let new_masked = new_value & Cr4::TLB_FLUSH_MASK as usize;
						if old_masked != new_masked
						{
							let ivc = InvvpidContext::Single(1);
							unsafe {
								invvpid(&ivc);
							}
						}
						let mut new_cr4 = Cr4::from_bits(new_value as u64);
						new_cr4.set_vmxe(true);
						vmwriteptr(GUEST_CR4, new_cr4.into_bits() as usize);
						vmwriteptr(CR4_READ_SHADOW, new_value);
					}
					x =>
					{
						error!("Interception to CR{x} is unsupported!");
						should_advance = false;
					}
				};
			}
			_ =>
			{
				error!("Unrecognized Access: {}", q.access_type());
				should_advance = false;
			}
		}
		if should_advance
		{
			self.advance_rip();
		}
		else
		{
			unsafe {
				inject_event(GENERAL_PROTECTION_FAULT, EventType::HardwareException, Some(0), true, 0);
			}
		}
	}

	fn handle_rdmsr(&mut self, context: &mut VtStackTop)
	{
		let index: u32 = context.gpr_state.rcx as u32;
		let ret_val: Option<u64> = match index
		{
			// Prevent the Guest from updating microcode.
			// Returning u64::MAX should prevent the guest from loading microcodes,
			// unless they ignore the current version of microcode.
			MSR_BIOS_UPDATE_TRIGGER => Some(u64::MAX),
			(0x40000000..0x80000000) =>
			{
				let f = dispatch_mshv_msr_handler(index);
				let mut r: u64 = 0;
				if f(&mut self.mshv_ctxt, false, &mut r) { Some(r) } else { None }
			}
			_ =>
			{
				error!("Unexpected interception to rdmsr! MSR-Index: 0x{index:X}");
				None
			}
		};
		match ret_val
		{
			Some(v) =>
			{
				context.gpr_state.rax = v & u32::MAX as u64;
				context.gpr_state.rdx = v >> 32;
				self.advance_rip();
			}
			None =>
			unsafe { inject_event(GENERAL_PROTECTION_FAULT, EventType::HardwareException, Some(0), true, 0) },
		}
	}

	fn handle_wrmsr(&mut self, context: &mut VtStackTop)
	{
		let index: u32 = context.gpr_state.rcx as u32;
		let fault = match index
		{
			// Prevent the Guest from updating microcode.
			// Do so by ignoring the update request.
			MSR_BIOS_UPDATE_TRIGGER => false,
			// Track the mode of the Local APIC.
			MSR_APIC_BASE => !lapic_write_base((context.gpr_state.rax & 0xFFFFFFFF) | (context.gpr_state.rdx << 32)),
			(0x40000000..0x80000000) =>
			{
				let f = dispatch_mshv_msr_handler(index);
				let mut v = (context.gpr_state.rax & 0xFFFFFFFF) | (context.gpr_state.rdx << 32);
				!f(&mut self.mshv_ctxt, true, &mut v)
			}
			_ =>
			{
				error!("Unexpected interception to wrmsr! MSR-Index: 0x{index:X}");
				true
			}
		};
		unsafe {
			if fault
			{
				inject_event(GENERAL_PROTECTION_FAULT, EventType::HardwareException, Some(0), true, 0);
			}
			else
			{
				self.advance_rip();
			}
		};
	}

	fn handle_invalid_state(&mut self, _context: &mut VtStackTop)
	{
		self.dump_current_vmcs();
		panic!("Invalid Guest State!");
	}

	fn handle_invalid_auto_msr(&mut self, _context: &mut VtStackTop)
	{
		panic!("Invalid Auto-MSR List!");
	}

	fn handle_ept_violation(&mut self, _context: &mut VtStackTop)
	{
		let gpa = vmread64(GUEST_PHYSICAL_ADDRESS).unwrap();
		let rip = vmreadptr(GUEST_RIP).unwrap();
		let qual = EptViolationQualification::read();
		// IDT-vectoring may happen if the violation happened on IDT-page,
		// first instruction of IDT handler, GDT for selector, or maybe even the stack.
		self.handle_idt_vectoring();
		if qual.nmi_unblocking_due_to_iret()
		{
			// If this bit is set, the instruction causing EPT-violation is iret instruction.
			// This would cause NMIs to be prematurely unblocked. Therefore, keep it blocked.
			let interruptibility = self.cached_ctxt.get_interruptibility_mut();
			interruptibility.set_blocking_by_nmi(true);
		}
		if is_ci_phys_page(gpa) || is_allocated_page(gpa)
		{
			let mut inslen = self.cached_ctxt.exit_instruction_length();
			error!("CI-fault for GPA=0x{gpa:X} is intercepted! rip=0x{rip:X}, Instruction-Length: {inslen}");
			if inslen == 0
			{
				// VMware's nested virtualization does not forward instruction length upon EPT-violation.
				// Fetch instruction from guest and manually advance rip.
				warn!("Instruction-Length from VMCS is 0! Fetching instruction via software...");
				let mut instruction = Instruction::new(self.fetch_instruction());
				instruction.decode(self.get_current_bitness());
				inslen = instruction.len() as u32;
				debug!("CI-fault instruction bytes: {:02X?} | {}", &instruction.instruction_bytes[..inslen as usize], instruction);
			}
			self.advance_rip_manually(inslen);
		}
		else
		{
			panic!("Unexpected EPT Violation happened! GPA=0x{gpa:X}, rip=0x{rip:X}, Qualification: {qual:?}");
		}
	}

	fn handle_ept_misconfig(&mut self, _context: &mut VtStackTop)
	{
		let gpa = vmread64(GUEST_PHYSICAL_ADDRESS).unwrap();
		let hv: &mut VtHypervisor = unsafe { &mut *self.hypervisor.cast() };
		let p = unsafe { &mut *hv.eptm.locate_pdpte(gpa) };
		error!("Dumping EPT Page Entries for EPT Misconfiguration...");
		error!("EPT PDPTE Entry: 0x{:016X}", p.into_bits());
		if let Some(p) = hv.eptm.locate_pde(gpa)
		{
			error!("EPT PDE Entry: 0x{:016X}", unsafe { (*p).into_bits() });
		}
		if let Some(p) = hv.eptm.locate_pte(gpa)
		{
			error!("EPT PTE Entry: 0x{:016X}", unsafe { (*p).into_bits() });
		}
		panic!("EPT Misconfiguration happened! GPA=0x{gpa:X}");
	}

	fn handle_xsetbv(&mut self, context: &mut VtStackTop)
	{
		let gpr_state = &mut context.gpr_state;
		let index = gpr_state.rcx as u32;
		if index == 0
		{
			let stack = self.get_stack_top_mut();
			let value = Xcr0::from_bits((gpr_state.rax & u32::MAX as u64) | (gpr_state.rdx << 32));
			// x87 must be enabled in xcr0 in whatever circumstances.
			let mut valid = value.x87();
			// SSE must be enabled when AVX is enabled.
			valid &= if value.avx() { value.sse() } else { true };
			// AVX must be enabled when AVX-512 is enabled.
			valid &= if value.opmask() || value.zmm_hi256() || value.hi16_zmm() { value.avx() } else { true };
			// Reserved bits must be cleared.
			valid &= !value.rsvd0() && value.rsvd1() == 0;
			// LWP and X are AMD's bits. Intel did not define them.
			valid &= !(value.lwp() || value.x());
			// TODO: Check other bits of XCR0 (MPX, MPK, AMX, etc.)
			if valid
			{
				stack.guest_xcr0 = value.into_bits();
				self.advance_rip();
			}
			else
			{
				error!("Invalid XCR0 value (0x{:X}) is specified in xsetbv instruction!!", value.into_bits());
				unsafe {
					inject_event(GENERAL_PROTECTION_FAULT, EventType::HardwareException, Some(0), true, 0);
				}
			}
		}
		else
		{
			let value = (gpr_state.rax & u32::MAX as u64) | (gpr_state.rdx << 32);
			error!("Invalid XCR (index=0x{index:X}, value=0x{value:016X}) is specified in xsetbv instruction!");
			unsafe {
				inject_event(GENERAL_PROTECTION_FAULT, EventType::HardwareException, Some(0), true, 0);
			}
		}
	}

	fn handle_unknown(&mut self, _context: &mut VtStackTop)
	{
		let exit_reason = self.cached_ctxt.exit_reason.into_bits();
		info!("Unknown VM-Exit happened! Reason: {exit_reason}");
		panic!("Unknown VM-Exit is intercepted! Exit-Reason: {} (0x{exit_reason:X})", exit_reason & 0xFFFF);
	}

	fn handle_idt_vectoring(&mut self)
	{
		let idt_vec_info = VmxIdtVectoringInformation::from_bits(vmread32(IDT_VECTORING_INFORMATION).unwrap());
		if idt_vec_info.valid()
		{
			let error_code = vmread32(IDT_VECTORING_ERROR_CODE).unwrap();
			trace!(
				"IDT-vectoring information contains valid event! VM-Exit Reason: {}, Info: 0x{:X}, Error-Code: 0x{error_code:X}",
				self.cached_ctxt.exit_reason.into_bits(),
				idt_vec_info.into_bits()
			);
			vmwrite32(VMENTRY_INTERRUPTION_INFORMATION_FIELD, idt_vec_info.into_bits());
			if idt_vec_info.error_code_valid()
			{
				vmwrite32(VMENTRY_EXCEPTION_ERROR_CODE, error_code);
			}
			// Software interrupts and software exceptions require the instruction length.
			if matches!(
				idt_vec_info.interruption_type(),
				EVENT_TYPE_SOFTWARE_INTERRUPT | EVENT_TYPE_PRIVILEGED_SOFTWARE_EXCEPTION | EVENT_TYPE_SOFTWARE_EXCEPTION
			)
			{
				vmwrite32(VMENTRY_INSTRUCTION_LENGTH, self.cached_ctxt.exit_instruction_length());
			}
		}
	}
}

#[unsafe(no_mangle)]
unsafe extern "win64" fn nvc_vt_exit_handler(context: *mut VtStackTop)
{
	let ctxt = unsafe { &mut *context };
	let vcpu = unsafe { &mut *ctxt.vcpu };
	// Reset the cached context.
	vcpu.cached_ctxt.reset();
	// Save rsp.
	ctxt.gpr_state.rsp = vcpu.cached_ctxt.rsp;
	// Put rsp and rip to the interrupt frame.
	ctxt.guest_frame.return_rsp = vcpu.cached_ctxt.rsp;
	ctxt.guest_frame.return_rip = vcpu.cached_ctxt.rip;
	let handler = dispatch_handler(vcpu.cached_ctxt.exit_reason);
	handler(vcpu, ctxt);
	// Flush cached context into the VMCS.
	vcpu.cached_ctxt.flush();
}

#[unsafe(no_mangle)]
unsafe extern "win64" fn nvc_vt_resume_failure(_context: *mut VtStackTop, _vmx_status: u8)
{
	panic!("VM-Entry failed on resume!");
}

#[unsafe(no_mangle)]
unsafe extern "win64" fn nvc_vt_inject_nmi_to_subverted_host()
{
	panic!("NMI happened in host context!");
}

pub const INTERCEPTED_EXCEPTION_NMI: u32 = 0;
pub const INTERCEPTED_EXTERNAL_INTERRUPT: u32 = 1;
pub const INTERCEPTED_TRIPLE_FAULT: u32 = 2;
pub const INTERCEPTED_INIT_SIGNAL: u32 = 3;
pub const INTERCEPTED_STARTUP_IPI: u32 = 4;
pub const INTERCEPTED_IO_SMI: u32 = 5;
pub const INTERCEPTED_OTHER_SMI: u32 = 6;
pub const INTERCEPTED_INTERRUPT_WINDOW: u32 = 7;
pub const INTERCEPTED_NMI_WINDOW: u32 = 8;
pub const INTERCEPTED_TASK_SWITCH: u32 = 9;
pub const INTERCEPTED_CPUID: u32 = 10;
pub const INTERCEPTED_GETSEC: u32 = 11;
pub const INTERCEPTED_HLT: u32 = 12;
pub const INTERCEPTED_INVD: u32 = 13;
pub const INTERCEPTED_INVLPG: u32 = 14;
pub const INTERCEPTED_RDPMC: u32 = 15;
pub const INTERCEPTED_RDTSC: u32 = 16;
pub const INTERCEPTED_RSM: u32 = 17;
pub const INTERCEPTED_VMCALL: u32 = 18;
pub const INTERCEPTED_VMCLEAR: u32 = 19;
pub const INTERCEPTED_VMLAUNCH: u32 = 20;
pub const INTERCEPTED_VMPTRLD: u32 = 21;
pub const INTERCEPTED_VMPTRST: u32 = 22;
pub const INTERCEPTED_VMREAD: u32 = 23;
pub const INTERCEPTED_VMRESUME: u32 = 24;
pub const INTERCEPTED_VMWRITE: u32 = 25;
pub const INTERCEPTED_VMXOFF: u32 = 26;
pub const INTERCEPTED_VMXON: u32 = 27;
pub const INTERCEPTED_CR_ACCESS: u32 = 28;
pub const INTERCEPTED_DR_ACCESS: u32 = 29;
pub const INTERCEPTED_IO: u32 = 30;
pub const INTERCEPTED_RDMSR: u32 = 31;
pub const INTERCEPTED_WRMSR: u32 = 32;
pub const INVALID_GUEST_STATE: u32 = 33;
pub const MSR_LOADING_FAILURE: u32 = 34;
pub const INTERCEPTED_MWAIT: u32 = 36;
pub const MONITOR_TRAP_FLAG: u32 = 37;
pub const INTERCEPTED_MONITOR: u32 = 39;
pub const INTERCEPTED_PAUSE: u32 = 40;
pub const MACHINE_CHECK_AT_ENTRY: u32 = 41;
pub const TPR_BELOW_THRESHOLD: u32 = 43;
pub const INTERCEPTED_APIC_ACCESS: u32 = 44;
pub const VIRTUALIZED_EOI: u32 = 45;
pub const INTERCEPTED_GDTR_IDTR_ACCESS: u32 = 46;
pub const INTERCEPTED_LDTR_TR_ACCESS: u32 = 47;
pub const EPT_VIOLATION: u32 = 48;
pub const EPT_MISCONFIGURATION: u32 = 49;
pub const INTERCEPTED_INVEPT: u32 = 50;
pub const INTERCEPTED_RDTSCP: u32 = 51;
pub const VMX_PREEMPTION_TIMER_EXPIRED: u32 = 52;
pub const INTERCEPTED_INVVPID: u32 = 53;
pub const INTERCEPTED_WBINVD: u32 = 54;
pub const INTERCEPTED_XSETBV: u32 = 55;
pub const INTERCEPTED_APIC_WRITE: u32 = 56;
pub const INTERCEPTED_RDRAND: u32 = 57;
pub const INTERCEPTED_INVPCID: u32 = 58;
pub const INTERCEPTED_VMFUNC: u32 = 59;
pub const INTERCEPTED_ENCLS: u32 = 60;
pub const INTERCEPTED_RDSEED: u32 = 61;
pub const INTERCEPTED_PML_FULL: u32 = 62;
pub const INTERCEPTED_XSAVES: u32 = 63;
pub const INTERCEPTED_XRSTORS: u32 = 64;
pub const INTERCEPTED_PCONFIG: u32 = 65;
pub const INTERCEPTED_SPP_RELATED_EVENT: u32 = 66;
pub const INTERCEPTED_UMWAIT: u32 = 67;
pub const INTERCEPTED_TPAUSE: u32 = 68;
pub const INTERCEPTED_LOADIWKEY: u32 = 69;
pub const INTERCEPTED_ENCLV: u32 = 70;
pub const ENQCMD_PASID_TRANSLATION_FAILURE: u32 = 72;
pub const ENQCMDS_PASID_TRANSLATION_FAILURE: u32 = 73;
pub const INTERCEPTED_BUS_LOCK: u32 = 74;
pub const INSTRUCTION_TIMEOUT: u32 = 75;
pub const INTERCEPTED_SEAMCALL: u32 = 76;
pub const INTERCEPTED_TDCALL: u32 = 77;
pub const INTERCEPTED_RDMSRLIST: u32 = 78;
pub const INTERCEPTED_WRMSRLIST: u32 = 79;

const VT_MAXIMUM_CODE: usize = 80;

type VtExitHandler = fn(&mut VtVcpu, &mut VtStackTop);

// Defining sparse array is much easier in Rust than in C!
static VT_EXIT_HANDLERS: [VtExitHandler; VT_MAXIMUM_CODE] = {
	let mut array: [VtExitHandler; VT_MAXIMUM_CODE] = [VtVcpu::handle_unknown; VT_MAXIMUM_CODE];
	array[INTERCEPTED_EXTERNAL_INTERRUPT as usize] = VtVcpu::handle_extint;
	array[INTERCEPTED_TRIPLE_FAULT as usize] = VtVcpu::handle_triple_fault;
	array[INTERCEPTED_INIT_SIGNAL as usize] = VtVcpu::handle_init;
	array[INTERCEPTED_STARTUP_IPI as usize] = VtVcpu::handle_sipi;
	array[INTERCEPTED_INTERRUPT_WINDOW as usize] = VtVcpu::handle_interrupt_window;
	array[INTERCEPTED_CPUID as usize] = VtVcpu::handle_cpuid;
	array[INTERCEPTED_GETSEC as usize] = VtVcpu::handle_getsec;
	array[INTERCEPTED_INVD as usize] = VtVcpu::handle_invd;
	array[INTERCEPTED_VMCALL as usize] = VtVcpu::handle_vmcall;
	array[INTERCEPTED_CR_ACCESS as usize] = VtVcpu::handle_cr_access;
	array[INTERCEPTED_RDMSR as usize] = VtVcpu::handle_rdmsr;
	array[INTERCEPTED_WRMSR as usize] = VtVcpu::handle_wrmsr;
	array[INVALID_GUEST_STATE as usize] = VtVcpu::handle_invalid_state;
	array[MSR_LOADING_FAILURE as usize] = VtVcpu::handle_invalid_auto_msr;
	array[EPT_VIOLATION as usize] = VtVcpu::handle_ept_violation;
	array[EPT_MISCONFIGURATION as usize] = VtVcpu::handle_ept_misconfig;
	array[INTERCEPTED_XSETBV as usize] = VtVcpu::handle_xsetbv;
	array
};

#[inline]
fn dispatch_handler(exit_reason: VmxExitReason) -> VtExitHandler
{
	match VT_EXIT_HANDLERS.get(exit_reason.basic_exit_reason() as usize)
	{
		Some(f) => *f,
		None => VtVcpu::handle_unknown,
	}
}
