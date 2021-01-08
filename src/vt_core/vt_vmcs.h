/*
  NoirVisor - A cross-platform designed HyperVisor

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file defines encodings of VMCS.

  This program is distributed in the hope that it will be useful, but
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  Reference:
  Appendix B, Volume 3D,
  Intel 64 and IA-32 Architectures Software Developer's Manual
  Order Number (Combined Volumes: 325462-070US, May 2019)

  File Location: /vt_core/vt_vmcs.h
*/

typedef enum _vmx_vmcs_encoding
{
	// 16-Bit Control Fields
	virtual_processor_identifier=0x0,
	posted_interrupt_notification_vector=0x2,
	ept_pointer_index=0x4,
	// 16-Bit Guest State Fields
	guest_es_selector=0x800,
	guest_cs_selector=0x802,
	guest_ss_selector=0x804,
	guest_ds_selector=0x806,
	guest_fs_selector=0x808,
	guest_gs_selector=0x80A,
	guest_ldtr_selector=0x80C,
	guest_tr_selector=0x80E,
	guest_interrupt_status=0x810,
	pml_index=0x812,
	// 16-Bit Host State Fields
	host_es_selector=0xC00,
	host_cs_selector=0xC02,
	host_ss_selector=0xC04,
	host_ds_selector=0xC06,
	host_fs_selector=0xC08,
	host_gs_selector=0xC0A,
	host_tr_selector=0xC0C,
	// 64-Bit Control Fields
	address_of_io_bitmap_a=0x2000,
	address_of_io_bitmap_b=0x2002,
	address_of_msr_bitmap=0x2004,
	vmexit_msr_store_address=0x2006,
	vmexit_msr_load_address=0x2008,
	vmentry_msr_load_address=0x200A,
	executive_vmcs_pointer=0x200C,
	pml_address=0x200E,
	tsc_offset=0x2010,
	virtual_apic_address=0x2012,
	apic_access_address=0x2014,
	posted_interrupt_descriptor_address=0x2016,
	vm_function_controls=0x2018,
	ept_pointer=0x201A,
	eoi_exit_bitmap0=0x201C,
	eoi_exit_bitmap1=0x201E,
	eoi_exit_bitmap2=0x2020,
	eoi_exit_bitmap3=0x2022,
	eptp_list_address=0x2024,
	vmread_bitmap_address=0x2026,
	vmwrite_bitmap_address=0x2028,
	virtualization_exception_information_address=0x202A,
	xss_exiting_bitmap=0x202C,
	encls_exiting_bitmap=0x202E,
	sub_page_permission_table=0x2030,
	tsc_multiplier=0x2032,
	enclv_exiting_bitmap=0x2036,
	// 64-Bit Read-Only Fields
	guest_physical_address=0x2400,
	// 64-Bit Guest-State Fields
	vmcs_link_pointer=0x2800,
	guest_msr_ia32_debug_ctrl=0x2802,
	guest_msr_ia32_pat=0x2804,
	guest_msr_ia32_efer=0x2806,
	guest_msr_ia32_perf_global_ctrl=0x2808,
	guest_pdpte0=0x280A,
	guest_pdpte1=0x280C,
	guest_pdpte2=0x280E,
	guest_pdpte3=0x2810,
	guest_msr_ia32_bound_config=0x2812,
	guest_msr_ia32_rtit_ctrl=0x2814,
	guest_msr_ia32_pkrs=0x2818,
	// 64-Bit Host State Fields
	host_msr_ia32_pat=0x2C00,
	host_msr_ia32_efer=0x2C02,
	host_msr_ia32_perf_global_ctrl=0x2C04,
	host_msr_ia32_pkrs=0x2C06,
	// 32-Bit Control Fields
	pin_based_vm_execution_controls=0x4000,
	primary_processor_based_vm_execution_controls=0x4002,
	exception_bitmap=0x4004,
	page_fault_error_code_mask=0x4006,
	page_fault_error_code_match=0x4008,
	cr3_target_count=0x400A,
	vmexit_controls=0x400C,
	vmexit_msr_store_count=0x400E,
	vmexit_msr_load_count=0x4010,
	vmentry_controls=0x4012,
	vmentry_msr_load_count=0x4014,
	vmentry_interruption_information_field=0x4016,
	vmentry_exception_error_code=0x4018,
	vmentry_instruction_length=0x401A,
	tpr_threshold=0x401C,
	secondary_processor_based_vm_execution_controls=0x401E,
	ple_gap=0x4020,
	ple_window=0x4022,
	// 32-Bit Read-Only Fields
	vm_instruction_error=0x4400,
	vmexit_reason=0x4402,
	vmexit_interruption_information=0x4404,
	vmexit_interruption_error_code=0x4406,
	idt_vectoring_information=0x4408,
	idt_vectoring_error_code=0x440A,
	vmexit_instruction_length=0x440C,
	vmexit_instruction_information=0x440E,
	// 32-Bit Guest-State Fields
	guest_es_limit=0x4800,
	guest_cs_limit=0x4802,
	guest_ss_limit=0x4804,
	guest_ds_limit=0x4806,
	guest_fs_limit=0x4808,
	guest_gs_limit=0x480A,
	guest_ldtr_limit=0x480C,
	guest_tr_limit=0x480E,
	guest_gdtr_limit=0x4810,
	guest_idtr_limit=0x4812,
	guest_es_access_rights=0x4814,
	guest_cs_access_rights=0x4816,
	guest_ss_access_rights=0x4818,
	guest_ds_access_rights=0x481A,
	guest_fs_access_rights=0x481C,
	guest_gs_access_rights=0x481E,
	guest_ldtr_access_rights=0x4820,
	guest_tr_access_rights=0x4822,
	guest_interruptibility_state=0x4824,
	guest_activity_state=0x4826,
	guest_smbase=0x4828,
	guest_msr_ia32_sysenter_cs_=0x482A,
	vmx_preemption_timer_value=0x482E,
	// 32-Bit Host State Fields
	host_msr_ia32_sysenter_cs_=0x4C00,
	// Natural-Width Control Fields
	cr0_guest_host_mask=0x6000,
	cr4_guest_host_mask=0x6002,
	cr0_read_shadow=0x6004,
	cr4_read_shadow=0x6006,
	cr3_target_value0=0x6008,
	cr3_target_value1=0x600A,
	cr3_target_value2=0x600C,
	cr3_target_value3=0x600E,
	// Natural-Width Read-Only Fields
	vmexit_qualification=0x6400,
	io_rcx=0x6402,
	io_rsi=0x6404,
	io_rdi=0x6406,
	io_rip=0x6408,
	guest_linear_address=0x640A,
	// Natural-Width Guest-State Fields
	guest_cr0=0x6800,
	guest_cr3=0x6802,
	guest_cr4=0x6804,
	guest_es_base=0x6806,
	guest_cs_base=0x6808,
	guest_ss_base=0x680A,
	guest_ds_base=0x680C,
	guest_fs_base=0x680E,
	guest_gs_base=0x6810,
	guest_ldtr_base=0x6812,
	guest_tr_base=0x6814,
	guest_gdtr_base=0x6816,
	guest_idtr_base=0x6818,
	guest_dr7=0x681A,
	guest_rsp=0x681C,
	guest_rip=0x681E,
	guest_rflags=0x6820,
	guest_pending_debug_exceptions=0x6822,
	guest_msr_ia32_sysenter_esp=0x6824,
	guest_msr_ia32_sysenter_eip=0x6826,
	guest_s_cet=0x6828,
	guest_ssp=0x682A,
	guest_msr_ia32_interrupt_ssp_table_addr=0x682C,
	// Natural-Width Host-State Fields
	host_cr0=0x6C00,
	host_cr3=0x6C02,
	host_cr4=0x6C04,
	host_fs_base=0x6C06,
	host_gs_base=0x6C08,
	host_tr_base=0x6C0A,
	host_gdtr_base=0x6C0C,
	host_idtr_base=0x6C0E,
	host_msr_ia32_sysenter_esp=0x6C10,
	host_msr_ia32_sysenter_eip=0x6C12,
	host_rsp=0x6C14,
	host_rip=0x6C16,
	host_s_cet=0x6C18,
	host_ssp=0x6C1A,
	host_msr_ia32_interrupt_ssp_table_addr=0x6C1C
}vmx_vmcs_encoding,*vmx_vmcs_encoding_p;

