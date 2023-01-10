/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file defines SMM SMRAM structures in NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/noirhvm.h
*/

#include "nvdef.h"

typedef enum _ia32_legacy_smram_state_save_offset
{
	ia32_legacy_rsm_cr0=0xFFFC,
	ia32_legacy_rsm_cr3=0xFFF8,
	ia32_legacy_rsm_eflags=0xFFF4,
	ia32_legacy_rsm_eip=0xFFF0,
	ia32_legacy_rsm_edi=0xFFEC,
	ia32_legacy_rsm_esi=0xFFE8,
	ia32_legacy_rsm_ebp=0xFFE4.
	ia32_legacy_rsm_esp=0xFFE0,
	ia32_legacy_rsm_ebx=0xFFDC,
	ia32_legacy_rsm_edx=0xFFD8,
	ia32_legacy_rsm_ecx=0xFFD4,
	ia32_legacy_rsm_eax=0xFFD0,
	ia32_legacy_rsm_dr6=0xFFCC,
	ia32_legacy_rsm_dr7=0xFFC8,
	ia32_legacy_rsm_tr=0xFFC4,
	ia32_legacy_rsm_gs=0xFFBC,
	ia32_legacy_rsm_fs=0xFFB8,
	ia32_legacy_rsm_ds=0xFFB4,
	ia32_legacy_rsm_ss=0xFFB0,
	ia32_legacy_rsm_cs=0xFFAC,
	ia32_legacy_rsm_es=0xFFA8,
	ia32_legacy_io_state=0xFFA4,
	ia32_legacy_io_memaddr=0xFFA0,
	ia32_legacy_auto_halt_restart=0xFF02,
	ia32_legacy_io_instruction_restart=0xFF00,
	ia32_legacy_smm_revision_id=0xFEFC,
	ia32_legacy_smbase=0xFEF8
}ia32_legacy_smram_state_save_offset,*ia32_legacy_smram_smram_state_save_offset_p;

typedef enum _ia32_smram_state_save_offset
{
	ia32_rsm_cr0=0xFFF8,
	ia32_rsm_cr3=0xFFF0,
	ia32_rsm_rflags=0xFFE8,
	ia32_rsm_efer=0xFFE0,
	ia32_rsm_rip=0xFFD8,
	ia32_rsm_dr6=0xFFD0,
	ia32_rsm_dr7=0xFFC8,
	ia32_rsm_tr=0xFFC4,
	ia32_rsm_ldtr=0xFFC0,
	ia32_rsm_gs=0xFFBC,
	ia32_rsm_fs=0xFFB8,
	ia32_rsm_ds=0xFFB4,
	ia32_rsm_ss=0xFFB0,
	ia32_rsm_cs=0xFFAC,
	ia32_rsm_es=0xFFA8,
	ia32_smm_io_misc=0xFFA4,
	ia32_smm_io_memaddr=0xFF9C,
	ia32_rsm_rdi=0xFF94,
	ia32_rsm_rsi=0xFF8C,
	ia32_rsm_rbp=0xFF84,
	ia32_rsm_rsp=0xFF7C,
	ia32_rsm_rbx=0xFF74,
	ia32_rsm_rdx=0xFF6C,
	ia32_rsm_rcx=0xFF64,
	ia32_rsm_rax=0xFF5C,
	ia32_rsm_r8=0xFF54,
	ia32_rsm_r9=0xFF4C,
	ia32_rsm_r10=0xFF44,
	ia32_rsm_r11=0xFF3C,
	ia32_rsm_r12=0xFF34,
	ia32_rsm_r13=0xFF2C,
	ia32_rsm_r14=0xFF24,
	ia32_rsm_r15=0xFF1C,
	ia32_smm_auto_halt_restart=0xFF02,
	ia32_smm_io_instruction_restart=0xFF00,
	ia32_smm_revision_id=0xFEFC,
	ia32_smm_smbase=0xFEF8,
	ia32_rsm_enable_ept=0xFEE0,
	ia32_rsm_eptp=0xFED8,
	ia32_rsm_ssp=0xFEC8,
	ia32_rsm_ldt_base_low=0xFE9C,
	ia32_rsm_idt_base_low=0xFE94,
	ia32_rsm_gdt_base_low=0xFE8C,
	ia32_rsm_cr4=0xFE40,
	ia32_smm_io_rip=0xFDE8,
	ia32_rsm_idt_base_high=0xFDD8,
	ia32_rsm_ldt_base_high=0xFDD4,
	ia32_rsm_gdt_base_high=0xFDD0
}ia32_smram_state_save_offset,*ia32_smram_state_save_offset_p;

