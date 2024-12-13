// This file defines registers in x64 targets.
use super::GdbRegisterTrait;

// This struct definition is generated from XML in QEMU.
#[derive(Debug,Default)]
#[repr(C,packed)] pub struct QemuGdbX64Registers
{
	pub rax:u64,
	pub rbx:u64,
	pub rcx:u64,
	pub rdx:u64,
	pub rsi:u64,
	pub rdi:u64,
	pub rbp:u64,
	pub rsp:u64,
	pub r8:u64,
	pub r9:u64,
	pub r10:u64,
	pub r11:u64,
	pub r12:u64,
	pub r13:u64,
	pub r14:u64,
	pub r15:u64,
	pub rip:u64,
	pub eflags:u32,
	pub cs:u32,
	pub ss:u32,
	pub ds:u32,
	pub es:u32,
	pub fs:u32,
	pub gs:u32,
	pub fs_base:u64,
	pub gs_base:u64,
	pub k_gs_base:u64,
	pub cr0:u64,
	pub cr2:u64,
	pub cr3:u64,
	pub cr4:u64,
	pub cr8:u64,
	pub efer:u64,
	pub st0:[u8;10],
	pub st1:[u8;10],
	pub st2:[u8;10],
	pub st3:[u8;10],
	pub st4:[u8;10],
	pub st5:[u8;10],
	pub st6:[u8;10],
	pub st7:[u8;10],
	pub fctrl:u32,
	pub fstat:u32,
	pub ftag:u32,
	pub fiseg:u32,
	pub fioff:u32,
	pub foseg:u32,
	pub fooff:u32,
	pub fop:u32,
	pub xmm0:[u8;16],
	pub xmm1:[u8;16],
	pub xmm2:[u8;16],
	pub xmm3:[u8;16],
	pub xmm4:[u8;16],
	pub xmm5:[u8;16],
	pub xmm6:[u8;16],
	pub xmm7:[u8;16],
	pub xmm8:[u8;16],
	pub xmm9:[u8;16],
	pub xmm10:[u8;16],
	pub xmm11:[u8;16],
	pub xmm12:[u8;16],
	pub xmm13:[u8;16],
	pub xmm14:[u8;16],
	pub xmm15:[u8;16],
	pub mxcsr:u32,
}

