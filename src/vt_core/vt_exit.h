/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file is the exit handler of Intel VT-x.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).
  
  Reference:
  Appendix C, Volume 3D,
  Intel 64 and IA-32 Architectures Software Developer's Manual
  Order Number (Combined Volumes: 325462-070US, May 2019)

  File Location: /vt_core/vt_exit.h
*/

#include <nvdef.h>

#define vmx_maximum_exit_reason		70

typedef enum _vmx_vmexit_reason
{
	exception_nmi=0,
	external_interrupt=1,
	triple_fault=2,
	init_signal=3,
	startup_ipi=4,
	io_smi=5,
	other_smi=6,
	interrupt_window=7,
	nmi_window=8,
	task_switch=9,
	intercept_cpuid=10,
	intercept_getsec=11,
	intercept_hlt=12,
	intercept_invd=13,
	intercept_invlpg=14,
	intercept_rdpmc=15,
	intercept_rdtsc=16,
	intercept_rsm=17,
	intercept_vmcall=18,
	intercept_vmclear=19,
	intercept_vmlaunch=20,
	intercept_vmptrld=21,
	intercept_vmptrst=22,
	intercept_vmread=23,
	intercept_vmresume=24,
	intercept_vmwrite=25,
	intercept_vmxoff=26,
	intercept_vmxon=27,
	cr_access=28,
	dr_access=29,
	intercept_io=30,
	intercept_rdmsr=31,
	intercept_wrmsr=32,
	invalid_guest_state=33,
	msr_loading_failure=34,
	intercept_mwait=36,
	monitor_trap_flag=37,
	intercept_monitor=39,
	intercept_pause=40,
	machine_check_at_entry=41,
	tpr_below_threshold=43,
	apic_access=44,
	virtualized_eoi=45,
	gdtr_idtr_access=46,
	ldtr_tr_access=47,
	ept_violation=48,
	ept_misconfiguration=49,
	intercept_invept=50,
	intercept_rdtscp=51,
	vmx_preemption_timer_expired=52,
	intercept_invvpid=53,
	intercept_wbinvd=54,
	intercept_xsetbv=55,
	apic_write=56,
	intercept_rdrand=57,
	intercept_invpcid=58,
	intercept_vmfunc=59,
	intercept_encls=60,
	intercept_rdseed=61,
	pml_full=62,
	intercept_xsaves=63,
	intercept_xrstors=64,
	spp_related_event=66,
	intercept_umwait=67,
	intercept_tpause=68,
	intercept_loadiwkey=69
}vmx_vmexit_reason,*vmx_vmexit_reason_p;

#define ia32_external_interrupt			0
#define ia32_non_maskable_interrupt		2
#define ia32_hardware_exception			3
#define ia32_software_interrupt			4
#define ia32_prisw_exception			5
#define ia32_software_exception			6
#define ia32_other_event				7

typedef union _ia32_vmentry_interruption_information_field
{
	struct
	{
		u32 vector:8;		// Bits	0-7
		u32 type:3;			// Bits	8-10
		u32 deliver:1;		// Bit	11
		u32 reserved:19;	// Bits	12-30
		u32 valid:1;		// Bit	31
	};
	u32 value;
}ia32_vmentry_interruption_information_field,*ia32_vmentry_interruption_information_field_p;

typedef union _ia32_cr_access_qualification
{
	struct
	{
		ulong_ptr cr_num:4;			// Bits	0-3
		ulong_ptr access_type:2;	// Bits	4-5
		ulong_ptr lmsw_type:1;		// Bit	6
		ulong_ptr reserved0:1;		// Bit	7
		ulong_ptr gpr_num:4;		// Bits	8-11
		ulong_ptr reserved1:4;		// Bits	12-15
		ulong_ptr lmsw_src:16;		// Bits	16-31
#if defined(_amd64)
		ulong_ptr reserved2:32;
#endif
	};
	ulong_ptr value;
}ia32_cr_access_qualification,*ia32_cr_access_qualification_p;

