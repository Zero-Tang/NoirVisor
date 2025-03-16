/*
 * NoirVisor Core in Rust
 * 
 * Copyright (c) Zero Tang, 2018-2025. All rights reserved.
 * 
 * This file defines assembly-based utilities of NoirVisor Core in Rust.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * without any warranty (no matter implied warranty or merchantability
 * or fitness for a particular purpose, etc.).
 */

#![allow(dead_code)]

pub mod io
{
	// Note that the string-based I/O will never be implemented, in that x86-S will remove them.
	use core::arch::asm;
	use paste::paste;

	macro_rules! build_in_func
	{
		($name:tt,$out_type:tt,$reg:tt) =>
		{
			paste!
			{
				/// # Safety
				/// I/O operations are not guaranteed to be safe.
				#[inline] pub unsafe fn [<in _ $name>](port:u16)->$out_type
				{
					let v:$out_type;
					unsafe
					{
						asm!
						(
							concat!("in ",$reg,",dx"),
							in("dx") port,
							out($reg) v
						);
					}
					v
				}
			}
		};
	}

	macro_rules! build_out_func
	{
		($name:tt,$in_type:tt,$reg:tt) =>
		{
			paste!
			{
				/// # Safety
				/// I/O operations are not guaranteed to be safe.
				#[inline] pub unsafe fn [<out _ $name>](port:u16,val:$in_type)
				{
					unsafe
					{
						asm!
						(
							concat!("out dx,",$reg),
							in("dx") port,
							in($reg) val
						);
					}
				}
			}
		};
	}

	build_in_func!(byte,u8,"al");
	build_in_func!(word,u16,"ax");
	build_in_func!(dword,u32,"eax");
	build_out_func!(byte,u8,"al");
	build_out_func!(word,u16,"ax");
	build_out_func!(dword,u32,"eax");
}

pub mod seg
{
	use core::arch::asm;
	use paste::paste;

	use crate::xpf_core::x86::descriptors::DescriptorTable;

	macro_rules! build_fn
	{
		($name:tt) =>
		{
			paste!
			{
				#[inline] pub fn [<read_ $name:lower>]()->u16
				{
					let val:u16;
					unsafe
					{
						asm!
						(
							concat!("mov {val:x},",stringify!($name)),
							val=out(reg) val
						);
					}
					val
				}

				#[inline] pub fn [<write_ $name:lower>](val:u16)
				{
					unsafe
					{
						asm!
						(
							concat!("mov ",stringify!($name),"{val:x}"),
							val=in(reg) val
						);
					}
				}
			}
		};
	}

	macro_rules! build_fn_special_16bit
	{
		($name:tt) =>
		{
			paste!
			{
				#[inline] pub fn [<read_ $name:lower>]()->u16
				{
					let val:u16;
					unsafe
					{
						asm!
						(
							concat!("s",stringify!($name)," {val:x}"),
							val=out(reg) val
						);
					}
					val
				}

				#[inline] pub fn [<write_ $name:lower>](val:u16)
				{
					unsafe
					{
						asm!
						(
							concat!("l",stringify!($name)," {val:x}"),
							val=in(reg) val
						);
					}
				}
			}
		};
	}

	macro_rules! build_fn_special_80bit
	{
		($name:tt) =>
		{
			paste!
			{
				#[inline] pub fn [<read_ $name:lower r>]()->DescriptorTable
				{
					let mut val=DescriptorTable{limit:0,base:0};
					unsafe
					{
						asm!
						(
							concat!("s",stringify!($name)," [{p}]"),
							p=in(reg) &raw mut val
						);
					}
					val
				}

				/// # Safety
				/// Use this function only if you understand the outcome
				/// of modifying the descriptor table register!
				#[inline] pub unsafe fn [<write_ $name:lower r>](reg:*const DescriptorTable)
				{
					unsafe
					{
						asm!
						(
							concat!("l",stringify!($name)," [{p}]"),
							p=in(reg) reg
						);
					}
				}
			}
		};
	}

	build_fn!(cs);
	build_fn!(ds);
	build_fn!(es);
	build_fn!(fs);
	build_fn!(gs);
	build_fn!(ss);

	build_fn_special_16bit!(tr);
	build_fn_special_16bit!(ldtr);

	build_fn_special_80bit!(gdt);
	build_fn_special_80bit!(idt);
}

pub mod crdr
{
	use core::arch::asm;
	use paste::paste;