/*
  In NoirVisor's Nested Virtualization, the
  convention is given as the followings:

  Offset	Length		Information
  0x0		0x100		VMCS Basic Header
  0x100		0x100		Read-Only Area
  0x100		0x40		16-Bit Read-Only Field
  0x140		0x40		64-Bit Read-Only Field
  0x180		0x40		32-Bit Read-Only Field
  0x1C0		0x40		Natual-Width Read-Only Field
  0x200		0x200		Host State Area
  0x200		0x80		16-Bit Host-State Field
  0x280		0x80		64-Bit Host-State Field
  0x300		0x80		32-Bit Host-State-Field
  0x380		0x80		Natural-Width Host-State Field
  0x400		0x400		Guest State Area
  0x400		0x80		16-Bit Guest-State Field
  0x480		0x180		64-Bit Guest-State Field
  0x600		0x100		32-Bit Guest-State Field
  0x700		0x100		Natural-Width Guest-State Field
  0x800		0x400		Control Area
  0x800		0x80		16-Bit Control Field
  0x880		0x180		64-Bit Control Field
  0xA00		0x100		32-Bit Control Field
  0xB00		0x100		Natural-Width Control Field
  0xC00		0x400		Reserved.
*/

/*
  VMCS Field has following encodings;
  31					14	12	12	10			1	0
  +---------------------+---+---+---+-----------+---+
  | Reserved			| W | R | T | Field		| H	|
  +---------------------+---+---+---+-----------+---+
  W: Width indicator
  R: Reserved
  T: Type
  H: Higher 32 Bit
*/

