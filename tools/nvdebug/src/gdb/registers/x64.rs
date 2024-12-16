// This file defines registers in x64 targets.
use hex;
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
	fn from_be_str(buffer:&str)->Self
	{
		let mut r=Self
		{
			rax:u64::from_be(u64::from_str_radix(&buffer[0..16],16).unwrap()),
			rbx:u64::from_be(u64::from_str_radix(&buffer[16..32],16).unwrap()),
			rcx:u64::from_be(u64::from_str_radix(&buffer[32..48],16).unwrap()),
			rdx:u64::from_be(u64::from_str_radix(&buffer[48..64],16).unwrap()),
			rsi:u64::from_be(u64::from_str_radix(&buffer[64..80],16).unwrap()),
			rdi:u64::from_be(u64::from_str_radix(&buffer[80..96],16).unwrap()),
			rbp:u64::from_be(u64::from_str_radix(&buffer[96..112],16).unwrap()),
			rsp:u64::from_be(u64::from_str_radix(&buffer[112..128],16).unwrap()),
			r8:u64::from_be(u64::from_str_radix(&buffer[128..144],16).unwrap()),
			r9:u64::from_be(u64::from_str_radix(&buffer[144..160],16).unwrap()),
			r10:u64::from_be(u64::from_str_radix(&buffer[160..176],16).unwrap()),
			r11:u64::from_be(u64::from_str_radix(&buffer[176..192],16).unwrap()),
			r12:u64::from_be(u64::from_str_radix(&buffer[192..208],16).unwrap()),
			r13:u64::from_be(u64::from_str_radix(&buffer[208..224],16).unwrap()),
			r14:u64::from_be(u64::from_str_radix(&buffer[224..240],16).unwrap()),
			r15:u64::from_be(u64::from_str_radix(&buffer[240..256],16).unwrap()),
			rip:u64::from_be(u64::from_str_radix(&buffer[256..272],16).unwrap()),
			eflags:u32::from_be(u32::from_str_radix(&buffer[272..280],16).unwrap()),
			cs:u32::from_be(u32::from_str_radix(&buffer[280..288],16).unwrap()),
			ss:u32::from_be(u32::from_str_radix(&buffer[288..296],16).unwrap()),
			ds:u32::from_be(u32::from_str_radix(&buffer[296..304],16).unwrap()),
			es:u32::from_be(u32::from_str_radix(&buffer[304..312],16).unwrap()),
			fs:u32::from_be(u32::from_str_radix(&buffer[312..320],16).unwrap()),
			gs:u32::from_be(u32::from_str_radix(&buffer[320..328],16).unwrap()),
			fs_base:u64::from_be(u64::from_str_radix(&buffer[328..344],16).unwrap()),
			gs_base:u64::from_be(u64::from_str_radix(&buffer[344..360],16).unwrap()),
			k_gs_base:u64::from_be(u64::from_str_radix(&buffer[360..376],16).unwrap()),
			cr0:u64::from_be(u64::from_str_radix(&buffer[376..392],16).unwrap()),
			cr2:u64::from_be(u64::from_str_radix(&buffer[392..408],16).unwrap()),
			cr3:u64::from_be(u64::from_str_radix(&buffer[408..424],16).unwrap()),
			cr4:u64::from_be(u64::from_str_radix(&buffer[424..440],16).unwrap()),
			cr8:u64::from_be(u64::from_str_radix(&buffer[440..456],16).unwrap()),
			efer:u64::from_be(u64::from_str_radix(&buffer[456..472],16).unwrap()),
			st0:[0;10],
			st1:[0;10],
			st2:[0;10],
			st3:[0;10],
			st4:[0;10],
			st5:[0;10],
			st6:[0;10],
			st7:[0;10],
			fctrl:u32::from_be(u32::from_str_radix(&buffer[632..640],16).unwrap()),
			fstat:u32::from_be(u32::from_str_radix(&buffer[640..648],16).unwrap()),
			ftag:u32::from_be(u32::from_str_radix(&buffer[648..656],16).unwrap()),
			fiseg:u32::from_be(u32::from_str_radix(&buffer[656..664],16).unwrap()),
			fioff:u32::from_be(u32::from_str_radix(&buffer[664..672],16).unwrap()),
			foseg:u32::from_be(u32::from_str_radix(&buffer[672..680],16).unwrap()),
			fooff:u32::from_be(u32::from_str_radix(&buffer[680..688],16).unwrap()),
			fop:u32::from_be(u32::from_str_radix(&buffer[688..696],16).unwrap()),
			xmm0:[0;16],
			xmm1:[0;16],
			xmm2:[0;16],
			xmm3:[0;16],
			xmm4:[0;16],
			xmm5:[0;16],
			xmm6:[0;16],
			xmm7:[0;16],
			xmm8:[0;16],
			xmm9:[0;16],
			xmm10:[0;16],
			xmm11:[0;16],
			xmm12:[0;16],
			xmm13:[0;16],
			xmm14:[0;16],
			xmm15:[0;16],
			mxcsr:u32::from_be(u32::from_str_radix(&buffer[1208..1216],16).unwrap()),
		};
		let _=hex::decode_to_slice(&buffer[472..492],&mut r.st0);
		let _=hex::decode_to_slice(&buffer[492..512],&mut r.st1);
		let _=hex::decode_to_slice(&buffer[512..532],&mut r.st2);
		let _=hex::decode_to_slice(&buffer[532..552],&mut r.st3);
		let _=hex::decode_to_slice(&buffer[552..572],&mut r.st4);
		let _=hex::decode_to_slice(&buffer[572..592],&mut r.st5);
		let _=hex::decode_to_slice(&buffer[592..612],&mut r.st6);
		let _=hex::decode_to_slice(&buffer[612..632],&mut r.st7);
		let _=hex::decode_to_slice(&buffer[696..728],&mut r.xmm0);
		let _=hex::decode_to_slice(&buffer[728..760],&mut r.xmm1);
		let _=hex::decode_to_slice(&buffer[760..792],&mut r.xmm2);
		let _=hex::decode_to_slice(&buffer[792..824],&mut r.xmm3);
		let _=hex::decode_to_slice(&buffer[824..856],&mut r.xmm4);
		let _=hex::decode_to_slice(&buffer[856..888],&mut r.xmm5);
		let _=hex::decode_to_slice(&buffer[888..920],&mut r.xmm6);
		let _=hex::decode_to_slice(&buffer[920..952],&mut r.xmm7);
		let _=hex::decode_to_slice(&buffer[952..984],&mut r.xmm8);
		let _=hex::decode_to_slice(&buffer[984..1016],&mut r.xmm9);
		let _=hex::decode_to_slice(&buffer[1016..1048],&mut r.xmm10);
		let _=hex::decode_to_slice(&buffer[1048..1080],&mut r.xmm11);
		let _=hex::decode_to_slice(&buffer[1080..1112],&mut r.xmm12);
		let _=hex::decode_to_slice(&buffer[1112..1144],&mut r.xmm13);
		let _=hex::decode_to_slice(&buffer[1144..1176],&mut r.xmm14);
		let _=hex::decode_to_slice(&buffer[1176..1208],&mut r.xmm15);
		r
	}
}