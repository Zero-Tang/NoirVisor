// NoirVisor CVM Interface module

use bitfield_struct::bitfield;

#[cfg(feature="user")]
pub mod c_api;

#[derive(Clone, Copy, PartialEq, Debug)]
#[repr(C)] pub struct CvmHandle(pub u32);

#[bitfield(u32)] pub struct CvmMappingFlags
{
	pub read:bool,
	pub write:bool,
	pub execute:bool,
	#[bits(3)] pub cache_type:u8,
	#[bits(26)] rsvd:u32
}

/// Default System Memory Space
pub const CVM_MAPPING_ASID_DEFAULT:u32=0;
/// System-Management RAM (SMRAM) Space
pub const CVM_MAPPING_ASID_SMRAM:u32=1;
/// Start of reserved memory space
pub const CVM_MAPPING_ASID_RESERVED_START:u32=2;
/// Start of free memory space. Must be created in order to use it.
pub const CVM_MAPPING_ASID_FREE_START:u32=0x80000000;

#[derive(Default, Clone, Copy)]
#[repr(C)] pub struct CvmMapping
{
	/// Guest Physical Address of the base of guest memory range.
	pub base_gpa:u64,
	/// Size of bytes to map. It must be aligned on page boundary.
	pub size:u64,
	/// Address-space identifier.
	/// - `0` for default memory space.
	/// - `1` for System-Management RAM (SMRAM) on x86.
	/// - `2..0x80000000` are reserved by NoirVisor.
	/// - `0x80000000..=0xFFFFFFFF` are free, but must after being created, to use.
	pub as_id:u32,
	/// To map, one of R/W/X bit must be set.\
	/// Valid Mapping Permission Combinations are: \
	/// R (Read Only), R/W (No Execute), R/X (No Write), R/W/X (All Accesses) \
	/// For Intel VT-x, X (Execute Only) permission is also valid.
	/// 
	/// To unmap, clear all of R/W/X bits.
	pub flags:CvmMappingFlags
}

