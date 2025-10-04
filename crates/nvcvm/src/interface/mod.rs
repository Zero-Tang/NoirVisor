// NoirVisor CVM Interface module

use bitfield_struct::bitfield;
use paste::paste;

#[derive(PartialEq, Debug)]
#[repr(C)] pub struct CvmHandle(pub u64);

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

#[derive(Default)]
#[repr(C)] pub struct CvmMapping
{
	/// Guest Physical Address of the base of guest memory range.
	pub base_gpa:u64,
	/// Host Virtual Address of the base of guest memory range.
	pub base_hva:u64,
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
	/// User hypervisor should ignore this VM-Exit and continue.
	pub const SCHEDULER_EXIT:Self=Self(0x80000000);
	/// NoirVisor encountered an internal bug.
	/// User hypervisor should panic upon such exit.
	pub const SCHEDULER_BUG:Self=Self(0x80000001);
}

#[repr(C)] pub struct ExitContext
{
	/// The reason of the VM-Exit.
	pub intercept_code:InterceptCode,
	/// Used for advancing `rip`.
	pub next_rip:u64,
	/// Hint for instruction decoder.
	pub vp_state:CvmX86VcpuStateOnExit,
	/// Per-interception exit context. Use the specific union item based on `intercept_code`.
	pub context:ExitContextUnion
}

