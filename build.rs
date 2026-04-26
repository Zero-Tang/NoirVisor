fn main()
{
	// It seems Rust compiler cannot track header files for global_asm.
	println!("cargo:rerun-if-changed=src/xpf_core/hv_host/asm_helper_x86.inc");
}