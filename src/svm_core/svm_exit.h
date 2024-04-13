/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file is the basic Exit Handler of SVM Driver.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_exit.h
*/

#include <nvdef.h>

#define intercepted_cr_read(x)		x
#define intercepted_cr_write(x)		0x10+x
#define intercepted_dr_read(x)		0x20+x
#define intercepted_dr_write(x)		0x30+x
#define intercepted_exception(x)	0x40+x
#define intercepted_interrupt		0x60
#define intercepted_nmi				0x61
#define intercepted_smi				0x62
#define intercepted_init			0x63
#define intercepted_vintr			0x64
#define intercepted_cr0_nottsmp		0x65
#define intercepted_sidt			0x66
#define intercepted_sgdt			0x67
#define intercepted_sldt			0x68
#define intercepted_str				0x69
#define intercepted_lidt			0x6A
#define intercepted_lgdt			0x6B
#define intercepted_lldt			0x6C
#define intercepted_ltr				0x6D
#define intercepted_rdtsc			0x6E
#define intercepted_rdpmc			0x6F
#define intercepted_pushf			0x70
#define intercepted_popf			0x71
#define intercepted_cpuid			0x72
#define intercepted_rsm				0x73
#define intercepted_iret			0x74
#define intercepted_int				0x75
#define intercepted_invd			0x76
#define intercepted_pause			0x77
#define intercepted_hlt				0x78
#define intercepted_invlpg			0x79
#define intercepted_invlpga			0x7A
#define intercepted_io				0x7B
#define intercepted_msr				0x7C
#define intercepted_task_switch		0x7D
#define intercepted_ferr_freeze		0x7E
#define intercepted_shutdown		0x7F
#define intercepted_vmrun			0x80
#define intercepted_vmmcall			0x81
#define intercepted_vmload			0x82
#define intercepted_vmsave			0x83
#define intercepted_stgi			0x84
#define intercepted_clgi			0x85
#define intercepted_skinit			0x86
#define intercepted_rdtscp			0x87
#define intercepted_icebp			0x88
#define intercepted_wbinvd			0x89
#define intercepted_monitor			0x8A
#define intercepted_mwait			0x8B
#define intercepted_mwait_cond		0x8C
#define intercepted_xsetbv			0x8D
#define intercepted_rdpru			0x8E
#define intercepted_efer_w_trap		0x8F
#define intercepted_cr_w_trap(x)	0x90+x
#define intercepted_invlpgb			0xA0
#define illegal_invlpgb				0xA2
#define intercepted_mcommit			0xA3
#define intercepted_tlbsync			0xA4
#define nested_page_fault			0x400
#define avic_incomplete_ipi			0x401
#define avic_no_acceleration		0x402
#define intercepted_vmgexit			0x403

#define invalid_guest_state			-1
#define intercepted_vmsa_busy		-2
#define idle_required				-3

#define noir_svm_maximum_code1		0xA5
#define noir_svm_maximum_code2		0x4
#define noir_svm_maximum_negative	3

#define amd64_external_virtual_interrupt	0
#define amd64_non_maskable_interrupt		2
#define amd64_fault_trap_exception			3
#define amd64_software_interrupt			4

typedef void (fastcall *noir_svm_exit_handler_routine)
(
 noir_gpr_state_p gpr_state,
 noir_svm_vcpu_p vcpu
);

typedef void (fastcall *noir_svm_cpuid_exit_handler)
(
 u32 leaf,
 u32 subleaf,
 noir_cpuid_general_info_p info
);

typedef void (fastcall *noir_svm_cvexit_handler_routine)
(
 noir_gpr_state_p gpr_state,
 noir_svm_vcpu_p vcpu,
 noir_svm_custom_vcpu_p cvcpu
);

typedef void (fastcall *noir_svm_nvexit_handler_routine)
(
 noir_gpr_state_p gpr_state,
 noir_svm_vcpu_p vcpu,
 noir_svm_nested_vcpu_node_p nvcpu
);

typedef void (fastcall *noir_svm_decoder_handler_routine)
(
 noir_gpr_state_p gpr_state,
 noir_svm_vcpu_p vcpu,
 noir_svm_custom_vcpu_p cvcpu
);