typedef union _ia32_smram_state_io_misc
{
	struct
	{
		u32 io_smi:1;
		u32 io_length:3;
		u32 type:1;
		u32 string:1;
		u32 repeat:1;
		u32 immediate_port:1;
		u32 reserved:8;
		u32 io_port:16;
	};
	u32 value;
}ia32_smram_state_io_misc,*ia32_smram_state_io_misc_p;

typedef enum _amd64_legacy_smram_state_save_offset
{
	amd64_legacy_smbase=0xFEF8,
	amd64_legacy_smm_revision_id=0xFEFC,
	amd64_legacy_io_instruction_restart=0xFF00,
	amd64_legacy_auto_halt_restart=0xFF02,
	amd64_legacy_rsm_gdt_base=0xFF88,
	amd64_legacy_rsm_idt_base=0xFF94,
	amd64_legacy_rsm_es=0xFFA8,
	amd64_legacy_rsm_cs=0xFFAC,
	amd64_legacy_rsm_ss=0xFFB0,
	amd64_legacy_rsm_ds=0xFFB4,
	amd64_legacy_rsm_fs=0xFFB8,
	amd64_legacy_rsm_gs=0xFFBC,
	amd64_legacy_rsm_ldt_base=0xFFC0,
	amd64_legacy_rsm_tr=0xFFC4,
	amd64_legacy_rsm_dr7=0xFFC8,
	amd64_legacy_rsm_dr6=0xFFCC,
	amd64_legacy_rsm_eax=0xFFD0,
	amd64_legacy_rsm_ecx=0xFFD4,
	amd64_legacy_rsm_edx=0xFFD8,
	amd64_legacy_rsm_ebx=0xFFDC,
	amd64_legacy_rsm_esp=0xFFE0,
	amd64_legacy_rsm_ebp=0xFFE4,
	amd64_legacy_rsm_esi=0xFFE8,
	amd64_legacy_rsm_edi=0xFFEC,
	amd64_legacy_rsm_eip=0xFFF0,
	amd64_legacy_rsm_eflags=0xFFF4,
	amd64_legacy_rsm_cr3=0xFFF8,
	amd64_legacy_rsm_cr0=0xFFFC
}amd64_legacy_smram_state_save_offset,*amd64_legacy_smram_state_save_offset_p;