typedef union _ia32_vmexit_instruction_information
{
	// ins, outs instructions use this field.
	struct
	{
		u32 reserved1:7;			// Bits	0-6
		u32 address_size:3;			// Bits 7-9
		u32 reserved2:5;			// Bits 10-14
		u32 segment:3;				// Bits 15-17
		u32 reserved3:14;			// Bits 18-31
	}f0;
	// invept, invpcid, invvpid instructions use this field.
	struct
	{
		u32 scaling:2;				// Bits	0-1
		u32 reserved1:5;			// Bits	2-6
		u32 address_size:3;			// Bits 7-9
		u32 reserved2:5;			// Bits	10-14
		u32 segment:3;				// Bits 15-17
		u32 index:4;				// Bits 18-21
		u32 index_invalid:1;		// Bit	22
		u32 base:4;					// Bits	23-26
		u32 base_invalid:1;			// Bit	27
		u32 reg2:4;					// Bits	28-31
	}f1;
	// lidt, lgdt, sidt, sgdt instructions use this field.
	struct
	{
		u32 scaling:2;				// Bits	0-1
		u32 reserved1:5;			// Bits	2-6
		u32 address_size:3;			// Bits 7-9
		u32 reserved2:1;			// Bit	10
		u32 operand_size:1;			// Bit	11
		u32 reserved3:3;			// Bits	12-14
		u32 segment:3;				// Bits 15-17
		u32 index:4;				// Bits 18-21
		u32 index_invalid:1;		// Bit	22
		u32 base:4;					// Bits	23-26
		u32 base_invalid:1;			// Bit	27
		u32 instruction_id:2;		// Bits	28-29
		u32 reserved4:2;			// Bits	30-31
	}f2;
	// lldt, ltr, sldt, str instructions use this field.
	struct
	{
		u32 scaling:2;				// Bits	0-1
		u32 reserved1:1;			// Bit	2
		u32 reg1:4;					// Bits	3-6
		u32 address_size:3;			// Bits 7-9
		u32 use_register:1;			// Bit	10
		u32 reserved2:4;			// Bits	11-14
		u32 segment:3;				// Bits 15-17
		u32 index:4;				// Bits 18-21
		u32 index_invalid:1;		// Bit	22
		u32 base:4;					// Bits	23-26
		u32 base_invalid:1;			// Bit	27
		u32 instruction_id:2;		// Bits	28-29
		u32 reserved3:2;			// Bits	30-31
	}f3;
	// rdrand, rdseed, tpause, umwait instructions use this field.
	struct
	{
		u32 reserved1:3;			// Bits	0-2
		u32 operand_reg:4;			// Bits 3-6
		u32 reserved2:4;			// Bits 7-10
		u32 operand_size:2;			// Bits 11-12
		u32 reserved3:19;			// Bits 13-31
	}f4;
	// vmclear, vmptrld, vmptrst, vmxon, xrstors, xsaves
	// instructions use this field.
	struct
	{
		u32 scaling:2;				// Bits	0-1
		u32 reserved1:5;			// Bits	2-6
		u32 address_size:3;			// Bits 7-9
		u32 reserved2:5;			// Bits	10-14
		u32 segment:3;				// Bits 15-17
		u32 index:4;				// Bits 18-21
		u32 index_invalid:1;		// Bit	22
		u32 base:4;					// Bits	23-26
		u32 base_invalid:1;			// Bit	27
		u32 instruction_id:2;		// Bits	28-29
		u32 reserved3:2;			// Bits	30-31
	}f5;
	// vmread, vmwrite instructions use this field.
	struct
	{
		u32 scaling:2;				// Bits	0-1
		u32 reserved1:1;			// Bit	2
		u32 reg1:4;					// Bits	3-6
		u32 address_size:3;			// Bits 7-9
		u32 use_register:1;			// Bit	10
		u32 reserved2:4;			// Bits	11-14
		u32 segment:3;				// Bits 15-17
		u32 index:4;				// Bits 18-21
		u32 index_invalid:1;		// Bit	22
		u32 base:4;					// Bits	23-26
		u32 base_invalid:1;			// Bit	27
		u32 reg2:4;					// Bits	28-31
	}f6;
	u32 value;
}ia32_vmexit_instruction_information,*ia32_vmexit_instruction_information_p;