typedef union _amd64_event_injection
{
	struct
	{
		u64 vector:8;		// Bits	0-7
		u64 type:3;			// Bits	8-10
		u64 error_valid:1;	// Bit	11
		u64 reserved:19;	// Bits	12-30
		u64 valid:1;		// Bit	31
		u64 error_code:32;	// Bits	32-63
	};
	u64 value;
}amd64_event_injection,*amd64_event_injection_p;

#if defined(_svm_exit)
void static fastcall nvc_svm_default_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_invalid_guest_state(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_cr4_write_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_db_exception_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_pf_exception_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_mc_exception_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_sx_exception_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_exception_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_nmi_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_sidt_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_sgdt_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_sldt_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_str_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_cpuid_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_invlpga_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_io_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_msr_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_shutdown_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_vmrun_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_vmmcall_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_vmload_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_vmsave_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_stgi_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_clgi_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_skinit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);
void static fastcall nvc_svm_nested_pf_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu);

noir_hvdata noir_svm_exit_handler_routine svm_exit_handler_group1[noir_svm_maximum_code1]=
{
	// 16 Control-Register Read Exit Handler...
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	// 16 Control-Register Write Exit Handler...
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	nvc_svm_cr4_write_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	// 16 Debug-Register Read Exit Handler...
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	// 16 Debug-Register Write Exit Handler...
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	// 32 Exception Exit Handler...
	nvc_svm_exception_handler,		// Exception Vector 0
	nvc_svm_db_exception_handler,	// Debug Exception
	nvc_svm_exception_handler,nvc_svm_exception_handler,
	nvc_svm_exception_handler,nvc_svm_exception_handler,nvc_svm_exception_handler,nvc_svm_exception_handler,
	nvc_svm_exception_handler,nvc_svm_exception_handler,nvc_svm_exception_handler,nvc_svm_exception_handler,
	nvc_svm_exception_handler,nvc_svm_exception_handler,
	nvc_svm_pf_exception_handler,	// Page-Fault Exception
	nvc_svm_exception_handler,		// Exception Vector 15
	nvc_svm_exception_handler,nvc_svm_exception_handler,
	nvc_svm_mc_exception_handler,	// Machine-Check Exception
	nvc_svm_exception_handler,		// Exception Vector 19
	nvc_svm_exception_handler,nvc_svm_exception_handler,nvc_svm_exception_handler,nvc_svm_exception_handler,
	nvc_svm_exception_handler,nvc_svm_exception_handler,nvc_svm_exception_handler,nvc_svm_exception_handler,
	nvc_svm_exception_handler,nvc_svm_exception_handler,
	nvc_svm_sx_exception_handler,	// Security Exception
	nvc_svm_exception_handler,		// Exception Vector 31
	// 32 Interception Handlers from Vector 1
	nvc_svm_default_handler,		// Physical External Interrupt
	nvc_svm_nmi_handler,			// Physical Non-Maskable Interrupt
	nvc_svm_default_handler,		// Physical System Management Interrupt
	nvc_svm_default_handler,		// Physical INIT Signal
	nvc_svm_default_handler,		// Virtual Interrupt
	nvc_svm_default_handler,		// CR0 Selective Write
	nvc_svm_sidt_handler,			// sidt Instruction
	nvc_svm_sgdt_handler,			// sgdt Instruction
	nvc_svm_sldt_handler,			// sldt Instruction
	nvc_svm_str_handler,			// str Instruction
	nvc_svm_default_handler,		// lidt Instruction
	nvc_svm_default_handler,		// lgdt Instruction
	nvc_svm_default_handler,		// lldt Instruction
	nvc_svm_default_handler,		// ltr Instruction
	nvc_svm_default_handler,		// rdtsc Instruction
	nvc_svm_default_handler,		// rdpmc Instruction
	nvc_svm_default_handler,		// pushf Instruction
	nvc_svm_default_handler,		// popf Instruction
	nvc_svm_cpuid_handler,			// cpuid Instruction
	nvc_svm_default_handler,		// rsm Instruction
	nvc_svm_default_handler,		// iret Instruction
	nvc_svm_default_handler,		// int Instruction
	nvc_svm_default_handler,		// invd Instruction
	nvc_svm_default_handler,		// pause Instruction	
	nvc_svm_default_handler,		// hlt Instruction
	nvc_svm_default_handler,		// invlpg Instruction
	nvc_svm_invlpga_handler,		// invlpga Instruction
	nvc_svm_io_handler,				// in/out Instruction
	nvc_svm_msr_handler,			// rdmsr/wrmsr Instruction
	nvc_svm_default_handler,		// Task Switch
	nvc_svm_default_handler,		// FP Error Frozen
	nvc_svm_shutdown_handler,		// Shutdown (Triple-Fault)
	// 16 Interception Handlers from Vector 2
	nvc_svm_vmrun_handler,			// vmrun Instruction
	nvc_svm_vmmcall_handler,		// vmmcall Instruction
	nvc_svm_vmload_handler,			// vmload Instruction
	nvc_svm_vmsave_handler,			// vmsave Instruction
	nvc_svm_stgi_handler,			// stgi Instruction
	nvc_svm_clgi_handler,			// clgi Instruction
	nvc_svm_skinit_handler,			// skinit Instruction
	nvc_svm_default_handler,		// rdtscp Instruction
	nvc_svm_default_handler,		// icebp (int1) Instruction
	nvc_svm_default_handler,		// wb(no)invd Instruction
	nvc_svm_default_handler,		// monitor(x) Instruction
	nvc_svm_default_handler,		// mwait(x) Instruction
	nvc_svm_default_handler,		// mwait(x) Instruction if Armed
	nvc_svm_default_handler,		// rdpru Instruction
	nvc_svm_default_handler,		// xsetbv Instruction
	nvc_svm_default_handler,		// Post EFER MSR Write Trap
	// 16 Control-Register Post-Write Exit Handler
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,nvc_svm_default_handler,
	// 5 Interception Handlers from Vector 3
	nvc_svm_default_handler,		// invlpgb Instruction
	nvc_svm_default_handler,		// Illegal invlpgb Instruction
	nvc_svm_default_handler,		// invpcid Instruction
	nvc_svm_default_handler,		// mcommit Instruction
	nvc_svm_default_handler			// tlbsync Instruction
};

