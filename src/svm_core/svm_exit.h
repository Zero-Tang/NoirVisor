/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the basic Exit Handler of SVM Driver.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_main.h
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
#define intercepted_cr0_tsmp		0x65
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
#define intercepted_efer_w_trap		0x8F
#define intercepted_cr_w_trap(x)	0x90+x
#define nested_page_fault			0x400
#define avic_incomplete_ipi			0x401
#define avic_no_acceleration		0x402
#define intercepted_vmgexit			0x403

#define invalid_guest_state			-1

#define noir_svm_maximum_code1		0x100
#define noir_svm_maximum_code2		0x4

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
 noir_gpr_state_p gpr_state,
 noir_svm_vcpu_p vcpu
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
noir_svm_exit_handler_routine** svm_exit_handlers=null;
extern noir_svm_cpuid_exit_handler** svm_cpuid_handlers;
#endif

void inline noir_svm_advance_rip(void* vmcb)
{
	ulong_ptr nrip=noir_svm_vmread(vmcb,next_rip);
	if(nrip)noir_svm_vmwrite(vmcb,guest_rip,nrip);
}

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