typedef enum _amd64_smram_state_save_offset
{
	amd64_rsm_es_selector=0xFE00,
	amd64_rsm_es_attributes=0xFE02,
	amd64_rsm_es_limit=0xFE04,
	amd64_rsm_es_base=0xFE08,
	amd64_rsm_cs_selector=0xFE10,
	amd64_rsm_cs_attributes=0xFE12,
	amd64_rsm_cs_limit=0xFE14,
	amd64_rsm_cs_base=0xFE18,
	amd64_rsm_ss_selector=0xFE20,
	amd64_rsm_ss_attributes=0xFE22,
	amd64_rsm_ss_limit=0xFE24,
	amd64_rsm_ss_base=0xFE28,
	amd64_rsm_ds_selector=0xFE30,
	amd64_rsm_ds_attributes=0xFE32,
	amd64_rsm_ds_limit=0xFE34,
	amd64_rsm_ds_base=0xFE38,
	amd64_rsm_fs_selector=0xFE40,
	amd64_rsm_fs_attributes=0xFE42,
	amd64_rsm_fs_limit=0xFE44,
	amd64_rsm_fs_base=0xFE48,
	amd64_rsm_gs_selector=0xFE50,
	amd64_rsm_gs_attributes=0xFE52,
	amd64_rsm_gs_limit=0xFE54,
	amd64_rsm_gs_base=0xFE58,
	amd64_rsm_gdtr_limit=0xFE64,
	amd64_rsm_gdtr_base=0xFE68,
	amd64_rsm_ldtr_selector=0xFE70,
	amd64_rsm_ldtr_attributes=0xFE72,
	amd64_rsm_ldtr_limit=0xFE74,
	amd64_rsm_ldtr_base=0xFE78,
	amd64_rsm_idtr_limit=0xFE84,
	amd64_rsm_idtr_base=0xFE88,
	amd64_rsm_tr_selector=0xFE90,
	amd64_rsm_tr_attributes=0xFE92,
	amd64_rsm_tr_limit=0xFE94,
	amd64_rsm_tr_base=0xFE98,
	amd64_smm_io_rip=0xFEA0,
	amd64_smm_io_rcx=0xFEA8,
	amd64_smm_io_rsi=0xFEB0,
	amd64_smm_io_rdi=0xFEB8,
	amd64_smm_io_instruction_restart_dword=0xFEC0,
	amd64_smm_io_instruction_restart=0xFEC8
	amd64_smm_io_auto_halt_restart=0xFEC9,
	amd64_rsm_efer=0xFED0,
	amd64_rsm_svm_guest=0xFED8,
	amd64_rsm_svm_guest_vmcb=0xFEE0,
	amd64_rsm_svm_guest_vintr=0xFEE8,
	amd64_smm_revision_id=0xFEFC,
	amd64_smm_smbase=0xFF00,
	amd64_rsm_ssp=0xFF18,
	amd64_rsm_svm_guest_pat=0xFF20,
	amd64_rsm_svm_host_efer=0xFF28,
	amd64_rsm_svm_host_cr4=0xFF30,
	amd64_rsm_svm_host_cr3=0xFF38,
	amd64_rsm_svm_host_cr0=0xFF40,
	amd64_rsm_cr4=0xFF48,
	amd64_rsm_cr3=0xFF50,
	amd64_rsm_cr0=0xFF58,
	amd64_rsm_dr7=0xFF60,
	amd64_rsm_dr6=0xFF68,
	amd64_rsm_rflags=0xFF70,
	amd64_rsm_rip=0xFF78,
	amd64_rsm_r15=0xFF80,
	amd64_rsm_r14=0xFF88,
	amd64_rsm_r13=0xFF90,
	amd64_rsm_r12=0xFF98,
	amd64_rsm_r11=0xFFA0,
	amd64_rsm_r10=0xFFA8,
	amd64_rsm_r9=0xFFB0,
	amd64_rsm_r8=0xFFB8,
	amd64_rsm_rdi=0xFFC0,
	amd64_rsm_rsi=0xFFC8,
	amd64_rsm_rbp=0xFFD0,
	amd64_rsm_rsp=0xFFD8,
	amd64_rsm_rbx=0xFFE0,
	amd64_rsm_rdx=0xFFE8,
	amd64_rsm_rcx=0xFFF0,
	amd64_rsm_rax=0xFFF8
}amd64_smram_state_save_offset,*amd64_smram_state_save_offset_p;

typedef union _amd64_smram_state_io_restart
{
	u32 type:1;
	u32 valid:1;
	u32 string:1;
	u32 repeat:1;
	u32 io_size:3;
	u32 addr_size:3;
	u32 intercepted_io_port:1;
	u32 tf:1;
	u32 b0:1;
	u32 b1:1;
	u32 b2:1;
	u32 b3:1;
	u32 port:16;
}amd64_smram_state_io_restart,*amd64_smram_state_io_restart_p;