noir_hvdata noir_svm_exit_handler_routine svm_exit_handler_group2[noir_svm_maximum_code2]=
{
	nvc_svm_nested_pf_handler,		// Nested Page Fault
	nvc_svm_default_handler,		// AVIC Incomplete Virtual IPI Delivery
	nvc_svm_default_handler,		// Virtual APIC Access Unhandled by AVIC Hardware
	nvc_svm_default_handler			// vmgexit Instruction
};

noir_hvdata noir_svm_exit_handler_routine svm_exit_handler_negative[noir_svm_maximum_negative]=
{
	nvc_svm_invalid_guest_state,	// Invalid Guest State
	nvc_svm_default_handler,		// VMSA Busy
	nvc_svm_default_handler			// Idle state is required
};

noir_hvdata noir_svm_exit_handler_routine* svm_exit_handlers[4]=
{
	svm_exit_handler_group1,
	svm_exit_handler_group2,
	null,null
};

noir_hvdata noir_svm_cpuid_exit_handler nvcp_svm_cpuid_handler=null;

extern noir_svm_cvexit_handler_routine* svm_cvexit_handlers[];
extern noir_svm_cvexit_handler_routine svm_cvexit_handler_negative[];
extern noir_svm_nvexit_handler_routine* svm_nvexit_handlers[];
extern noir_svm_nvexit_handler_routine svm_nvexit_handler_negative[];
extern noir_svm_decoder_handler_routine* svm_decoder_handlers[];
extern noir_svm_decoder_handler_routine svm_decoder_handler_negative[];
#elif defined(_svm_cvexit)
void static fastcall nvc_svm_default_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_invalid_state_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_cr4_read_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_cr4_write_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_cr_access_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_dr_access_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_exception_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_mc_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_sx_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_extint_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_nmi_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_smi_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_virtint_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_cpuid_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_iret_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_invd_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_hlt_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_invlpga_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_io_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_msr_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_shutdown_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_vmrun_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_vmmcall_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_vmload_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_vmsave_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_stgi_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_clgi_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_skinit_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_nested_pf_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_incomplete_ipi_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static fastcall nvc_svm_unaccelerated_avic_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);

