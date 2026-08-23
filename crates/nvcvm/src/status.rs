// NoirVisor CVM Status definition module
use core::{alloc::AllocError, error::Error, fmt};

#[derive(Debug,PartialEq,Eq,PartialOrd,Ord)]
#[repr(u8)] pub enum Severity
{
	Success=0,
	Info=1,
	Warning=2,
	Error=3,
	Unknown
}

#[derive(Debug,PartialEq,Eq,PartialOrd,Ord)]
#[repr(u8)] pub enum Facility
{
	Xpf=0,
	Intel=1,
	AMD=2,
	Emulator=3,
	Unknown=(1<<6)-1
}

use Severity::*;
use Facility::*;

#[derive(Debug,PartialEq,Eq,PartialOrd,Ord,Clone,Copy)]
#[repr(C)] pub struct Status(pub u32);

impl Status
{
	pub const fn construct(severity:Severity,facility:Facility,code:u32)->Self
	{
		let s=severity as u32;
		let f=facility as u32;
		Status((s<<30)|(f<<24)|code)
	}

	// Success-level
	pub const SUCCESS:Status=Status::construct(Success,Xpf,0);

	// Info-level
	pub const ALREADY_RESCINDED:Status=Status::construct(Info,Xpf,1);

	// Error-level: XPF-Core
	pub const UNSUCCESSFUL:Status=Status::construct(Error,Xpf,0);
	pub const INSUFFICIENT_RESOURCES:Status=Status::construct(Error,Xpf,1);
	pub const NOT_IMPLEMENTED:Status=Status::construct(Error,Xpf,2);
	pub const UNKNOWN_PROCESSOR:Status=Status::construct(Error,Xpf,3);
	pub const INVALID_PARAMETER:Status=Status::construct(Error,Xpf,4);
	pub const HYPERVISION_ABSENT:Status=Status::construct(Error,Xpf,5);
	pub const VCPU_ALREADY_CREATED:Status=Status::construct(Error,Xpf,6);
	pub const BUFFER_TOO_SMALL:Status=Status::construct(Error,Xpf,7);
	pub const VCPU_NOT_EXIST:Status=Status::construct(Error,Xpf,8);
	pub const USER_PAGE_VIOLATION:Status=Status::construct(Error,Xpf,9);
	pub const GUEST_PAGE_ABSENT:Status=Status::construct(Error,Xpf,10);
	pub const ACCESS_DENIED:Status=Status::construct(Error,Xpf,11);
	pub const HARDWARE_ERROR:Status=Status::construct(Error,Xpf,12);
	pub const UNINITIALIZED:Status=Status::construct(Error,Xpf,13);
	pub const NSV_VIOLATION:Status=Status::construct(Error,Xpf,14);
	pub const ACPI_NO_SUCH_TABLE:Status=Status::construct(Error,Xpf,15);
	pub const DISPATCH_FAILURE:Status=Status::construct(Error,Xpf,16);
	pub const SYNCHRONIZATION_VIOLATION:Status=Status::construct(Error,Xpf,17);

	// Error-level: VT-Core
	pub const NOT_INTEL:Status=Status::construct(Error,Intel,0);
	pub const VMX_NOT_SUPPORTED:Status=Status::construct(Error,Intel,1);
	pub const EPT_NOT_SUPPORTED:Status=Status::construct(Error,Intel,2);
	pub const DMAR_NOT_SUPPORTED:Status=Status::construct(Error,Intel,3);

	// Error-level: SVM-Core
	pub const NOT_AMD:Status=Status::construct(Error,AMD,0);
	pub const SVM_NOT_SUPPORTED:Status=Status::construct(Error,AMD,1);
	pub const NPT_NOT_SUPPORTED:Status=Status::construct(Error,AMD,2);
	pub const IVRS_NOT_SUPPROTED:Status=Status::construct(Error,AMD,3);

	// Error-level: Emulator
	pub const NOT_EMULATABLE:Status=Status::construct(Error,Emulator,0);
	pub const UNKNOWN_INSTRUCTION:Status=Status::construct(Error,Emulator,1);
}

impl Error for Status {}

impl From<AllocError> for Status
{
	fn from(_value: AllocError) -> Self
	{
		Self::INSUFFICIENT_RESOURCES
	}
}

impl fmt::Display for Status
{
	fn fmt(&self,f:&mut fmt::Formatter<'_>)->fmt::Result
	{
		let s=match *self
		{
			Status::SUCCESS=>Some("Operation is successfully completed"),
			Status::ALREADY_RESCINDED=>Some("vCPU is already rescinded"),
			Status::UNSUCCESSFUL=>Some("Operation is unsuccessful with unspecified error"),
			Status::INSUFFICIENT_RESOURCES=>Some("Resource is insufficient"),
			Status::NOT_IMPLEMENTED=>Some("This operation is not implemented"),
			Status::UNKNOWN_PROCESSOR=>Some("This processor is unknown"),
			Status::INVALID_PARAMETER=>Some("One or more parameters are invalid"),
			Status::HYPERVISION_ABSENT=>Some("NoirVisor has not subverted the system yet"),
			Status::VCPU_ALREADY_CREATED=>Some("The vCPU is already created in the VM"),
			Status::BUFFER_TOO_SMALL=>Some("The buffer is too small"),
			Status::VCPU_NOT_EXIST=>Some("The vCPU does not exist in this VM"),
			Status::GUEST_PAGE_ABSENT=>Some("The guest page is absent"),
			Status::ACCESS_DENIED=>Some("Access is denied"),
			Status::HARDWARE_ERROR=>Some("Hardware error"),
			Status::UNINITIALIZED=>Some("Uninitialized"),
			Status::NSV_VIOLATION=>Some("NoirVisor Secure Virtualization policy violated"),
			Status::ACPI_NO_SUCH_TABLE=>Some("The specified ACPI table can't be found"),
			Status::DISPATCH_FAILURE=>Some("This I/O request is not properly dispatched"),
			Status::SYNCHRONIZATION_VIOLATION=>Some("Synchonization rule is violated"),
			Status::NOT_INTEL=>Some("This CPU is not manufactured by Intel"),
			Status::VMX_NOT_SUPPORTED=>Some("This CPU does not support Intel VT-x"),
			Status::EPT_NOT_SUPPORTED=>Some("This CPU does not support Extended Page Tables"),
			Status::DMAR_NOT_SUPPORTED=>Some("This system does not support Intel VT-d"),
			Status::NOT_AMD=>Some("This CPU is not manufactured by AMD"),
			Status::SVM_NOT_SUPPORTED=>Some("This CPU does not support AMD-V"),
			Status::NPT_NOT_SUPPORTED=>Some("This CPU does not support Nested Paging"),
			Status::IVRS_NOT_SUPPROTED=>Some("This system does not support AMD-Vi"),
			Status::NOT_EMULATABLE=>Some("This instruction cannot be emulated"),
			Status::UNKNOWN_INSTRUCTION=>Some("This instruction is not recognized by the emulator"),
			_=>None
		};
		match s
		{
			Some(reason_string)=>write!(f,"{reason_string}"),
			None=>write!(f,"Unknown Status 0x{:08X}",self.0)
		}
	}
}