/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file defines basic structure of VMCB.

  This program is distributed in the hope that it will be useful, but
  without any warranty (no matter implied warranty of merchantablity
  or fitness for a particular purpose, etc.).

  File location: /svm_core/svm_vmcb.h
*/

#include <nvdef.h>

// Following designation allows us to setup and dump VMCB
// data in the way that we perform on Intel VT-x driver.
#define noir_svm_vmread8(v,o)			*(u8*)((ulong_ptr)v+o)
#define noir_svm_vmread16(v,o)			*(u16*)((ulong_ptr)v+o)
#define noir_svm_vmread32(v,o)			*(u32*)((ulong_ptr)v+o)
#define noir_svm_vmread64(v,o)			*(u64*)((ulong_ptr)v+o)

#define noir_svm_vmwrite8(v,o,d)		*(u8*)((ulong_ptr)v+o)=(u8)d
#define noir_svm_vmwrite16(v,o,d)		*(u16*)((ulong_ptr)v+o)=(u16)d
#define noir_svm_vmwrite32(v,o,d)		*(u32*)((ulong_ptr)v+o)=(u32)d
#define noir_svm_vmwrite64(v,o,d)		*(u64*)((ulong_ptr)v+o)=(u64)d

#define noir_svm_vmcb_and8(v,o,d)		*(u8*)((ulong_ptr)v+o)&=(u8)d
#define noir_svm_vmcb_and16(v,o,d)		*(u16*)((ulong_ptr)v+o)&=(u16)d
#define noir_svm_vmcb_and32(v,o,d)		*(u32*)((ulong_ptr)v+o)&=(u32)d
#define noir_svm_vmcb_and64(v,o,d)		*(u32*)((ulong_ptr)v+o)&=(u64)d

#define noir_svm_vmcb_or8(v,o,d)		*(u8*)((ulong_ptr)v+o)|=(u8)d
#define noir_svm_vmcb_or16(v,o,d)		*(u16*)((ulong_ptr)v+o)|=(u16)d
#define noir_svm_vmcb_or32(v,o,d)		*(u32*)((ulong_ptr)v+o)|=(u32)d
#define noir_svm_vmcb_or64(v,o,d)		*(u32*)((ulong_ptr)v+o)|=(u64)d

#define noir_svm_vmcb_xor8(v,o,d)		*(u8*)((ulong_ptr)v+o)^=(u8)d
#define noir_svm_vmcb_xor16(v,o,d)		*(u16*)((ulong_ptr)v+o)^=(u16)d
#define noir_svm_vmcb_xor32(v,o,d)		*(u32*)((ulong_ptr)v+o)^=(u32)d
#define noir_svm_vmcb_xor64(v,o,d)		*(u32*)((ulong_ptr)v+o)^=(u64)d

#define noir_svm_vmcb_bt32(v,o,d)		noir_bt((u32*)((ulong_ptr)v+o),d)
#define noir_svm_vmcb_bts32(v,o,d)		noir_bts((u32*)((ulong_ptr)v+o),d)
#define noir_svm_vmcb_btr32(v,o,d)		noir_btr((u32*)((ulong_ptr)v+o),d)
#define noir_svm_vmcb_btc32(v,o,d)		noir_btc((u32*)((ulong_ptr)v+o),d)

#if defined(_amd64)
#define noir_svm_vmcb_bt64(v,o,d)		noir_bt64((u64*)((ulong_ptr)v+o),d)
#define noir_svm_vmcb_bts64(v,o,d)		noir_bts64((u64*)((ulong_ptr)v+o),d)
#define noir_svm_vmcb_btr64(v,o,d)		noir_btr64((u64*)((ulong_ptr)v+o),d)
#define noir_svm_vmcb_btc64(v,o,d)		noir_btc64((u64*)((ulong_ptr)v+o),d)
#endif

#if defined(_amd64)
#define noir_svm_vmread		noir_svm_vmread64
#define noir_svm_vmwrite	noir_svm_vmwrite64
#else
#define noir_svm_vmread		noir_svm_vmread32
#define noir_svm_vmwrite	noir_svm_vmwrite32
#endif