noir_hvdata noir_svm_cvexit_handler_routine svm_cvexit_handler_group1[noir_svm_maximum_code1]=
{
	// 16 Control-Register Read & Write Handlers.
	nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,
	nvc_svm_cr4_read_cvexit_handler,nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,
	nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,
	nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,
	nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,
	nvc_svm_cr4_write_cvexit_handler,nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,
	nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,
	nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,nvc_svm_cr_access_cvexit_handler,
	// 16 Debug-Register Read & Write Handlers.
	nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,
	nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,
	nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,
	nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,
	nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,
	nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,
	nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,
	nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,nvc_svm_dr_access_cvexit_handler,
	// 32 Exception Handlers.
	nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,
	nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,
	nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,
	nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,
	nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,nvc_svm_mc_cvexit_handler,nvc_svm_exception_cvexit_handler,
	nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,
	nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,
	nvc_svm_exception_cvexit_handler,nvc_svm_exception_cvexit_handler,nvc_svm_sx_cvexit_handler,nvc_svm_exception_cvexit_handler,
	// 32 Interception Handlers from Vector 1.
	nvc_svm_extint_cvexit_handler,		// Physical External Interrupt
	nvc_svm_nmi_cvexit_handler,			// Physical Non-Maskable Interrupt
	nvc_svm_smi_cvexit_handler,			// Physical System Management Interrupt
	nvc_svm_default_cvexit_handler,		// Physical INIT Signal
	nvc_svm_virtint_cvexit_handler,		// Virtual Interrupt
	nvc_svm_default_cvexit_handler,		// CR0 Selective Write
	nvc_svm_default_cvexit_handler,		// sidt Instruction
	nvc_svm_default_cvexit_handler,		// sgdt Instruction
	nvc_svm_default_cvexit_handler,		// sldt Instruction
	nvc_svm_default_cvexit_handler,		// str Instruction
	nvc_svm_default_cvexit_handler,		// lidt Instruction
	nvc_svm_default_cvexit_handler,		// lgdt Instruction
	nvc_svm_default_cvexit_handler,		// lldt Instruction
	nvc_svm_default_cvexit_handler,		// ltr Instruction
	nvc_svm_default_cvexit_handler,		// rdtsc Instruction
	nvc_svm_default_cvexit_handler,		// rdpmc Instruction
	nvc_svm_default_cvexit_handler,		// pushf Instruction
	nvc_svm_default_cvexit_handler,		// popf Instruction
	nvc_svm_cpuid_cvexit_handler,		// cpuid Instruction
	nvc_svm_default_cvexit_handler,		// rsm Instruction
	nvc_svm_iret_cvexit_handler,		// iret Instruction
	nvc_svm_default_cvexit_handler,		// int Instruction
	nvc_svm_invd_cvexit_handler,		// invd Instruction
	nvc_svm_default_cvexit_handler,		// pause Instruction	
	nvc_svm_hlt_cvexit_handler,			// hlt Instruction
	nvc_svm_default_cvexit_handler,		// invlpg Instruction
	nvc_svm_invlpga_cvexit_handler,		// invlpga Instruction
	nvc_svm_io_cvexit_handler,			// in/out Instruction
	nvc_svm_msr_cvexit_handler,			// rdmsr/wrmsr Instruction
	nvc_svm_default_cvexit_handler,		// Task Switch
	nvc_svm_default_cvexit_handler,		// FP Error Frozen
	nvc_svm_shutdown_cvexit_handler,	// Shutdown (Triple-Fault)
	// 16 Interception Handlers from Vector 2
	nvc_svm_vmrun_cvexit_handler,		// vmrun Instruction
	nvc_svm_vmmcall_cvexit_handler,		// vmmcall Instruction
	nvc_svm_vmload_cvexit_handler,		// vmload Instruction
	nvc_svm_vmsave_cvexit_handler,		// vmsave Instruction
	nvc_svm_stgi_cvexit_handler,		// stgi Instruction
	nvc_svm_clgi_cvexit_handler,		// clgi Instruction
	nvc_svm_skinit_cvexit_handler,		// skinit Instruction
	nvc_svm_default_cvexit_handler,		// rdtscp Instruction
	nvc_svm_default_cvexit_handler,		// icebp (int1) Instruction
	nvc_svm_default_cvexit_handler,		// wb(no)invd Instruction
	nvc_svm_default_cvexit_handler,		// monitor(x) Instruction
	nvc_svm_default_cvexit_handler,		// mwait(x) Instruction
	nvc_svm_default_cvexit_handler,		// mwait(x) Instruction if Armed
	nvc_svm_default_cvexit_handler,		// rdpru Instruction
	nvc_svm_default_cvexit_handler,		// xsetbv Instruction
	nvc_svm_default_cvexit_handler,		// Post EFER MSR Write Trap
	// 16 Control-Register Post-Write Exit Handler
	nvc_svm_default_cvexit_handler,nvc_svm_default_cvexit_handler,nvc_svm_default_cvexit_handler,nvc_svm_default_cvexit_handler,
	nvc_svm_default_cvexit_handler,nvc_svm_default_cvexit_handler,nvc_svm_default_cvexit_handler,nvc_svm_default_cvexit_handler,
	nvc_svm_default_cvexit_handler,nvc_svm_default_cvexit_handler,nvc_svm_default_cvexit_handler,nvc_svm_default_cvexit_handler,
	nvc_svm_default_cvexit_handler,nvc_svm_default_cvexit_handler,nvc_svm_default_cvexit_handler,nvc_svm_default_cvexit_handler,
	// 5 Interception Handlers from Vector 3
	nvc_svm_default_cvexit_handler,		// invlpgb Instruction
	nvc_svm_default_cvexit_handler,		// Illegal invlpgb Instruction
	nvc_svm_default_cvexit_handler,		// invpcid Instruction
	nvc_svm_default_cvexit_handler,		// mcommit Instruction
	nvc_svm_default_cvexit_handler		// tlbsync Instruction
};