	macro_rules! build_fn
	{
		($name:tt) =>
		{
			paste!
			{
				#[inline] pub fn [<read_ $name:lower>]()->u64
				{
					let val:u64;
					unsafe
					{
						asm!
						(
							concat!("mov {val},",stringify!($name)),
							val=out(reg) val
						);
					}
					val
				}

				#[inline] pub fn [<write_ $name:lower>](val:u64)
				{
					unsafe
					{
						asm!
						(
							concat!("mov ",stringify!($name),",{val}"),
							val=in(reg) val
						);
					}
				}
			}
		};
	}

	build_fn!(cr0);
	build_fn!(cr2);
	build_fn!(cr3);
	build_fn!(cr4);
	build_fn!(cr8);

	build_fn!(dr0);
	build_fn!(dr1);
	build_fn!(dr2);
	build_fn!(dr3);
	build_fn!(dr6);
	build_fn!(dr7);
}

pub mod cpuid
{
	use core::arch::x86_64::*;
	
	#[inline] pub fn cpuid(ia:u32,ic:u32,a:Option<&mut u32>,b:Option<&mut u32>,c:Option<&mut u32>,d:Option<&mut u32>)
	{
		let r=unsafe{__cpuid_count(ia,ic)};
		if let Some(a)=a {*a=r.eax};
		if let Some(b)=b {*b=r.ebx};
		if let Some(c)=c {*c=r.ecx};
		if let Some(d)=d {*d=r.edx};
	}

	#[inline] pub fn cpuid2(ia:u32,ic:u32)->(u32,u32,u32,u32)
	{
		let r=unsafe{__cpuid_count(ia,ic)};
		(r.eax,r.ebx,r.ecx,r.edx)
	}
}

pub mod msr
{
	use core::arch::asm;

	/// # Read MSR
	/// Returns the Model-Specific Register value with specified `index`.
	#[inline] pub fn rdmsr(index:u32)->u64
	{
		let lo:u32;
		let hi:u32;
		unsafe
		{
			asm!
			(
				"rdmsr",
				in("ecx") index,
				out("eax") lo,
				out("edx") hi
			);
		}
		(lo as u64)|((hi as u64)<<32)
	}

	/// # Write MSR
	/// Writes the Model-Specific Register specified in `index` with `value`.
	#[inline] pub fn wrmsr(index:u32,value:u64)
	{
		let lo:u32=(value&0xffffffff) as u32;
		let hi:u32=(value>>32) as u32;
		unsafe
		{
			asm!
			(
				"wrmsr",
				in("ecx") index,
				in("eax") lo,
				in("edx") hi
			);
		}
	}
}

pub mod svm
{
	use core::arch::asm;

	#[inline] pub fn vmmcall(index:u32,context:usize)
	{
		unsafe
		{
			asm!
			(
				"vmmcall",
				in("ecx") index,
				in("rdx") context
			);
		}
	}

	#[inline] pub fn vmload(vmcb_phys:u64)
	{
		unsafe
		{
			asm!
			(
				"vmload",
				in("rax") vmcb_phys
			);
		}
	}

	#[inline] pub fn vmsave(vmcb_phys:u64)
	{
		unsafe
		{
			asm!
			(
				"vmsave",
				in("rax") vmcb_phys
			);
		}
	}

	#[inline] pub fn stgi()
	{
		unsafe
		{
			asm!("stgi");
		}
	}

	#[inline] pub fn clgi()
	{
		unsafe 
		{
			asm!("clgi");
		}
	}

	#[inline] pub fn invlpga(ptr:u64,asid:u32)
	{
		unsafe
		{
			asm!
			(
				"invlpga rax,ecx",
				in("rax") ptr,
				in("ecx") asid
			);
		}
	}
}

pub mod vt
{
	use core::{arch::asm,fmt::*};
	const VM_INSTRUCTION_ERROR:usize=0x4400;

	pub struct EmptyUnit(pub ());

	impl Display for EmptyUnit
	{
		fn fmt(&self, _f: &mut Formatter<'_>) -> Result
		{
			Ok(())
		}
	}

	macro_rules! dispatch_vmx_result
	{
		($e:tt,$v:expr) =>
		{
			match $e
			{
				0=>VmxResult::Ok($v),
				1=>VmxResult::Err(vmread32(VM_INSTRUCTION_ERROR).unwrap()),
				2=>VmxResult::NoVmcs,
				_=>panic!("Unexpected VMX Fail Flag: {}!",$e)
			}
		};
	}

