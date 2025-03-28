// NoirVisor CVM Status definition module

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

#[repr(C)] pub struct Status(pub u32);

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
		let f: u32=(st.0>>24)&0x3F;
		match f
		{
			0=>Xpf,
			1=>Intel,
			2=>AMD,
			3=>Emulator,
			_=>UnknownFacility
		}
	}

	pub const SUCCESS:Self=Self::new(Success,Xpf,0);
	pub const ALREADY_RESCINDED:Self=Self::new(Info,Xpf,1);
	pub const UNSUCCESSFUL:Self=Self::new(Error,Xpf,0);
	pub const INSUFFICIENT_RESOURCES:Self=Self::new(Error,Xpf,1);
	pub const NOT_IMPLEMENTED:Self=Self::new(Error,Xpf,2);
	pub const UNKNOWN_PROCESSOR:Self=Self::new(Error,Xpf,3);
	pub const INVALID_PARAMETER:Self=Self::new(Error,Xpf,4);
	pub const HYPERVISION_ABSENT:Self=Self::new(Error,Xpf,5);
	pub const VCPU_ALREADY_CREATED:Self=Self::new(Error,Xpf,6);
	pub const BUFFER_TOO_SMALL:Self=Self::new(Error,Xpf,7);
	pub const VCPU_NOT_EXIST:Self=Self::new(Error,Xpf,8);
}