noir_hvdata noir_svm_cvexit_handler_routine svm_cvexit_handler_group2[noir_svm_maximum_code2]=
{
	nvc_svm_nested_pf_cvexit_handler,				// Nested Page Fault
	nvc_svm_incomplete_ipi_cvexit_handler,			// AVIC Incomplete Virtual IPI Delivery
	nvc_svm_unaccelerated_avic_cvexit_handler,		// Virtual APIC Access Unhandled by AVIC Hardware
	nvc_svm_default_cvexit_handler					// vmgexit Instruction
};

noir_hvdata noir_svm_cvexit_handler_routine svm_cvexit_handler_negative[noir_svm_maximum_negative]=
{
	nvc_svm_invalid_state_cvexit_handler,	// Invalid Guest State
	nvc_svm_default_cvexit_handler			// VMSA Busy
};

noir_hvdata noir_svm_cvexit_handler_routine* svm_cvexit_handlers[4]=
{
	svm_cvexit_handler_group1,
	svm_cvexit_handler_group2,
	null,null
};

bool nvc_svm_rdmsr_nsvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
bool nvc_svm_wrmsr_nsvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void nvc_svm_nsv_load_nae_synthetic_msr_state(noir_svm_custom_vcpu_p cvcpu);
#elif defined(_svm_nvcpu)
void static fastcall nvc_svm_nvexit_default_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_nested_vcpu_node_p nvcpu);
void static fastcall nvc_svm_nvexit_cpuid_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_nested_vcpu_node_p nvcpu);
void static fastcall nvc_svm_nvexit_shutdown_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_nested_vcpu_node_p nvcpu);