	/// ## Safety
	/// The `context` is in fact a raw pointer. Make sure it's valid.
	#[inline] pub unsafe fn vmcall(index:u32,context:usize)->VmxResult<EmptyUnit>
	{
		let vmx_err:u8;
		unsafe
		{
			asm!
			(
				"vmcall",
				"setc {cf}",
				"setz {zf}",
				"adc {cf},{zf}",
				in("ecx") index,
				in("rdx") context,
				zf=out(reg_byte) _,
				cf=out(reg_byte) vmx_err
			);
			dispatch_vmx_result!(vmx_err,EmptyUnit(()))
		}
	}

	/// ## Safety
	/// The `vmxon_region_phys` is a raw pointer to a physical address.
	#[inline] pub unsafe fn vmxon(vmxon_region_phys:*const u64)->VmxResult<EmptyUnit>
	{
		let vmx_err:u8;
		unsafe
		{
			asm!
			(
				"vmxon qword ptr [{vmxon_phys}]",
				"setc {cf}",
				"setz {zf}",
				"adc {cf},{zf}",
				vmxon_phys=in(reg) vmxon_region_phys,
				zf=out(reg_byte) _,
				cf=out(reg_byte) vmx_err
			);
			dispatch_vmx_result!(vmx_err,EmptyUnit(()))
		}
	}

	/// ## Safety
	/// Just understand what `vmxoff` implies.
	#[inline] pub unsafe fn vmxoff()->VmxResult<EmptyUnit>
	{
		let vmx_err:u8;
		unsafe
		{
			asm!
			(
				"vmxoff",
				"setc {cf}",
				"setz {zf}",
				"adc {cf},{zf}",
				zf=out(reg_byte) _,
				cf=out(reg_byte) vmx_err
			);
			dispatch_vmx_result!(vmx_err,EmptyUnit(()))
		}
	}

	/// ## Safety
	/// The `vmcs_phys` is a raw pointer to the physical address of VMCS.
	#[inline] pub unsafe fn vmclear(vmcs_phys:*const u64)->VmxResult<EmptyUnit>
	{
		let vmx_err:u8;
		unsafe
		{
			asm!
			(
				"vmclear qword ptr [{vmcs_phys}]",
				"setc {cf}",
				"setz {zf}",
				"adc {cf},{zf}",
				vmcs_phys=in(reg) vmcs_phys,
				zf=out(reg_byte) _,
				cf=out(reg_byte) vmx_err
			);
			dispatch_vmx_result!(vmx_err,EmptyUnit(()))
		}
	}

	/// ## Safety
	/// The `vmcs_phys` is a raw pointer to the physical address of VMCS.
	#[inline] pub unsafe fn vmptrld(vmcs_phys:*const u64)->VmxResult<EmptyUnit>
	{
		let vmx_err:u8;
		unsafe
		{
			asm!
			(
				"vmptrld qword ptr [{vmcs_phys}]",
				"setc {cf}",
				"setz {zf}",
				"adc {cf},{zf}",
				vmcs_phys=in(reg) vmcs_phys,
				zf=out(reg_byte) _,
				cf=out(reg_byte) vmx_err
			);
			dispatch_vmx_result!(vmx_err,EmptyUnit(()))
		}
	}

	/// ## Safety
	/// This function returns the physical address of current VMCS as a result.
	#[inline] pub unsafe fn vmptrst()->VmxResult<u64>
	{
		let vmx_err:u8;
		let mut vmcs_phys:u64=0;
		unsafe
		{
			asm!
			(
				"vmptrld qword ptr [{vmcs_phys}]",
				"setc {cf}",
				"setz {zf}",
				"adc {cf},{zf}",
				vmcs_phys=in(reg) &raw mut vmcs_phys,
				zf=out(reg_byte) _,
				cf=out(reg_byte) vmx_err
			);
			dispatch_vmx_result!(vmx_err,vmcs_phys)
		}
	}

	/// ## Safety
	/// This function drastically changes the processor state!
	#[inline] pub unsafe fn vmlaunch()->VmxResult<EmptyUnit>
	{
		let vmx_err:u8;
		unsafe
		{
			asm!
			(
				"vmlaunch",
				"setc {cf}",
				"setz {zf}",
				"adc {cf},{zf}",
				zf=out(reg_byte) _,
				cf=out(reg_byte) vmx_err
			);
			dispatch_vmx_result!(vmx_err,EmptyUnit(()))
		}
	}

