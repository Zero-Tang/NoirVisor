/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2024. All rights reserved.
 * 
 * This file defines status utilities of NoirVisor Core in Rust.
 * This will prove useful when we use the Result enum.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

#![allow(dead_code)]

#[repr(C)] pub struct Status(pub u32);

pub enum Severity
{
	Success,
	Info,
	Warning,
	Error,
	UnknownSeverity
}

pub enum Facility
{
	Xpf,
	Intel,
	AMD,
	Emulator,
	UnknownFacility
}

use Severity::*;
use Facility::*;

impl Status
{
	pub const fn new(severity:Severity,facility:Facility,code:u32)->Self
	{
		let s=severity as u32;
		let f=facility as u32;
		Status((s<<30)|(f<<24)|code)
	}

	pub fn get_severity(st:Self)->Severity
	{
		let s=st.0>>30;
		match s
		{
			0=>Success,
			1=>Info,
			2=>Warning,
			3=>Error,
			_=>UnknownSeverity
		}
	}

	pub fn get_facility(st:Self)->Facility
	{
		let f=(st.0>>24)&0x3F;
		match f
		{
			0=>Xpf,
			1=>Intel,
			2=>AMD,
			3=>Emulator,
			_=>UnknownFacility
		}
	}
}

// Success-level
pub const NOIR_SUCCESS:Status=Status::new(Success,Xpf,0);

// Info-level
pub const NOIR_ALREADY_RESCINDED:Status=Status::new(Info,Xpf,1);

// Error-level: XPF-Core
pub const NOIR_UNSUCCESSFUL:Status=Status::new(Error,Xpf,0);
pub const NOIR_INSUFFICIENT_RESOURCES:Status=Status::new(Error,Xpf,1);
pub const NOIR_NOT_IMPLEMENTED:Status=Status::new(Error,Xpf,2);
pub const NOIR_UNKNOWN_PROCESSOR:Status=Status::new(Error,Xpf,3);
pub const NOIR_INVALID_PARAMETER:Status=Status::new(Error,Xpf,4);
pub const NOIR_HYPERVISION_ABSENT:Status=Status::new(Error,Xpf,5);
pub const NOIR_VCPU_ALREADY_CREATED:Status=Status::new(Error,Xpf,6);
pub const NOIR_BUFFER_TOO_SMALL:Status=Status::new(Error,Xpf,7);
pub const NOIR_VCPU_NOT_EXIST:Status=Status::new(Error,Xpf,8);
pub const NOIR_USER_PAGE_VIOLATION:Status=Status::new(Error,Xpf,9);
pub const NOIR_GUEST_PAGE_ABSENT:Status=Status::new(Error,Xpf,10);
pub const NOIR_ACCESS_DENIED:Status=Status::new(Error,Xpf,11);
pub const NOIR_HARDWARE_ERROR:Status=Status::new(Error,Xpf,12);
pub const NOIR_UNINITIALIZED:Status=Status::new(Error,Xpf,13);
pub const NOIR_NSV_VIOLATION:Status=Status::new(Error,Xpf,14);
pub const NOIR_ACPI_NO_SUCH_TABLE:Status=Status::new(Error,Xpf,15);

// Error-level: VT-Core
pub const NOIR_NOT_INTEL:Status=Status::new(Error,Intel,0);
pub const NOIR_VMX_NOT_SUPPORTED:Status=Status::new(Error,Intel,1);
pub const NOIR_EPT_NOT_SUPPORTED:Status=Status::new(Error,Intel,2);
pub const NOIR_DMAR_NOT_SUPPORTED:Status=Status::new(Error,Intel,3);

// Error-level: SVM-Core
pub const NOIR_NOT_AMD:Status=Status::new(Error,AMD,0);
pub const NOIR_SVM_NOT_SUPPORTED:Status=Status::new(Error,AMD,1);
pub const NOIR_NPT_NOT_SUPPORTED:Status=Status::new(Error,AMD,2);

// Error-level: Emulator
pub const NOIR_EMU_NOT_EMULATABLE:Status=Status::new(Error,Emulator,0);
pub const NOIR_EMU_UNKNOWN_INSTRUCTION:Status=Status::new(Error,Emulator,1);