noir_hvdata noir_svm_nvexit_handler_routine svm_nvexit_handler_group1[noir_svm_maximum_code1]=
{
	// 16 Control-Register Read & Write Handlers.
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	// 16 Debug-Register Read & Write Handlers.
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	// 32 Exception Handlers.
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	// 32 Interceptions Handlers for Vector 1.
	nvc_svm_nvexit_default_handler,			// Physical Interrupts
	nvc_svm_nvexit_default_handler,			// Non-Maskable Interrupts
	nvc_svm_nvexit_default_handler,			// System Management Interrupts
	nvc_svm_nvexit_default_handler,			// INIT Signals
	nvc_svm_nvexit_default_handler,			// Virtual Interrupts
	nvc_svm_nvexit_default_handler,			// Write to CR0 other than TS/MP bits.
	nvc_svm_nvexit_default_handler,			// sidt Instruction
	nvc_svm_nvexit_default_handler,			// sgdt Instruction
	nvc_svm_nvexit_default_handler,			// sldt Instruction
	nvc_svm_nvexit_default_handler,			// str Instruction
	nvc_svm_nvexit_default_handler,			// lidt Instruction
	nvc_svm_nvexit_default_handler,			// lgdt Instruction
	nvc_svm_nvexit_default_handler,			// lldt Instruction
	nvc_svm_nvexit_default_handler,			// ltr Instruction
	nvc_svm_nvexit_default_handler,			// rdtsc Instruction
	nvc_svm_nvexit_default_handler,			// rdpmc Instruction
	nvc_svm_nvexit_default_handler,			// pushf Instruction
	nvc_svm_nvexit_default_handler,			// popf Instruction
	nvc_svm_nvexit_cpuid_handler,			// cpuid Instruction
	nvc_svm_nvexit_default_handler,			// rsm Instruction
	nvc_svm_nvexit_default_handler,			// iret Instruction
	nvc_svm_nvexit_default_handler,			// int Instruction
	nvc_svm_nvexit_default_handler,			// invd Instruction
	nvc_svm_nvexit_default_handler,			// pause Instruction
	nvc_svm_nvexit_default_handler,			// hlt Instruction
	nvc_svm_nvexit_default_handler,			// invlpg Instruction
	nvc_svm_nvexit_default_handler,			// invlpga Instruction
	nvc_svm_nvexit_default_handler,			// I/O Instruction
	nvc_svm_nvexit_default_handler,			// MSR Instruction
	nvc_svm_nvexit_default_handler,			// Task Switches
	nvc_svm_nvexit_default_handler,			// FP Error Freezing
	nvc_svm_nvexit_shutdown_handler,		// Shutdown Condition
	// 16 Interception Handlers for Vector 2.
	nvc_svm_nvexit_default_handler,			// vmrun Instruction
	nvc_svm_nvexit_default_handler,			// vmmcall Instruction
	nvc_svm_nvexit_default_handler,			// vmload Instruction
	nvc_svm_nvexit_default_handler,			// vmsave Instruction
	nvc_svm_nvexit_default_handler,			// stgi Instruction
	nvc_svm_nvexit_default_handler,			// clgi Instruction
	nvc_svm_nvexit_default_handler,			// skinit Instruction
	nvc_svm_nvexit_default_handler,			// rdtscp Instruction
	nvc_svm_nvexit_default_handler,			// icebp Instruction
	nvc_svm_nvexit_default_handler,			// wb(no)invd Instruction
	nvc_svm_nvexit_default_handler,			// monitor(x) Instruction
	nvc_svm_nvexit_default_handler,			// mwait(x) Instruction
	nvc_svm_nvexit_default_handler,			// Conditional mwait(x) Instruction
	nvc_svm_nvexit_default_handler,			// rdpru Instruction
	nvc_svm_nvexit_default_handler,			// xsetbv Instruction
	nvc_svm_nvexit_default_handler,			// wrmsr EFER Trap
	// 16 Control Register Write Trap Handlers.
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,nvc_svm_nvexit_default_handler,
	// 5 Interception Handlers for Vector 3.
	nvc_svm_nvexit_default_handler,			// invlpgb Instruction
	nvc_svm_nvexit_default_handler,			// Illegal invlpgb Instruction
	nvc_svm_nvexit_default_handler,			// invpcid Instruction
	nvc_svm_nvexit_default_handler,			// mcommit Instruction
	nvc_svm_nvexit_default_handler,			// tlbsync Instruction
};