	/// ## Safety
	/// The `vmcs_phys` is a raw pointer to the physical address of VMCS.
	#[inline] unsafe fn vmread(field:usize,value:*mut usize)->u8
	{
		let vmx_err:u8;
		unsafe
		{
			asm!
			(
				"vmread [{value}],{field}",
				"setc {cf}",
				"setz {zf}",
				"adc {cf},{zf}",
				field=in(reg) field,
				value=in(reg) value,
				zf=out(reg_byte) _,
				cf=out(reg_byte) vmx_err
			);
			vmx_err
		}
	}

	#[inline] unsafe fn vmwrite(field:usize,value:*const usize)->u8
	{
		let vmx_err:u8;
		unsafe
		{
			asm!
			(
				"vmwrite {field},[{value}]",
				"setc {cf}",
				"setz {zf}",
				"adc {cf},{zf}",
				field=in(reg) field,
				value=in(reg) value,
				zf=out(reg_byte) _,
				cf=out(reg_byte) vmx_err
			);
			vmx_err
		}
	}

	const INVEPT_SINGLE:usize=1;
	const INVEPT_GLOBAL:usize=2;

	pub enum InveptContext
	{
		Single(u64),
		Global
	}

	impl InveptContext
	{
		fn as_operands(&self)->(usize,InveptDescriptor)
		{
			match self
			{
				InveptContext::Single(v)=>(INVEPT_SINGLE,InveptDescriptor{eptp:*v,reserved:0}),
				InveptContext::Global=>(INVEPT_GLOBAL,InveptDescriptor{eptp:0,reserved:0})
			}
		}
	}

	#[repr(C)] struct InveptDescriptor
	{
		eptp:u64,
		reserved:u64
	}

	/// ## Safety
	/// This function invalidates TLB of EPT. Understand what it implies if you use this instruction.
	#[inline] pub unsafe fn invept(context:&InveptContext)->VmxResult<EmptyUnit>
	{
		let vmx_err:u8;
		let (inv_type,descriptor)=context.as_operands();
		unsafe
		{
			asm!
			(
				"invept {inv_type},xmmword ptr [{descriptor}]",
				"setc {cf}",
				"setz {zf}",
				"adc {cf},{zf}",
				inv_type=in(reg) inv_type,
				descriptor=in(reg) &raw const descriptor,
				zf=out(reg_byte) _,
				cf=out(reg_byte) vmx_err
			);
			dispatch_vmx_result!(vmx_err,EmptyUnit(()))
		}
	}

	const INVVPID_INDIVIDUAL_ADDRESS:usize=0;
	const INVVPID_SINGLE_CONTEXT:usize=1;
	const INVVPID_GLOBAL_CONTEXT:usize=2;
	const INVVPID_RETAIN_GLOBAL:usize=3;

	pub enum InvvpidContext
	{
		LinearAddress(u16,u64),
		Single(u16),
		Global,
		RetainGlobal(u16)
	}

	impl InvvpidContext
	{
		fn as_operands(&self)->(usize,InvvpidDescriptor)
		{
			match self
			{
				InvvpidContext::LinearAddress(vpid,la)=>(INVVPID_INDIVIDUAL_ADDRESS,InvvpidDescriptor{vpid:*vpid,reserved0:0,reserved1:0,linear_address:*la}),
				InvvpidContext::Single(vpid)=>(INVVPID_SINGLE_CONTEXT,InvvpidDescriptor{vpid:*vpid,reserved0:0,reserved1:0,linear_address:0}),
				InvvpidContext::Global=>(INVVPID_GLOBAL_CONTEXT,InvvpidDescriptor{vpid:0,reserved0:0,reserved1:0,linear_address:0}),
				InvvpidContext::RetainGlobal(vpid)=>(INVVPID_RETAIN_GLOBAL,InvvpidDescriptor{vpid:*vpid,reserved0:0,reserved1:0,linear_address:0})
			}
		}
	}

	#[repr(C)] struct InvvpidDescriptor
	{
		vpid:u16,
		reserved0:u16,
		reserved1:u32,
		linear_address:u64
	}

	/// ## Safety
	/// This function invalidates TLB of the Guest. Understand what it implies if you use this instruction.
	#[inline] pub unsafe fn invvpid(context:&InvvpidContext)->VmxResult<EmptyUnit>
	{
		let vmx_err:u8;
		let (inv_type,descriptor)=context.as_operands();
		unsafe
		{
			asm!
			(
				"invvpid {inv_type},xmmword ptr [{descriptor}]",
				"setc {cf}",
				"setz {zf}",
				"adc {cf},{zf}",
				inv_type=in(reg) inv_type,
				descriptor=in(reg) &raw const descriptor,
				zf=out(reg_byte) _,
				cf=out(reg_byte) vmx_err
			);
			dispatch_vmx_result!(vmx_err,EmptyUnit(()))
		}
	}
	