// Misdefinition can be revised easily by this designation!
// Since we simply define offsets, we won't need to consider alignment issues.
typedef enum _svm_vmcb_offset
{
	// Control Area
	intercept_read_cr=0x0,
	intercept_write_cr=0x2,
	intercept_access_cr=0x0,
	intercept_read_dr=0x4,
	intercept_write_dr=0x6,
	intercept_access_dr=0x4,
	intercept_exceptions=0x8,
	intercept_instruction1=0xC,
	intercept_instruction2=0x10,
	intercept_write_cr_post=0x12,
	intercept_instruction3=0x14,
	pause_filter_threshold=0x3C,
	pause_filter_count=0x3E,
	iopm_physical_address=0x40,
	msrpm_physical_address=0x48,
	tsc_offset=0x50,
	guest_asid=0x58,
	tlb_control=0x5C,
	avic_control=0x60,
	guest_interrupt=0x68,
	exit_code=0x70,
	exit_info1=0x78,
	exit_info2=0x80,
	exit_interrupt_info=0x88,
	npt_control=0x90,
	avic_apic_bar=0x98,
	ghcb_physical_address=0xA0,
	event_injection=0xA8,
	event_error_code=0xAC,
	npt_cr3=0xB0,
	lbr_virtualization_control=0xB8,
	vmcb_clean_bits=0xC0,
	next_rip=0xC8,
	number_of_bytes_fetched=0xD0,
	guest_instruction_bytes=0xD1,
	avic_backing_page_pointer=0xE0,
	avic_logical_table_pointer=0xF0,
	avic_physical_table_pointer=0xF8,
	vmsa_pointer=0x108,
	vmgexit_rax=0x110,
	vmgexit_cpl=0x118,
	// Following offset definitions would be available only if Microsoft enlightenments are enabled.
	enlightenments_control=0x3E0,
	vp_id=0x3E4,
	vm_id=0x3E8,
	partition_assist_page=0x3F0,
	// Following offset definitions would be unusable if SEV-ES is enabled.
	// State Save Area - SEV-ES Disabled
	// ES
	guest_es_selector=0x400,
	guest_es_attrib=0x402,
	guest_es_limit=0x404,
	guest_es_base=0x408,
	// CS
	guest_cs_selector=0x410,
	guest_cs_attrib=0x412,
	guest_cs_limit=0x414,
	guest_cs_base=0x418,
	// SS
	guest_ss_selector=0x420,
	guest_ss_attrib=0x422,
	guest_ss_limit=0x424,
	guest_ss_base=0x428,
	// DS
	guest_ds_selector=0x430,
	guest_ds_attrib=0x432,
	guest_ds_limit=0x434,
	guest_ds_base=0x438,
	// FS
	guest_fs_selector=0x440,
	guest_fs_attrib=0x442,
	guest_fs_limit=0x444,
	guest_fs_base=0x448,
	// GS
	guest_gs_selector=0x450,
	guest_gs_attrib=0x452,
	guest_gs_limit=0x454,
	guest_gs_base=0x458,
	// GDTR
	guest_gdtr_selector=0x460,
	guest_gdtr_attrib=0x462,
	guest_gdtr_limit=0x464,
	guest_gdtr_base=0x468,
	// LDTR
	guest_ldtr_selector=0x470,
	guest_ldtr_attrib=0x472,
	guest_ldtr_limit=0x474,
	guest_ldtr_base=0x478,
	// IDTR
	guest_idtr_selector=0x480,
	guest_idtr_attrib=0x482,
	guest_idtr_limit=0x484,
	guest_idtr_base=0x488,
	// TR
	guest_tr_selector=0x490,
	guest_tr_attrib=0x492,
	guest_tr_limit=0x494,
	guest_tr_base=0x498,
	//
	guest_cpl=0x4CB,
	guest_efer=0x4D0,
	guest_cr4=0x548,
	guest_cr3=0x550,
	guest_cr0=0x558,
	guest_dr7=0x560,
	guest_dr6=0x568,
	guest_rflags=0x570,
	guest_rip=0x578,
	guest_rsp=0x5D8,
	guest_s_cet=0x5E0,
	guest_ssp=0x5E8,
	guest_isst=0x5F0,
	guest_rax=0x5F8,
	guest_star=0x600,
	guest_lstar=0x608,
	guest_cstar=0x610,
	guest_sfmask=0x618,
	guest_kernel_gs_base=0x620,
	guest_sysenter_cs=0x628,
	guest_sysenter_esp=0x630,
	guest_sysenter_eip=0x638,
	guest_cr2=0x640,
	guest_pat=0x668,
	guest_debug_ctrl=0x670,
	guest_last_branch_from=0x678,
	guest_last_branch_to=0x680,
	guest_last_exception_from=0x688,
	guest_last_exception_to=0x690,
	guest_debug_extened_config=0x698,
	guest_spec_ctrl=0x6E0,
	guest_lbr_stack_from=0xA70,
	guest_lbr_stack_to=0xAF0,
	guest_lbr_select=0xB70,
	guest_ibs_fetch_ctrl=0xB78,
	guest_ibs_fetch_linear_address=0xB80,
	guest_ibs_op_ctrl=0xB88,
	guest_ibs_op_rip=0xB90,
	guest_ibs_op_data1=0xB98,
	guest_ibs_op_data2=0xBA0,
	guest_ibs_op_data3=0xBA8,
	guest_ibs_dc_linear_address=0xBB0,
	guest_bp_ibstgt_rip=0xBB8,
	guest_ic_ibs_extd_ctrl=0xBC0
}svm_vmcb_offset,*svm_vmcb_offset_p;