typedef void (fastcall *noir_vt_exit_handler_routine)
(
 noir_gpr_state_p gpr_state,
 noir_vt_vcpu_p vcpu
);

typedef void (fastcall *noir_vt_cpuid_exit_handler)
(
 u32 leaf,
 u32 subleaf,
 noir_cpuid_general_info_p info
);

#if defined(_vt_exit)
const char* vmx_exit_msg[vmx_maximum_exit_reason]=
{
	"Exception or NMI is intercepted!",						// Reason=0
	"External Interrupt is intercepted!",					// Reason=1
	"Triple fault is intercepted!",							// Reason=2
	"An INIT signal arrived!",								// Reason=3
	"Startup-IPI arrived!",									// Reason=4
	"I/O System-Management Interrupt is intercepted!",		// Reason=5
	"Other System-Management Interrupt is intercepted!",	// Reason=6
	"Exit is due to Interrupt Window!",						// Reason=7
	"Exit is due to NMI Window!",							// Reason=8
	"Task Switch is intercepted!",							// Reason=9
	"CPUID instruction is intercepted!",					// Reason=10
	"GETSEC instruction is intercepted!",					// Reason=11
	"HLT instruction is intercepted!",						// Reason=12
	"INVD instruction is intercepted!",						// Reason=13
	"INVLPG instruction is intercepted!",					// Reason=14
	"RDPMC instruction is intercepted!",					// Reason=15
	"RDTSC instruction is intercepted!",					// Reason=16
	"RSM instruction is intercepted!",						// Reason=17
	"VMCALL instruction is intercepted!",					// Reason=18
	"VMCLEAR instruction is intercepted!",					// Reason=19
	"VMLAUNCH instruction is intercepted!",					// Reason=20
	"VMPTRLD instruction is intercepted!",					// Reason=21
	"VMPTRST instruction is intercepted!",					// Reason=22
	"VMREAD instruction is intercepted!",					// Reason=23
	"VMRESUME instruction is intercepted!",					// Reason=24
	"VMWRITE instruction is intercepted!",					// Reason=25
	"VMXOFF instruction is intercepted!",					// Reason=26
	"VMXON instruction is intercepted!",					// Reason=27
	"Control-Register access is intercepted!",				// Reason=28
	"Debug-Register access is intercepted!",				// Reason=29
	"I/O instruction is intercepted!",						// Reason=30
	"RDMSR instruction is intercepted!",					// Reason=31
	"WRMSR instruction is intercepted!",					// Reason=32
	"VM-Entry failed due to Invalid Guest-State!",			// Reason=33
	"VM-Entry failed due to MSR-Loading!",					// Reason=34
	"Unknown Exit, Reason=35!",								// Reason=35
	"MWAIT instruction is intercepted!",					// Reason=36
	"Exit is due to Monitor Trap Flag!",					// Reason=37
	"Unknown Exit, Reason=38!",								// Reason=38
	"MONITOR instruction is intercepted!",					// Reason=39
	"PAUSE instruction is intercepted!",					// Reason=40
	"VM-Entry failed due to Machine-Check Event!",			// Reason=41
	"Unknown Exit, Reason=42!",								// Reason=42
	"TPR is below threshold!",								// Reason=43
	"APIC access is intercepted!",							// Reason=44
	"EOI-Virtualization is performed!",						// Reason=45
	"Access to GDTR or IDTR is intercepted!",				// Reason=46
	"Access to LDTR or TR is intercepted!",					// Reason=47
	"EPT Violation is intercepted!",						// Reason=48
	"EPT Misconfiguration is intercepted!",					// Reason=49
	"INVEPT instruction is intercepted!",					// Reason=50
	"RDTSCP instruction is intercepted!",					// Reason=51
	"VMX-preemption Timer is expired!",						// Reason=52
	"INVVPID instruction is intercepted!",					// Reason=53
	"WBINVD instruction is intercepted!",					// Reason=54
	"XSETBV instruction is intercepted!",					// Reason=55
	"APIC write is intercepted!",							// Reason=56
	"RDRAND instruction is intercepted!",					// Reason=57
	"INVPCID instruction is intercepted!",					// Reason=58
	"WBINVD instruction is intercepted!",					// Reason=59
	"ENCLS instruction is intercepted!",					// Reason=60
	"RDSEED instruction is intercepted!",					// Reason=61
	"Page-Modification Log is full!",						// Reason=62
	"XSAVES instruction is intercepted!",					// Reason=63
	"XRSTORS instruction is intercepted!",					// Reason=64
	"Unknown Exit, Reason=65",								// Reason=65
	"Sub-Page Miss or Misconfiguration is intercepted!",	// Reason=66
	"UMWAIT instruction is intercepted!",					// Reason=67
	"TPAUSE instruction is intercepted!",					// Reason=68
	"LOADIWKEY instruction is intercepted!",				// Reason=69
};