	// Unfortunately, we won't be able to abuse generics to read/write VMCS...
	pub enum VmxResult<T>
	{
		Ok(T),
		Err(u32),
		NoVmcs
	}
	
	impl<T> VmxResult<T>
	{
		#[inline(always)] pub fn unwrap(self)->T
		{
			match self
			{
				VmxResult::Ok(v)=>v,
				_=>panic!("Unwrapping Unsuccessful VMX Instruction Result!")
			}
		}
	}
	
	macro_rules! vmread_proc
	{
		($f:tt,$t:ty) =>
		{
			{
				let mut v:usize=0;
				let r=vmread($f,&raw mut v);
				dispatch_vmx_result!(r,(v as $t))
			}
		};
	}
	
	macro_rules! vmwrite_proc
	{
		($f:tt,$v:tt) =>
		{
			{
				let v=$v as usize;
				let r=vmwrite($f,&raw const v);
				dispatch_vmx_result!(r,EmptyUnit(()))
			}
		};
	}
	
	/// ## Safety
	/// Understand what this field means.
	#[inline] pub unsafe fn vmread16(field:usize)->VmxResult<u16>
	{
		unsafe
		{
			vmread_proc!(field,u16)
		}
	}
	
	/// ## Safety
	/// Understand what this field means.
	#[inline] pub unsafe fn vmread32(field:usize)->VmxResult<u32>
	{
		unsafe
		{
			vmread_proc!(field,u32)
		}
	}
	
	/// ## Safety
	/// Understand what this field means.
	#[inline] pub unsafe fn vmreadptr(field:usize)->VmxResult<usize>
	{
		unsafe
		{
			vmread_proc!(field,usize)
		}
	}
	
	/// ## Safety
	/// Understand what this field means.
	#[inline] pub unsafe fn vmread64(field:usize)->VmxResult<u64>
	{
		unsafe
		{
			if cfg!(target_arch="x86_64")
			{
				vmread_proc!(field,u64)
			}
			else
			{
				match vmread32(field)
				{
					VmxResult::Ok(v1)=>
					{
						match vmread32(field+1)
						{
							VmxResult::Ok(v2)=>VmxResult::Ok((v1 as u64)|((v2 as u64)<<32)),
							VmxResult::Err(e)=>VmxResult::Err(e),
							VmxResult::NoVmcs=>VmxResult::NoVmcs
						}
					}
					VmxResult::Err(e)=>VmxResult::Err(e),
					VmxResult::NoVmcs=>VmxResult::NoVmcs
				}
			}
		}
	}
	
	/// ## Safety
	/// Understand what this field means.
	#[inline] pub unsafe fn vmwrite16(field:usize,value:u16)->VmxResult<EmptyUnit>
	{
		unsafe
		{
			vmwrite_proc!(field,value)
		}
	}
	
	/// ## Safety
	/// Understand what this field means.
	#[inline] pub unsafe fn vmwrite32(field:usize,value:u32)->VmxResult<EmptyUnit>
	{
		unsafe
		{
			vmwrite_proc!(field,value)
		}
	}
	
	/// ## Safety
	/// Understand what this field means.
	#[inline] pub unsafe fn vmwriteptr(field:usize,value:usize)->VmxResult<EmptyUnit>
	{
		unsafe
		{
			vmwrite_proc!(field,value)
		}
	}
	
	/// ## Safety
	/// Understand what this field means.
	#[inline] pub unsafe fn vmwrite64(field:usize,value:u64)->VmxResult<EmptyUnit>
	{
		unsafe
		{
			if cfg!(target_arch="x86_64")
			{
				vmwrite_proc!(field,value)
			}
			else
			{
				match vmwrite32(field,(value&0xffffffff) as u32)
				{
					VmxResult::Ok(EmptyUnit(()))=>vmwrite32(field+1,(value>>32) as u32),
					VmxResult::Err(e)=>VmxResult::Err(e),
					VmxResult::NoVmcs=>VmxResult::NoVmcs
				}
			}
		}
	}
}

pub mod misc
{
	use core::arch::asm;

	/// # Safety
	/// This function will purposefully generate an `ud2` instruction.
	/// Use this function only to test panic handler of NoirVisor!
	#[inline] pub unsafe fn ud2()
	{
		unsafe
		{
			asm!("ud2");
		}
	}

	#[inline] pub fn wbinvd()
	{
		unsafe
		{
			asm!("wbinvd");
		}
	}
}