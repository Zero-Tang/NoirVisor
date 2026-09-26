fn main()
{
    println!("cargo:rustc-link-arg=/ENTRY:uefi_entry");
    println!("cargo:rustc-link-arg=/SUBSYSTEM:EFI_APPLICATION");
}