void static fastcall nvc_vt_default_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_trifault_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_init_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_sipi_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_task_switch_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_cpuid_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_getsec_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_invd_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_vmcall_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_vmclear_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_vmptrld_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_vmptrst_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_vmxoff_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_vmxon_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_vmx_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_cr_access_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_rdmsr_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_wrmsr_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_invalid_guest_state(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_invalid_msr_loading(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_ept_violation_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_ept_misconfig_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);
void static fastcall nvc_vt_xsetbv_handler(noir_gpr_state_p gpr_state,noir_vt_vcpu_p vcpu);

noir_vt_exit_handler_routine vt_exit_handlers[vmx_maximum_exit_reason]=
{
	nvc_vt_default_handler,			// Exception or NMI
	nvc_vt_default_handler,			// External Interrupt
	nvc_vt_trifault_handler,		// Triple Fault
	nvc_vt_init_handler,			// INIT Signal
	nvc_vt_sipi_handler,			// Start-up IPI
	nvc_vt_default_handler,			// I/O SMI
	nvc_vt_default_handler,			// Other SMI
	nvc_vt_default_handler,			// Interrupt Window
	nvc_vt_default_handler,			// NMI Window
	nvc_vt_task_switch_handler,		// Task Switch
	nvc_vt_cpuid_handler,			// CPUID Instruction
	nvc_vt_getsec_handler,			// GETSEC Instruction
	nvc_vt_default_handler,			// HLT Instruction
	nvc_vt_invd_handler,			// INVD Instruction
	nvc_vt_default_handler,			// INVLPG Instruction
	nvc_vt_default_handler,			// RDPMC Instruction
	nvc_vt_default_handler,			// RDTSC Instruction
	nvc_vt_default_handler,			// RSM Instruction
	nvc_vt_vmcall_handler,			// VMCALL Instruction
	nvc_vt_vmclear_handler,			// VMCLEAR Instruction
	nvc_vt_default_handler,			// VMLAUNCH Instruction
	nvc_vt_vmptrld_handler,			// VMPTRLD Instruction
	nvc_vt_vmptrst_handler,			// VMPTRST Instruction
	nvc_vt_default_handler,			// VMREAD Instruction
	nvc_vt_default_handler,			// VMRESUME Instruction
	nvc_vt_default_handler,			// VMWRITE Instruction
	nvc_vt_vmxoff_handler,			// VMXOFF Instruction
	nvc_vt_vmxon_handler,			// VMXON Instruction
	nvc_vt_cr_access_handler,		// Control-Register Access
	nvc_vt_default_handler,			// Debug-Register Access
	nvc_vt_default_handler,			// I/O Instruction
	nvc_vt_rdmsr_handler,			// RDMSR Instruction
	nvc_vt_wrmsr_handler,			// WRMSR Instruction
	nvc_vt_invalid_guest_state,		// Invalid Guest State
	nvc_vt_invalid_msr_loading,		// MSR-Loading Failure
	nvc_vt_default_handler,			// Reserved (35)
	nvc_vt_default_handler,			// MWAIT Instruction
	nvc_vt_default_handler,			// Monitor Trap Flag
	nvc_vt_default_handler,			// Reserved (38)
	nvc_vt_default_handler,			// MONITOR Instruction
	nvc_vt_default_handler,			// PAUSE Instruction
	nvc_vt_default_handler,			// Machine-Check during VM-Entry
	nvc_vt_default_handler,			// Reserved (42)
	nvc_vt_default_handler,			// TPR Below Threshold
	nvc_vt_default_handler,			// APIC Access
	nvc_vt_default_handler,			// Virtualized EOI
	nvc_vt_default_handler,			// GDTR/IDTR Access
	nvc_vt_default_handler,			// LDTR/TR Access
	nvc_vt_ept_violation_handler,	// EPT Violation
	nvc_vt_ept_misconfig_handler,	// EPT Misconfiguration
	nvc_vt_default_handler,			// INVEPT Instruction
	nvc_vt_default_handler,			// RDTSCP Instruction
	nvc_vt_default_handler,			// VMX-Preemption Timer Expiry
	nvc_vt_default_handler,			// INVVPID Instruction
	nvc_vt_default_handler,			// WBINVD/WBNOINVD Instruction
	nvc_vt_xsetbv_handler,			// XSETBV Instruction
	nvc_vt_default_handler,			// APIC Write
	nvc_vt_default_handler,			// RDRAND Instruction
	nvc_vt_default_handler,			// INVPCID Instruction
	nvc_vt_default_handler,			// VMFUNC Instruction
	nvc_vt_default_handler,			// ENCLS Instruction
	nvc_vt_default_handler,			// RDSEED Instruction
	nvc_vt_default_handler,			// Page-Modification Log Full
	nvc_vt_default_handler,			// XSAVES Instruction
	nvc_vt_default_handler,			// XRSTORS Instruction
	nvc_vt_default_handler,			// Reserved (65)
	nvc_vt_default_handler,			// Sub-Page Permission Related Event
	nvc_vt_default_handler,			// UMWAIT Instruction
	nvc_vt_default_handler,			// TPAUSE Instruction
	nvc_vt_default_handler			// LOADIWKEY Instruction
};
noir_vt_cpuid_exit_handler nvcp_vt_cpuid_handler=null;
#endif

void inline noir_vt_advance_rip()
{
	ulong_ptr gip,gflags;
	u32 len;
	// Special treatings for Single-Step scenarios.
	u8 vst=noir_vt_vmread(guest_rflags,&gflags);
	if(vst==0 && noir_bt(&gflags,ia32_rflags_tf))
	{
		// Don't inject an #DB exception since Intel has its own
		// specific feature regarding pending debug exceptions.
		ia32_vmx_pending_debug_exceptions pending_de;
		ia32_vmx_interruptibility_state interruptibility;
		// General Single-Step debug exceptions.
		noir_vt_vmread(guest_pending_debug_exceptions,&pending_de.value);
		pending_de.bs=true;
		noir_vt_vmwrite(guest_pending_debug_exceptions,pending_de.value);
		// Special Blockings onto Single-Step debug exceptions.
		noir_vt_vmread(guest_interruptibility_state,&interruptibility.value);
		interruptibility.blocking_by_sti=false;
		interruptibility.blocking_by_mov_ss=false;
		noir_vt_vmwrite(guest_interruptibility_state,interruptibility.value);
	}
	// Regular stuff...
	noir_vt_vmread(guest_rip,&gip);
	noir_vt_vmread(vmexit_instruction_length,&len);
	noir_vt_vmwrite(guest_rip,gip+len);
}

void inline noir_vt_inject_event(u8 vector,u8 type,bool deliver,u32 length,u32 err_code)
{
	ia32_vmentry_interruption_information_field event_field;
	event_field.value=0;
	event_field.valid=1;
	event_field.deliver=deliver;
	event_field.vector=vector;
	event_field.type=type;
	noir_vt_vmwrite(vmentry_interruption_information_field,event_field.value);
	if(deliver)noir_vt_vmwrite(vmentry_exception_error_code,err_code);
	noir_vt_vmwrite(vmentry_instruction_length,length);
}