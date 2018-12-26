/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file is the exit handler of Intel VT-x.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /vt_core/vt_exit.h
*/

#include <nvdef.h>

#define vmx_maximum_exit_reason		65

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
	intercept_xrstors=64
}vmx_vmexit_reason,*vmx_vmexit_reason_p;

typedef void (fastcall *noir_vt_exit_handler_routine)
(
 noir_gpr_state_p gpr_state,
 u32 exit_reason
);

char* vmx_exit_msg[vmx_maximum_exit_reason]=
{
	"Exception or NMI is intercepted!",						//Reason=0
	"External Interrupt is intercepted!",					//Reason=1
	"Triple fault is intercepted!",							//Reason=2
	"An INIT signal arrived!",								//Reason=3
	"Startup-IPI arrived!",									//Reason=4
	"I/O System-Management Interrupt is intercepted!",		//Reason=5
	"Other System-Management Interrupt is intercepted!",	//Reason=6
	"Exit is due to Interrupt Window!",						//Reason=7
	"Exit is due to NMI Window!",							//Reason=8
	"Task Switch is intercepted!",							//Reason=9
	"CPUID instruction is intercepted!",					//Reason=10
	"GETSEC instruction is intercepted!",					//Reason=11
	"HLT instruction is intercepted!",						//Reason=12
	"INVD instruction is intercepted!",						//Reason=13
	"INVLPG instruction is intercepted!",					//Reason=14
	"RDPMC instruction is intercepted!",					//Reason=15
	"RDTSC instruction is intercepted!",					//Reason=16
	"RSM instruction is intercepted!",						//Reason=17
	"VMCALL instruction is intercepted!",					//Reason=18
	"VMCLEAR instruction is intercepted!",					//Reason=19
	"VMLAUNCH instruction is intercepted!",					//Reason=20
	"VMPTRLD instruction is intercepted!",					//Reason=21
	"VMPTRST instruction is intercepted!",					//Reason=22
	"VMREAD instruction is intercepted!",					//Reason=23
	"VMRESUME instruction is intercepted!",					//Reason=24
	"VMWRITE instruction is intercepted!",					//Reason=25
	"VMXOFF instruction is intercepted!",					//Reason=26
	"VMXON instruction is intercepted!",					//Reason=27
	"Control-Register access is intercepted!",				//Reason=28
	"Debug-Register access is intercepted!",				//Reason=29
	"I/O instruction is intercepted!",						//Reason=30
	"RDMSR instruction is intercepted!",					//Reason=31
	"WRMSR instruction is intercepted!",					//Reason=32
	"VM-Entry failed due to Invalid Guest-State!",			//Reason=33
	"VM-Entry failed due to MSR-Loading!",					//Reason=34
	"Unknown Exit, Reason=35!",								//Reason=35
	"MWAIT instruction is intercepted!",					//Reason=36
	"Exit is due to Monitor Trap Flag!",					//Reason=37
	"Unknown Exit, Reason=38!",								//Reason=38
	"MONITOR instruction is intercepted!",					//Reason=39
	"PAUSE instruction is intercepted!",					//Reason=40
	"VM-Entry failed due to Machine-Check Event!",			//Reason=41
	"Unknown Exit, Reason=42!",								//Reason=42
	"TPR is below threshold!",								//Reason=43
	"APIC access is intercepted!",							//Reason=44
	"EOI-Virtualization is performed!",						//Reason=45
	"Access to GDTR or IDTR is intercepted!",				//Reason=46
	"Access to LDTR or TR is intercepted!",					//Reason=47
	"EPT Violation is intercepted!",						//Reason=48
	"EPT Misconfiguration is intercepted!",					//Reason=49
	"INVEPT instruction is intercepted!",					//Reason=50
	"RDTSCP instruction is intercepted!",					//Reason=51
	"VMX-preemption Timer is expired!",						//Reason=52
	"INVVPID instruction is intercepted!",					//Reason=53
	"WBINVD instruction is intercepted!",					//Reason=54
	"XSETBV instruction is intercepted!",					//Reason=55
	"APIC write is intercepted!",							//Reason=56
	"RDRAND instruction is intercepted!",					//Reason=57
	"INVPCID instruction is intercepted!",					//Reason=58
	"WBINVD instruction is intercepted!",					//Reason=59
	"ENCLS instruction is intercepted!",					//Reason=60
	"RDSEED instruction is intercepted!",					//Reason=61
	"Page-Modification Log is full!",						//Reason=62
	"XSAVES instruction is intercepted!",					//Reason=63
	"XRSTORS instruction is intercepted!",					//Reason=64
};

noir_vt_exit_handler_routine* vt_exit_handlers=null;

void inline noir_vt_advance_rip()
{
	ulong_ptr gip;
	u32 len;
	noir_vt_vmread(guest_rip,&gip);
	noir_vt_vmread(vmexit_instruction_length,&len);
	noir_vt_vmwrite(guest_rip,gip+len);
}