#[bitfield(u32)] pub struct CvmRequestedEventInformation
{
	/// Interrupt Vector of the Event.
	pub vector:u8,
	/// Type of Event. Can be one of:
	/// - External Interrupt.
	/// - Non-Maskable Interrupt.
	/// - Hardware Exception.
	/// - Software Interrupt.
	#[bits(3)] pub r#type:u8,
	/// Specify whether this event has error code. \
	/// The `type` must be Hardware Exception if the event has an error code.
	pub has_error_code:bool,
	/// Specify the priority of the event. \
	/// The `type` must be External Interrupt if the event has a priority.
	#[bits(4)] pub priority:u8,
	/// Specify the instruction length of the event. \
	/// The `type` must be Software Interrupt if the event has an instruction length.
	#[bits(4)] pub insn_len:u64,
	/// Ignores the TPR when delivering the event. \
	/// The `type` must be External Interrupt in order to ignore the TPR.
	pub ignore_tpr:bool,
	/// Infers the priority of the event to be delivered. \
	/// The `type` must be External Interrupt in order to infer the priority.
	pub infer_priority:bool,
	#[bits(10)] rsvd:u32
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct CvmRequestedEvent
{
	/// The basic information of the event.
	pub info:CvmRequestedEventInformation,
	/// The error code of the exception.
	pub error_code:u32,
	/// The payload of the exception. (e.g.: new CR2 value of #PF exception)
	pub payload:u64
}

#[derive(Clone, Copy, Default, Debug, PartialEq)]
#[repr(C)] pub struct InterceptCode(pub u32);

impl InterceptCode
{
	/// The vCPU is loaded with invalid state.
	pub const INVALID_STATE:Self=Self(0);
	/// The vCPU triggered a triple-fault.
	pub const SHUTDOWN_CONDITION:Self=Self(1);
	/// The vCPU accessed a memory address with insufficient
	/// privilege specified by the user hypervisor.
	pub const MEMORY_ACCESS:Self=Self(2);
	/// The hypervisor rescinded this vCPU's execution.
	pub const RESCISSION:Self=Self(3);
	/// The vCPU executed the `hlt` instruction.
	pub const HLT_INSTRUCTION:Self=Self(4);
	/// The vCPU executed port I/O instruction.
	pub const IO_INSTRUCTION:Self=Self(5);
	/// The vCPU executed `cpuid` instruction while the
	/// user hypervisor specified interception for it.
	pub const CPUID_INSTRUCTION:Self=Self(6);
	/// The vCPU accessed MSR while the
	/// user hypervisor specified interception for it.
	pub const MSR_INSTRUCTION:Self=Self(7);
	/// The vCPU triggered an exception while the
	/// user hypervisor specified interception for it.
	pub const EXCEPTION:Self=Self(8);
	/// The vCPU accessed the control register while the
	/// user hypervisor specified interception for it.
	pub const CR_ACCESS:Self=Self(9);
	/// The vCPU accessed the debug register while the
	/// user hypervisor specified interception for it.
	pub const DR_ACCESS:Self=Self(10);
	/// The vCPU executed a hypercall instruction.
	pub const HYPERCALL:Self=Self(11);
	/// The vCPU entered a spin-loop with a hint while the
	/// user hypervisor specified interception for it. \
	/// User hypervisor may yield the thread to give host a better performance.
	pub const PAUSE:Self=Self(12);
	
	/// A physical interrupt arrived during vCPU's execution. \
	/// User hypervisor should ignore this VM-Exit and continue its execution.
	pub const SCHEDULER_EXIT:Self=Self(0x80000000);
	/// NoirVisor encountered an internal bug.
	/// User hypervisor should panic upon such exit.
	pub const SCHEDULER_BUG:Self=Self(0x80000001);
}

#[repr(C)] pub struct ExitContext
{
	/// Used for advancing `rip`.
	pub next_rip:u64,
	/// Per-interception exit context. Use the specific union item based on `intercept_code`.
	pub context:ExitContextUnion
}

#[derive(Clone, Copy, Debug, Default)]
#[repr(C)] pub struct SegmentRegister
{
	pub selector:u16,
	pub attrib:u16,
	pub limit:u32,
	pub base:u64
}

impl SegmentRegister
{
	pub fn from_dt(limit:u16,base:u64)->Self
	{
		Self
		{
			selector:0,
			attrib:0,
			limit:limit as u32,
			base
		}
	}

	pub const fn reset_code()->Self
	{
		Self
		{
			selector:0xF000,
			attrib:0x9B,
			limit:0xFFFF,
			base:0xFFFF0000
		}
	}

	pub const fn reset_data()->Self
	{
		Self
		{
			selector:0,
			attrib:0x92,
			limit:0xFFFF,
			base:0
		}
	}

	pub const fn reset_tss()->Self
	{
		Self
		{
			selector:0,
			attrib:0x83,
			limit:0xFFFF,
			base:0
		}
	}

	pub const fn reset_ldt()->Self
	{
		Self
		{
			selector:0,
			attrib:0x82,
			limit:0xFFFF,
			base:0
		}
	}

	pub const fn reset_dt()->Self
	{
		Self
		{
			selector:0,
			attrib:0,
			limit:0xFFFF,
			base:0
		}
	}
}

#[bitfield(u32)] pub struct IoExitContext
{
	pub io_type:bool,
	pub string:bool,
	pub repeat:bool,
	#[bits(3)] pub operand_size:usize,
	#[bits(4)] pub address_width:usize,
	#[bits(6)] reserved:u32,
	pub port:u16
}

#[bitfield(u8)] pub struct CvmMemoryAccessInfo
{
	/// If this field is `false`, the page is absent. \
	/// If this field is `true`, the page is present.
	pub present:bool,
	/// If this field is `false`, the access is a `read`. \
	/// If this field is `true`, the access is a `write`.
	pub write:bool,
	/// If this field is `false`, the access is a `read` or `write`. \
	/// If this field is `true`, the access is an instruction fetch.
	pub execute:bool,
	/// If this field is `true`, the access is from guest page-table walking. \
	/// Otherwise, it is not.
	pub guest_pt:bool,
	#[bits(4)] pub fetched_bytes:usize
}

#[bitfield(u64)] pub struct CvmMemoryAccessFlags
{
	pub instruction_code:u16,
	/// Size of the operand in bytes.
	pub operand_size:u16,
	#[bits(5)] pub operand_class:usize,
	#[bits(7)] pub operand_code:usize,
	#[bits(18)] reserved:u64,
	/// If this bit is set, then `gva` is valid.
	pub gva_valid:bool,
	/// If this bit is set, then this `CvmMemoryAccessFlags` field is valid.
	pub decoded:bool
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct MemoryAccessContext
{
	pub access:CvmMemoryAccessInfo,
	/// In case NoirVisor couldn't decode the instruction,
	/// you can decode the instruction on your own.
	pub instruction_bytes:[u8;15],
	pub gpa:u64,
	pub gva:u64,
	pub flags:CvmMemoryAccessFlags,
}

pub union ExitContextUnion
{
	pub io:IoExitContext,
	pub mem:MemoryAccessContext
}

impl Default for ExitContextUnion
{
	fn default() -> Self
	{
		unsafe
		{
			core::mem::zeroed()
		}
	}
}

#[repr(C,align(1024))] pub struct X64SyncReg
{
	pub rax:u64,
	pub rcx:u64,
	pub rdx:u64,
	pub rbx:u64,
	pub rsp:u64,
	pub rbp:u64,
	pub rsi:u64,
	pub rdi:u64,
	pub r8:u64,
	pub r9:u64,
	pub r10:u64,
	pub r11:u64,
	pub r12:u64,
	pub r13:u64,
	pub r14:u64,
	pub r15:u64,
	pub rflags:u64,
	pub rip:u64,
	pub es:SegmentRegister,
	pub cs:SegmentRegister,
	pub ss:SegmentRegister,
	pub ds:SegmentRegister,
	pub fs:SegmentRegister,
	pub gs:SegmentRegister,
	pub gdtr:SegmentRegister,
	pub ldtr:SegmentRegister,
	pub idtr:SegmentRegister,
	pub tr:SegmentRegister,
	pub cr0:u64,
	pub cr2:u64,
	pub cr3:u64,
	pub cr4:u64,
	pub cr8:u64,
	pub dr0:u64,
	pub dr1:u64,
	pub dr2:u64,
	pub dr3:u64,
	pub dr6:u64,
	pub dr7:u64,
	pub ssp:u64,
	pub efer:u64,
	pub star:u64,
	pub lstar:u64,
	pub cstar:u64,
	pub ststar:u64,
	pub sfmask:u64,
	pub kgsbase:u64,
	pub sysenter_cs:u64,
	pub sysenter_esp:u64,
	pub sysenter_eip:u64,
}

#[bitfield(u64)] pub struct X64SyncFlags
{
	/// GPRs (rax~r15) /w rflags & rip.
	pub gpr:bool,
	/// cs, ds, es, ss
	pub seg:bool,
	/// fs, gs and kernel_gs
	pub fg:bool,
	/// gdtr & ldtr
	pub dt:bool,
	/// ldtr & tr
	pub lt:bool,
	/// cr0, cr2, cr3, cr4, cr8, efer
	pub cr:bool,
	/// dr0, dr1, dr2, dr3, dr6, dr7
	pub dr:bool,
	/// star, lstar, cstar and sfmask
	pub sc:bool,
	/// sysenter_cs, sysenter_esp and sysenter_eip
	pub se:bool,
	#[bits(55)] rsvd:u64
}

/// # VPCB Structure
/// The VPCB (Virtual Processor Control Block) structure is intended
/// for user hypervisor to control execution by changing fields in
/// the VPCB structure prior to running the vCPU, and also obtain
/// relevant information about what kind of interception the vCPU
/// encountered by looking into the VPCB structure.
#[repr(C,align(4096))] pub struct Vpcb
{
	/// The intercept code of VM-Exit defined by NoirVisor.
	pub intercept_code:InterceptCode,
	/// If true, intercept the vCPU execution once the
	/// vCPU stops masking interrupts.
	pub request_interrupt_window:bool,
	/// If true, rescinds (kicks) the vCPU.
	pub request_rescission:bool,
	/// Bitmap for what registers should be synchronized.
	pub sync_flags:X64SyncFlags,
	/// Exit Context.
	pub exit_context:ExitContext,
	/// Raw data of registers to be synchronized.
	pub sync_regs:X64SyncReg,
}