// Following definitions is for State Save Area with SEV-ES Enabled
// You may notice the offset is 0x400 different from corresponding fields.
typedef enum _svm_vmsa_offset
{
	guest_sev_es_segment=0x0,
	guest_sev_cs_segment=0x10,
	guest_sev_ss_segment=0x20,
	guest_sev_ds_segment=0x30,
	guest_sev_fs_segment=0x40,
	guest_sev_gs_segment=0x50,
	guest_sev_gdtr_segment=0x60,
	guest_sev_ldtr_segment=0x70,
	guest_sev_idtr_segment=0x80,
	guest_sev_tr_segment=0x90,
	guest_sev_cpl=0xCB,
	guest_sev_efer=0xCC,
	guest_sev_cr4=0x148,
	guest_sev_cr3=0x150,
	guest_sev_cr0=0x158,
	guest_sev_dr7=0x160,
	guest_sev_dr6=0x168,
	guest_sev_rflags=0x170,
	guest_sev_rip=0x178,
	guest_sev_dr0=0x180,
	guest_sev_dr1=0x188,
	guest_sev_dr2=0x190,
	guest_sev_dr3=0x198,
	guest_sev_dr0_mask=0x1A0,
	guest_sev_dr1_mask=0x1A8,
	guest_sev_dr2_mask=0x1B0,
	guest_sev_dr3_mask=0x1B8,
	guest_sev_rsp=0x1D8,
	guest_sev_rax=0x1F8,
	guest_sev_star=0x200,
	guest_sev_lstar=0x208,
	guest_sev_cstar=0x210,
	guest_sev_sfmask=0x218,
	guest_sev_kernel_gs_base=0x220,
	guest_sev_sysenter_cs=0x228,
	guest_sev_sysenter_esp=0x230,
	guest_sev_sysenter_eip=0x238,
	guest_sev_cr2=0x240,
	guest_sev_pat=0x268,
	guest_sev_debug_ctrl=0x270,
	guest_sev_last_branch_from=0x278,
	guest_sev_last_branch_to=0x280,
	guest_sev_last_exception_from=0x288,
	guest_sev_last_exception_to=0x290,
	guest_sev_pkru=0x2E8,
	guest_sev_tsc_aux=0x2EC,
	guest_sev_tsc_scale=0x2F0,
	guest_sev_tsc_offset=0x2F8,
	guest_sev_reg_prot_nonce=0x300,
	guest_sev_rcx=0x308,
	guest_sev_rdx=0x310,
	guest_sev_rbx=0x318,
	guest_sev_rbp=0x328,
	guest_sev_rsi=0x330,
	guest_sev_rdi=0x338,
	guest_sev_r8=0x340,
	guest_sev_r9=0x348,
	guest_sev_r10=0x350,
	guest_sev_r11=0x358,
	guest_sev_r12=0x360,
	guest_sev_r13=0x368,
	guest_sev_r14=0x370,
	guest_sev_r15=0x378,
	sw_exit_info1=0x390,
	sw_exit_info2=0x398,
	sw_exit_int_info=0x3A0,
	sw_next_rip=0x3A8,
	sev_features=0x3B0,
	sev_vintr_ctrl=0x3B8,
	sev_guest_exitcode=0x3C0,
	sev_virtual_tom=0x3C8,
	sev_tlb_id=0x3D0,
	sev_pcpu_id=0x3D8,
	sev_event_injection=0x3E0,
	guest_sev_xcr0=0x3E8,
	guest_sev_x87dp=0x400,
	guest_sev_mxcsr=0x408,
	guest_sev_x87ftw=0x40C,
	guest_sev_x87fsw=0x40E,
	guest_sev_x87fcw=0x410,
	guest_sev_x87fop=0x412,
	guest_sev_x87ds=0x414,
	guest_sev_x87cs=0x416,
	guest_sev_x87rip=0x418,
	guest_sev_fpreg_x87=0x420,
	guest_sev_fpreg_xmm=0x470,
	guest_sev_fpreg_ymm=0x570
}svm_vmsa_offset,*svm_vmsa_offset_p;

// The following definitions are intended for 
// hypervisor's Host-Save Area nested in NoirVisor.
typedef struct _noir_svm_nested_hsave_area
{
	// Segment Selectors
	u16 cs_sel;
	u16 ds_sel;
	u16 es_sel;
	u16 ss_sel;
	u16 gdtr_limit;
	u16 idtr_limit;
	u64 gdtr_base;
	u64 idtr_base;
	// General-Purpose Registers
	u64 rax;
	u64 rsp;
	u64 rip;
	u64 rflags;
	// Control Registers
	u64 cr0;
	u64 cr3;
	u64 cr4;
	u64 efer;
}noir_svm_nested_hsave_area,*noir_svm_nested_hsave_area_p;