impl GdbRegisterTrait for QemuGdbX64Registers
{
	// This trait method is generated from XML in QEMU.
	fn from_be_str(buffer:&str)->Self
	{
		let mut r=Self::default();
		r.rax=u64::from_be(u64::from_str_radix(&buffer[0..16],16).unwrap());
		r.rbx=u64::from_be(u64::from_str_radix(&buffer[16..32],16).unwrap());
		r.rcx=u64::from_be(u64::from_str_radix(&buffer[32..48],16).unwrap());
		r.rdx=u64::from_be(u64::from_str_radix(&buffer[48..64],16).unwrap());
		r.rsi=u64::from_be(u64::from_str_radix(&buffer[64..80],16).unwrap());
		r.rdi=u64::from_be(u64::from_str_radix(&buffer[80..96],16).unwrap());
		r.rbp=u64::from_be(u64::from_str_radix(&buffer[96..112],16).unwrap());
		r.rsp=u64::from_be(u64::from_str_radix(&buffer[112..128],16).unwrap());
		r.r8=u64::from_be(u64::from_str_radix(&buffer[128..144],16).unwrap());
		r.r9=u64::from_be(u64::from_str_radix(&buffer[144..160],16).unwrap());
		r.r10=u64::from_be(u64::from_str_radix(&buffer[160..176],16).unwrap());
		r.r11=u64::from_be(u64::from_str_radix(&buffer[176..192],16).unwrap());
		r.r12=u64::from_be(u64::from_str_radix(&buffer[192..208],16).unwrap());
		r.r13=u64::from_be(u64::from_str_radix(&buffer[208..224],16).unwrap());
		r.r14=u64::from_be(u64::from_str_radix(&buffer[224..240],16).unwrap());
		r.r15=u64::from_be(u64::from_str_radix(&buffer[240..256],16).unwrap());
		r.rip=u64::from_be(u64::from_str_radix(&buffer[256..272],16).unwrap());
		r.eflags=u32::from_be(u32::from_str_radix(&buffer[272..280],16).unwrap());
		r.cs=u32::from_be(u32::from_str_radix(&buffer[280..288],16).unwrap());
		r.ss=u32::from_be(u32::from_str_radix(&buffer[288..296],16).unwrap());
		r.ds=u32::from_be(u32::from_str_radix(&buffer[296..304],16).unwrap());
		r.es=u32::from_be(u32::from_str_radix(&buffer[304..312],16).unwrap());
		r.fs=u32::from_be(u32::from_str_radix(&buffer[312..320],16).unwrap());
		r.gs=u32::from_be(u32::from_str_radix(&buffer[320..328],16).unwrap());
		r.fs_base=u64::from_be(u64::from_str_radix(&buffer[328..344],16).unwrap());
		r.gs_base=u64::from_be(u64::from_str_radix(&buffer[344..360],16).unwrap());
		r.k_gs_base=u64::from_be(u64::from_str_radix(&buffer[360..376],16).unwrap());
		r.cr0=u64::from_be(u64::from_str_radix(&buffer[376..392],16).unwrap());
		r.cr2=u64::from_be(u64::from_str_radix(&buffer[392..408],16).unwrap());
		r.cr3=u64::from_be(u64::from_str_radix(&buffer[408..424],16).unwrap());
		r.cr4=u64::from_be(u64::from_str_radix(&buffer[424..440],16).unwrap());
		r.cr8=u64::from_be(u64::from_str_radix(&buffer[440..456],16).unwrap());
		r.efer=u64::from_be(u64::from_str_radix(&buffer[456..472],16).unwrap());
		for i in (0..10).step_by(2) {r.st0[i]=u8::from_str_radix(&buffer[i+472..i+474],16).unwrap();}
		for i in (0..10).step_by(2) {r.st1[i]=u8::from_str_radix(&buffer[i+492..i+494],16).unwrap();}
		for i in (0..10).step_by(2) {r.st2[i]=u8::from_str_radix(&buffer[i+512..i+514],16).unwrap();}
		for i in (0..10).step_by(2) {r.st3[i]=u8::from_str_radix(&buffer[i+532..i+534],16).unwrap();}
		for i in (0..10).step_by(2) {r.st4[i]=u8::from_str_radix(&buffer[i+552..i+554],16).unwrap();}
		for i in (0..10).step_by(2) {r.st5[i]=u8::from_str_radix(&buffer[i+572..i+574],16).unwrap();}
		for i in (0..10).step_by(2) {r.st6[i]=u8::from_str_radix(&buffer[i+592..i+594],16).unwrap();}
		for i in (0..10).step_by(2) {r.st7[i]=u8::from_str_radix(&buffer[i+612..i+614],16).unwrap();}
		r.fctrl=u32::from_be(u32::from_str_radix(&buffer[632..640],16).unwrap());
		r.fstat=u32::from_be(u32::from_str_radix(&buffer[640..648],16).unwrap());
		r.ftag=u32::from_be(u32::from_str_radix(&buffer[648..656],16).unwrap());
		r.fiseg=u32::from_be(u32::from_str_radix(&buffer[656..664],16).unwrap());
		r.fioff=u32::from_be(u32::from_str_radix(&buffer[664..672],16).unwrap());
		r.foseg=u32::from_be(u32::from_str_radix(&buffer[672..680],16).unwrap());
		r.fooff=u32::from_be(u32::from_str_radix(&buffer[680..688],16).unwrap());
		r.fop=u32::from_be(u32::from_str_radix(&buffer[688..696],16).unwrap());
		for i in (0..16).step_by(2) {r.xmm0[i]=u8::from_str_radix(&buffer[i+696..i+698],16).unwrap();}
		for i in (0..16).step_by(2) {r.xmm1[i]=u8::from_str_radix(&buffer[i+728..i+730],16).unwrap();}
		for i in (0..16).step_by(2) {r.xmm2[i]=u8::from_str_radix(&buffer[i+760..i+762],16).unwrap();}
		for i in (0..16).step_by(2) {r.xmm3[i]=u8::from_str_radix(&buffer[i+792..i+794],16).unwrap();}
		for i in (0..16).step_by(2) {r.xmm4[i]=u8::from_str_radix(&buffer[i+824..i+826],16).unwrap();}
		for i in (0..16).step_by(2) {r.xmm5[i]=u8::from_str_radix(&buffer[i+856..i+858],16).unwrap();}
		for i in (0..16).step_by(2) {r.xmm6[i]=u8::from_str_radix(&buffer[i+888..i+890],16).unwrap();}
		for i in (0..16).step_by(2) {r.xmm7[i]=u8::from_str_radix(&buffer[i+920..i+922],16).unwrap();}
		for i in (0..16).step_by(2) {r.xmm8[i]=u8::from_str_radix(&buffer[i+952..i+954],16).unwrap();}
		for i in (0..16).step_by(2) {r.xmm9[i]=u8::from_str_radix(&buffer[i+984..i+986],16).unwrap();}
		for i in (0..16).step_by(2) {r.xmm10[i]=u8::from_str_radix(&buffer[i+1016..i+1018],16).unwrap();}
		for i in (0..16).step_by(2) {r.xmm11[i]=u8::from_str_radix(&buffer[i+1048..i+1050],16).unwrap();}
		for i in (0..16).step_by(2) {r.xmm12[i]=u8::from_str_radix(&buffer[i+1080..i+1082],16).unwrap();}
		for i in (0..16).step_by(2) {r.xmm13[i]=u8::from_str_radix(&buffer[i+1112..i+1114],16).unwrap();}
		for i in (0..16).step_by(2) {r.xmm14[i]=u8::from_str_radix(&buffer[i+1144..i+1146],16).unwrap();}
		for i in (0..16).step_by(2) {r.xmm15[i]=u8::from_str_radix(&buffer[i+1176..i+1178],16).unwrap();}
		r.mxcsr=u32::from_be(u32::from_str_radix(&buffer[1208..1216],16).unwrap());
		r
	}
}