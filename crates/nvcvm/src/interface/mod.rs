// NoirVisor CVM Interface module

#[repr(C)] pub struct CvmHandle(pub u64);

#[repr(C)] pub struct InterceptCode(pub u32);

impl InterceptCode
{
	pub const INVALID_STATE:u32=0;
	pub const SHUTDOWN_CONDITION:u32=1;
	pub const MEMORY_ACCESS:u32=2;
	pub const RESCISSION:u32=3;
	pub const HLT_INSTRUCTION:u32=4;
	pub const IO_INSTRUCTION:u32=5;
	pub const CPUID_INSTRUCTION:u32=6;
	pub const MSR_INSTRUCTION:u32=7;
	pub const EXCEPTION:u32=8;
	pub const CR_ACCESS:u32=9;
	pub const DR_ACCESS:u32=10;
	pub const HYPERCALL:u32=11;
	pub const INTERRUPT_WINDOW:u32=12;
	
	pub const SCHEDULER_EXIT:u32=0x80000000;
	pub const SCHEDULER_PAUSE:u32=0x80000001;
	pub const SCHEDULER_BUG:u32=0x80000002;
}

#[repr(C)] pub struct ExitContext
{
	pub intercept_code:InterceptCode,
	pub rip:u64,
	pub next_rip:u64,
	pub rflags:u64,
	// Hint for instruction decoder.
	pub vp_state:u64,
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct SegmentRegister
{
	pub selector:u16,
	pub attrib:u16,
	pub limit:u32,
	pub base:u64
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct IoExitContext
{
	pub port:u16,
	pub direction:bool,
	pub string:bool,
	pub repeat:bool,
	pub operand_size:u8,
	pub address_width:u8,
	pub padding:u8,
}

#[derive(Clone, Copy)]
#[repr(C)] pub struct MemoryAccessContext
{
	pub access:u8,
	pub instruction_bytes:[u8;15],
	pub gpa:u64,
	pub gva:u64,
	pub flags:u64,
}

pub union ExitContextUnion
{
	pub io:IoExitContext,
	pub mem:MemoryAccessContext
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
	efer:u64
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
	pub if_flag:bool,
	/// If true, intercept the vCPU execution once the
	/// vCPU stops masking interrupts.
	pub request_interrupt_window:bool,
	/// If true, rescinds (kicks) the vCPU.
	pub request_rescission:bool,
	/// Bitmap for what registers should be synchronized.
	pub sync_flags:u64,
	/// Raw data of registers to be synchronized.
	pub sync_regs:X64SyncReg
}