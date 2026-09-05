// NoirVisor CVM Hypercall Interface Definitions.

use core::{arch::{naked_asm, x86_64::__cpuid}, ffi::c_void};

use nvcvm::status::Status;

#[unsafe(naked)] unsafe extern "win64" fn vmcall(fn_index:u32,context:*mut c_void,ctxt_len:usize)->Status
{
	naked_asm!
	(
		"vmcall",
		"ret"
	)
}

#[unsafe(naked)] unsafe extern "win64" fn vmmcall(fn_index:u32,context:*mut c_void,ctxt_len:usize)->Status
{
	naked_asm!
	(
		"vmmcall",
		"ret"
	)
}

unsafe extern "win64" fn unknown_hypercall(_fn_index:u32,_context:*mut c_void,_ctxt_len:usize)->Status
{
	Status::HYPERVISION_ABSENT
}

static mut HYPERCALL_FN:unsafe extern "win64" fn(fn_index:u32,context:*mut c_void,ctxt_len:usize)->Status=unknown_hypercall;

pub(crate) fn init()
{
	let vendor_id=__cpuid(0);
	let mut vendor_str_raw:[u8;12]=[0;12];
	vendor_str_raw[..4].copy_from_slice(&vendor_id.ebx.to_le_bytes());
	vendor_str_raw[4..8].copy_from_slice(&vendor_id.edx.to_le_bytes());
	vendor_str_raw[8..].copy_from_slice(&vendor_id.ecx.to_le_bytes());
	let vendor_str_raw=unsafe{str::from_utf8_unchecked(&vendor_str_raw)};
	unsafe
	{
		HYPERCALL_FN=match vendor_str_raw
		{
			"GenuineIntel"|"VIA VIA VIA "|"CentaurHauls"|"  Shanghai  "=>vmcall,
			"AuthenticAMD"|"AMDisbetter!"|"HygonGenuine"=>vmmcall,
			_=>unknown_hypercall
		}
	}
}

/// Perform a hypercall on the current system.
/// 
/// ## Safety
/// If NoirVisor is not loaded, the behavior is undefined.
#[inline(always)] pub(crate) unsafe fn hypercall<T:Sized>(fn_index:u32,context:*mut T,ctxt_len:usize)->Result<(),Status>
{
	let st=unsafe{HYPERCALL_FN(fn_index,context.cast(),ctxt_len)};
	match st
	{
		Status::SUCCESS=>Ok(()),
		_=>Err(st)
	}
}