typedef union _ia32_vmx_vmcs_encoding
{
	struct
	{
		u32 hi:1;
		u32 field:10;
		u32 type:2;
		u32 reserved1:1;
		u32 width:2;
		u32 reserved2:18;
	};
	u32 value;
}ia32_vmx_vmcs_encoding,*ia32_vmx_vmcs_encoding_p;

#if defined(_vt_nvcpu)
const u16 noir_vt_vmcs_redirection[4][4]=
{
//	Control		Read-Only	Guest	Host
	{0x800,		0x100,		0x400,	0x200},		// 16-Bit Fields
	{0x880,		0x140,		0x480,	0x280},		// 64-Bit Fields
	{0xA00,		0x180,		0x600,	0x300},		// 32-Bit Fields
	{0xB00,		0x1C0,		0x700,	0x380}		// Natural-Width
};

const u8 noir_vt_vmcs_limit[4][4]=
{
//	Control		Read-Only	Guest	Host
	{0x3,		0x0,		0x10,	0x7},		// 16-Bit Fields
	{0x1A,		0x1,		0xA,	0x3},		// 64-Bit Fields
	{0x11,		0x8,		0x17,	0x1},		// 32-Bit Fields
	{0x8,		0x6,		0x13,	0xB}		// Natural-Width
};

const u8 noir_vt_vmcs_field_size[4]={2,8,4,sizeof(void*)};
#endif

// This structure should have 256 bytes at maximum
typedef struct _noir_vt_nested_vmcs_header
{
	// Common Header as Required by Intel.
	struct
	{
		u32 revision_id:31;
		u32 shadowing:1;
	};
	u32 vmx_abort_indicator;
	// Reserved 32 bytes for future Intel Modifications.
	u64 reserved[4];
	// 216 bytes remaining.
	union
	{
		// Use clean-bits to cache VMCS info.
		// Value of zero means VMCS is totally uncached.
		// These fields are unlikely to be revised.
		struct
		{
			// Segment Register State.
			u64 sreg:1;
			// Control Register State
			u64 creg:1;
			// Debug Register State.
			u64 dreg:1;
			// Virtual Processor Identifier.
			u64 vpid:1;
			// Pin, PriProc, 2ndProc, Exit, Entry.
			u64 ctrl:1;
			// MSR-Bitmap, I/O-Bitmap Address
			u64 pmpa:1;
			// Auto-Load/Store MSR List
			u64 amsr:1;
			// Guest MSRs stored in VMCS.
			u64 gmsr:1;
			// Host State Area
			u64 host:1;
			u64 reserved:54;
			// VMCS Status indicator.
			u64 active:1;
			u64 launched:1;
		};
		u64 value;
	}clean_fields;
	u64 related_vmxon;
}noir_vt_nested_vmcs_header,*noir_vt_nested_vmcs_header_p;