noir_hvdata noir_svm_nvexit_handler_routine svm_nvexit_handler_group2[noir_svm_maximum_code2]=
{
	nvc_svm_nvexit_default_handler,					// Nested Page Fault
	nvc_svm_nvexit_default_handler,					// AVIC Incomplete Virtual IPI Delivery
	nvc_svm_nvexit_default_handler,					// Virtual APIC Access Unhandled by AVIC Hardware
	nvc_svm_nvexit_default_handler					// vmgexit Instruction
};

noir_hvdata noir_svm_nvexit_handler_routine svm_nvexit_handler_negative[noir_svm_maximum_negative]=
{
	nvc_svm_nvexit_default_handler,			// Invalid State
	nvc_svm_nvexit_default_handler			// VMSA is busy
};

noir_hvdata noir_svm_nvexit_handler_routine* svm_nvexit_handlers[4]=
{
	svm_nvexit_handler_group1,
	svm_nvexit_handler_group2,
	null,null
};
#elif defined(_svm_decode)
void static nvc_svm_decoder_instruction_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static nvc_svm_decoder_event_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static nvc_svm_decoder_cr_access_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static nvc_svm_decoder_dr_access_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static nvc_svm_decoder_pf_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static nvc_svm_decoder_int_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static nvc_svm_decoder_invlpg_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static nvc_svm_decoder_io_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);
void static nvc_svm_decoder_npf_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu);

noir_hvdata noir_svm_decoder_handler_routine svm_decoder_group1[noir_svm_maximum_code1]=
{
	// Decodings of Control-Register Accesses are assisted by the processor.
	nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,
	nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,
	nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,
	nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,
	nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,
	nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,
	nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,
	nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,nvc_svm_decoder_cr_access_handler,
	// Decodings of Debug-Register Accesses are assisted by the processor.
	nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,
	nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,
	nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,
	nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,
	nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,
	nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,
	nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,
	nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,nvc_svm_decoder_dr_access_handler,
	// Decodings of Exceptions, except #PF, are not assisted by the processor.
	nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,
	nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,
	nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,
	nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_pf_handler,nvc_svm_decoder_event_handler,
	nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,
	nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,
	nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,
	nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,
	// Interception Vector 1
	nvc_svm_decoder_event_handler,			// Physical Interrupts
	nvc_svm_decoder_event_handler,			// Non-Maskable Interrupts
	nvc_svm_decoder_event_handler,			// System-Management Interrupts
	nvc_svm_decoder_event_handler,			// INIT Signals
	nvc_svm_decoder_event_handler,			// Virtual Interrupts
	nvc_svm_decoder_instruction_handler,	// Write-to-CR0 other than TS/MP bits
	nvc_svm_decoder_instruction_handler,	// sidt
	nvc_svm_decoder_instruction_handler,	// sgdt
	nvc_svm_decoder_instruction_handler,	// sldt
	nvc_svm_decoder_instruction_handler,	// str
	nvc_svm_decoder_instruction_handler,	// lidt
	nvc_svm_decoder_instruction_handler,	// lgdt
	nvc_svm_decoder_instruction_handler,	// lldt
	nvc_svm_decoder_instruction_handler,	// ltr
	nvc_svm_decoder_instruction_handler,	// rdtsc
	nvc_svm_decoder_instruction_handler,	// rdpmc
	nvc_svm_decoder_instruction_handler,	// pushf
	nvc_svm_decoder_instruction_handler,	// popf
	nvc_svm_decoder_instruction_handler,	// cpuid
	nvc_svm_decoder_instruction_handler,	// rsm
	nvc_svm_decoder_instruction_handler,	// iret
	nvc_svm_decoder_int_handler,			// int
	nvc_svm_decoder_instruction_handler,	// invd
	nvc_svm_decoder_instruction_handler,	// pause
	nvc_svm_decoder_instruction_handler,	// hlt
	nvc_svm_decoder_invlpg_handler,			// invlpg
	nvc_svm_decoder_instruction_handler,	// invlpga
	nvc_svm_decoder_io_handler,				// I/O instructions
	nvc_svm_decoder_instruction_handler,	// MSR instructions
	nvc_svm_decoder_instruction_handler,	// Task Switches
	nvc_svm_decoder_event_handler,			// FP-Error Freezing
	nvc_svm_decoder_event_handler,			// Shutdown Condition
	// Interception Vector 2
	nvc_svm_decoder_instruction_handler,	// vmrun
	nvc_svm_decoder_instruction_handler,	// vmmcall
	nvc_svm_decoder_instruction_handler,	// vmload
	nvc_svm_decoder_instruction_handler,	// vmsave
	nvc_svm_decoder_instruction_handler,	// stgi
	nvc_svm_decoder_instruction_handler,	// clgi
	nvc_svm_decoder_instruction_handler,	// skinit
	nvc_svm_decoder_instruction_handler,	// rdtscp
	nvc_svm_decoder_instruction_handler,	// icebp
	nvc_svm_decoder_instruction_handler,	// wb(no)invd
	nvc_svm_decoder_instruction_handler,	// monitor(x)
	nvc_svm_decoder_instruction_handler,	// mwait(x)
	nvc_svm_decoder_instruction_handler,	// Conditional mwait(x)
	nvc_svm_decoder_instruction_handler,	// rdpru
	nvc_svm_decoder_instruction_handler,	// xsetbv
	nvc_svm_decoder_event_handler,			// wrmsr EFER trap
	// No need to decode write-traps of control registers.
	nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,
	nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,
	nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,
	nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,nvc_svm_decoder_event_handler,
	// Interception Vector 3
	nvc_svm_decoder_instruction_handler,	// invlpgb
	nvc_svm_decoder_instruction_handler,	// invlpgb (illegal)
	nvc_svm_decoder_instruction_handler,	// invpcid
	nvc_svm_decoder_instruction_handler,	// mcommit
	nvc_svm_decoder_instruction_handler		// tlbsync
};

