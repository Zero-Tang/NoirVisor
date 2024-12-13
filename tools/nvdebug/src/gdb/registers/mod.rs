// This module lists all definitions of registers in GDB Protocol.
pub mod x64;

pub trait GdbRegisterTrait
{
	fn from_be_str(buffer:&str)->Self;
}