#[bitfield(u64)] pub struct CvmX86VcpuStateOnExit
{
	#[bits(2)] pub cpl:u8,
	pub cr0_pe:bool,
	pub cr0_am:bool,
	pub cr0_pg:bool,
	pub cr4_pae:bool,
	pub efer_lma:bool,
	#[bits(4)] pub instruction_length:usize,
	#[bits(4)] pub tpr:u8,
	pub interrupt_pending:bool,
	pub interrupt_shadow:bool,
	#[bits(47)] reserved:u64
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct SegmentRegister
{
	pub selector:u16,
	pub attrib:u16,
	pub limit:u32,
	pub base:u64
}

#[bitfield(u32)] pub struct IoAccessInfo
{
	pub io_type:bool,
	pub string:bool,
	pub repeat:bool,
	#[bits(3)] pub operand_size:usize,
	#[bits(4)] pub address_width:usize,
	#[bits(6)] reserved:u32,
	pub port:u16
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct IoExitStringRegisters
{
	/// Repeat times of the string operation.
	pub rcx:u64,
	/// Can be either `rsi` or `rdi`. Depends on the I/O direction.
	pub linear_address:u64,
	/// Can be either `es` or `ds`. Depends on the I/O direction.
	pub segment:SegmentRegister
}

#[derive(Clone, Copy)]
pub union IoExitRegisterContext
{
	/// Value of the `rax` register. \
	/// This value is undefined if this is string operation.
	pub rax:u64,
	/// Registers for String I/O operation. \
	/// This value is undefined is this is not string operation.
	pub string_regs:IoExitStringRegisters
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct IoExitContext
{
	/// Access Information.
	pub access_info:IoAccessInfo,
	/// Registers provided by NoirVisor.
	pub reg_ctxt:IoExitRegisterContext
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
	#[bits(19)] reserved:u64,
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
	pub mem:MemoryAccessContext,
	empty:()
}

impl Default for ExitContextUnion
{
	fn default() -> Self
	{
		Self{empty:()}
	}
}

#[derive(Default, Clone, Copy)]
#[repr(C)] pub struct CvmX64RegisterCode(pub u32);

macro_rules! derive_reg_const
{
	($n:tt)=>
	{
		paste!
		{
			pub const [<R $n>]:Self=Self($n);
		}
	};
	($name:tt,$num:tt,$base:tt)=>
	{
		paste!
		{
			pub const [<$name $num>]:Self=Self($base+$num);
		}
	}
}

impl CvmX64RegisterCode
{
	// General-Purpose Registers
	pub const RAX:Self=Self(0);
	pub const RCX:Self=Self(1);
	pub const RDX:Self=Self(2);
	pub const RBX:Self=Self(3);
	pub const RSP:Self=Self(4);
	pub const RBP:Self=Self(5);
	pub const RSI:Self=Self(6);
	pub const RDI:Self=Self(7);
	derive_reg_const!(8);
	derive_reg_const!(9);
	derive_reg_const!(10);
	derive_reg_const!(11);
	derive_reg_const!(12);
	derive_reg_const!(13);
	derive_reg_const!(14);
	derive_reg_const!(15);
	// Reserve 0x10~0x1F for APX (Advanced Performance Extension)
	derive_reg_const!(16);
	derive_reg_const!(17);
	derive_reg_const!(18);
	derive_reg_const!(19);
	derive_reg_const!(20);
	derive_reg_const!(21);
	derive_reg_const!(22);
	derive_reg_const!(23);
	derive_reg_const!(24);
	derive_reg_const!(25);
	derive_reg_const!(26);
	derive_reg_const!(27);
	derive_reg_const!(28);
	derive_reg_const!(29);
	derive_reg_const!(30);
	derive_reg_const!(31);
	pub const RFLAGS:Self=Self(0x20);
	pub const RIP:Self=Self(0x21);
	pub const SSP:Self=Self(0x22);
	// Segment Registers
	pub const GDTR:Self=Self(0x26);
	pub const IDTR:Self=Self(0x27);
	pub const ES:Self=Self(0x28);
	pub const CS:Self=Self(0x29);
	pub const SS:Self=Self(0x2A);
	pub const DS:Self=Self(0x2B);
	pub const FS:Self=Self(0x2C);
	pub const GS:Self=Self(0x2D);
	pub const LDTR:Self=Self(0x2E);
	pub const TR:Self=Self(0x2F);
	// Control Registers
	derive_reg_const!(CR,0,0x30);
	derive_reg_const!(CR,2,0x30);
	derive_reg_const!(CR,3,0x30);
	derive_reg_const!(CR,4,0x30);
	derive_reg_const!(CR,8,0x30);
	// Debug Registers
	derive_reg_const!(DR,0,0x40);
	derive_reg_const!(DR,1,0x40);
	derive_reg_const!(DR,2,0x40);
	derive_reg_const!(DR,3,0x40);
	derive_reg_const!(DR,6,0x40);
	derive_reg_const!(DR,7,0x40);
	// Extended Control Registers
	derive_reg_const!(XCR,0,0x50);
	// XMM/YMM/ZMM Registers
	// Note XMM is just lower 128 bits of YMM and ZMM.
	// Likewise, YMM is lower 256 bits of ZMM.
	derive_reg_const!(ZMM,0,0x60);
	derive_reg_const!(ZMM,1,0x60);
	derive_reg_const!(ZMM,2,0x60);
	derive_reg_const!(ZMM,3,0x60);
	derive_reg_const!(ZMM,4,0x60);
	derive_reg_const!(ZMM,5,0x60);
	derive_reg_const!(ZMM,6,0x60);
	derive_reg_const!(ZMM,7,0x60);
	derive_reg_const!(ZMM,8,0x60);
	derive_reg_const!(ZMM,9,0x60);
	derive_reg_const!(ZMM,10,0x60);
	derive_reg_const!(ZMM,11,0x60);
	derive_reg_const!(ZMM,12,0x60);
	derive_reg_const!(ZMM,13,0x60);
	derive_reg_const!(ZMM,14,0x60);
	derive_reg_const!(ZMM,15,0x60);
	derive_reg_const!(ZMM,16,0x60);
	derive_reg_const!(ZMM,17,0x60);
	derive_reg_const!(ZMM,18,0x60);
	derive_reg_const!(ZMM,19,0x60);
	derive_reg_const!(ZMM,20,0x60);
	derive_reg_const!(ZMM,21,0x60);
	derive_reg_const!(ZMM,22,0x60);
	derive_reg_const!(ZMM,23,0x60);
	derive_reg_const!(ZMM,24,0x60);
	derive_reg_const!(ZMM,25,0x60);
	derive_reg_const!(ZMM,26,0x60);
	derive_reg_const!(ZMM,27,0x60);
	derive_reg_const!(ZMM,28,0x60);
	derive_reg_const!(ZMM,29,0x60);
	derive_reg_const!(ZMM,30,0x60);
	derive_reg_const!(ZMM,31,0x60);
	// ST Registers
	derive_reg_const!(ST,0,0x80);
	derive_reg_const!(ST,1,0x80);
	derive_reg_const!(ST,2,0x80);
	derive_reg_const!(ST,3,0x80);
	derive_reg_const!(ST,4,0x80);
	derive_reg_const!(ST,5,0x80);
	derive_reg_const!(ST,6,0x80);
	derive_reg_const!(ST,7,0x80);
	pub const FP_CONTROL_STATUS:Self=Self(0x88);
	pub const XMM_CONTROL_STATUS:Self=Self(0x89);
	pub const FP_RDP:Self=Self(0x8A);
	pub const FP_RIP:Self=Self(0x8B);
	// TMM Registers
	derive_reg_const!(TMM,0,0x90);
	derive_reg_const!(TMM,1,0x90);
	derive_reg_const!(TMM,2,0x90);
	derive_reg_const!(TMM,3,0x90);
	derive_reg_const!(TMM,4,0x90);
	derive_reg_const!(TMM,5,0x90);
	derive_reg_const!(TMM,6,0x90);
	derive_reg_const!(TMM,7,0x90);
	// Model-Specific Registers
	pub const TSC:Self=Self(0x1000);
	pub const EFER:Self=Self(0x1001);
	pub const STAR:Self=Self(0x1002);
	pub const LSTAR:Self=Self(0x1003);
	pub const CSTAR:Self=Self(0x1004);
	pub const SFMASK:Self=Self(0x1005);
	pub const KGS_BASE:Self=Self(0x1006);
	pub const STSTAR:Self=Self(0x1007);	// Reserved for AMD's whitepaper.
	pub const SYSENTER_CS:Self=Self(0x1008);
	pub const SYSENTER_ESP:Self=Self(0x1009);
	pub const SYSENTER_EIP:Self=Self(0x100A);
	pub const PAT:Self=Self(0x100B);
	pub const DEBUG_CTRL:Self=Self(0x100C);
	pub const XSS:Self=Self(0x100D);
	pub const S_CET:Self=Self(0x100E);
	pub const U_CET:Self=Self(0x100F);
	pub const PL0_SSP:Self=Self(0x1010);
	pub const PL1_SSP:Self=Self(0x1011);
	pub const PL2_SSP:Self=Self(0x1012);
	pub const PL3_SSP:Self=Self(0x1013);
	pub const ISST_ADDR:Self=Self(0x1014);
	pub const TSC_AUX:Self=Self(0x1015);
	pub const SMBASE:Self=Self(0x1016);	
}

#[repr(C,align(1024))] pub struct X64SyncReg
{
	rax:u64,
	rcx:u64,
	rdx:u64,
	rbx:u64,
	rsp:u64,
	rbp:u64,
	rsi:u64,
	rdi:u64,
	r8:u64,
	r9:u64,
	r10:u64,
	r11:u64,
	r12:u64,
	r13:u64,
	r14:u64,
	r15:u64,
	rflags:u64,
	rip:u64,
	es:SegmentRegister,
	cs:SegmentRegister,
	ss:SegmentRegister,
	ds:SegmentRegister,
	fs:SegmentRegister,
	gs:SegmentRegister,
	gdtr:SegmentRegister,
	ldtr:SegmentRegister,
	idtr:SegmentRegister,
	tr:SegmentRegister,
	cr0:u64,
	cr2:u64,
	cr3:u64,
	cr4:u64,
	dr0:u64,
	dr1:u64,
	dr2:u64,
	dr3:u64,
	dr6:u64,
	dr7:u64,
	ssp:u64,
	efer:u64,
	star:u64,
	lstar:u64,
	cstar:u64,
	ststar:u64,
	sfmask:u64,
	kgsbase:u64,
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
	/// Is the vCPU interruptible at this moment?
	pub vcpu_state:CvmX86VcpuStateOnExit,
	/// If true, intercept the vCPU execution once the
	/// vCPU stops masking interrupts.
	pub request_interrupt_window:bool,
	/// If true, rescinds (kicks) the vCPU.
	pub request_rescission:bool,
	/// Bitmap for what registers should be synchronized.
	pub sync_flags:u64,
	/// Exit Context.
	pub exit_context:ExitContext,
	/// Raw data of registers to be synchronized.
	pub sync_regs:X64SyncReg,
}