noir_hvdata noir_svm_decoder_handler_routine svm_decoder_group2[noir_svm_maximum_code2]=
{
	nvc_svm_decoder_npf_handler,			// Nested Page Fault
	nvc_svm_decoder_event_handler,			// Incomplete VIPI
	nvc_svm_decoder_event_handler,			// Unaccelerated AVIC
	nvc_svm_decoder_instruction_handler		// vmgexit
};

noir_hvdata noir_svm_decoder_handler_routine svm_decoder_handler_negative[noir_svm_maximum_negative]=
{
	nvc_svm_decoder_event_handler,			// Invalid State
	nvc_svm_decoder_event_handler			// Busy VMSA
};

noir_hvdata noir_svm_decoder_handler_routine* svm_decoder_handlers[4]=
{
	svm_decoder_group1,
	svm_decoder_group2,
	null,null
};
#endif

void inline noir_svm_inject_event(void* vmcb,u8 vector,u8 type,u8 ev,u8 v,u32 ec)
{
	amd64_event_injection event_field;
	event_field.vector=vector;
	event_field.type=type;
	event_field.error_valid=ev;
	event_field.reserved=0;
	event_field.valid=v;
	event_field.error_code=ec;
	noir_svm_vmwrite64(vmcb,event_injection,event_field.value);
}

void inline noir_svm_advance_rip(void* vmcb)
{
	ulong_ptr nrip=noir_svm_vmread(vmcb,next_rip);
	// In case the guest is single-step debugging, we should inject debug trace trap so that
	// the next instruction won't be skipped in debugger, confusing the debugging personnel.
	// If guest is single-step debugging, slight penalty due to branch predictor is acceptable.
	if(unlikely(noir_svm_vmcb_bt32(vmcb,guest_rflags,amd64_rflags_tf)))
		noir_svm_inject_event(vmcb,amd64_debug_exception,amd64_fault_trap_exception,false,true,0);
	// FIXME: Check Debug Registers. If condition matches, inject #DB exception.
	if(nrip)noir_svm_vmwrite(vmcb,guest_rip,nrip);
}