/*
  NoirVisor - A cross-platform designed HyperVisor

  This file defines constants and basic structures for Intel VT-x.

  This program is distributed in the hope that it will be useful, but
  without any warranty (no matter implied warranty of merchantablity
  or fitness for a particular purpose, etc.).

  File location: /include/vt/vmxdef.h

  Update Time: Sept.12th, 2018
*/

#include <nvdef.h>

#define vmx_cpuid_support_bit		5

#define vmx_success(s)				s==0

typedef union _ia32_vmx_basic_msr
{
	struct
	{
		u64 revision_id:31;				//bits	0-30
		u64 reserved1:1;				//bit	31
		u64 vmxon_region_size:13;		//bits	32-44
		u64 reserved2:3;				//bits	45-47
		u64 vmxon_pa_width:1;			//bit	48
		u64 dual_mon_treat:1;			//bit	49
		u64 memory_type:4;				//bits	50-53
		u64 report_io_on_exit:1;		//bit	54
		u64 use_true_msr:1;				//bit	55
		u64 reserved3:8;				//bits	56-63
	};
	u64 value;
}ia32_vmx_basic_msr,*ia32_vmx_basic_msr_p;

typedef union _ia32_vmx_pinbased_controls
{
	struct
	{
		u32 external_interrupt_exiting:1;		//bit	0
		u32 reserved0:2;						//bits	1-2
		u32 nmi_exiting:1;						//bit	3
		u32	reserved1:1;						//bit	4
		u32 virtual_nmi:1;						//bit	5
		u32 activate_vmx_preemption_timer:1;	//bit	6
		u32 process_posted_interrupts:1;		//bit	7
		u32 reserved2:24;						//bit 8-31
	};
	u32 value;
}ia32_vmx_pinbased_controls,*ia32_vmx_pinbased_controls_p;

typedef union _ia32_vmx_pinbased_ctrl_msr
{
	struct
	{
		ia32_vmx_pinbased_controls allowed0_settings;
		ia32_vmx_pinbased_controls allowed1_settings;
	};
	u64 value;
}ia32_vmx_pinbased_ctrl_msr,*ia32_vmx_pinbased_ctrl_msr_p;

typedef union _ia32_vmx_priproc_controls
{
	struct
	{
		u32 reserved0:2;					//bits	0-1
		u32 interrupt_window_exiting:1;		//bit	2
		u32 use_tsc_offsetting:1;			//bit	3
		u32 reserved1:3;					//bits	4-6
		u32 hlt_exiting:1;					//bit	7
		u32 reserved2:1;					//bit	8
		u32 invlpg_exiting:1;				//bit	9
		u32 mwait_exiting:1;				//bit	10
		u32 rdpmc_exiting:1;				//bit	11
		u32 rdtsc_exiting:1;				//bit	12
		u32 reserved3:2;					//bits	13-14
		u32 cr3_load_exiting:1;				//bit	15
		u32 cr3_store_exiting:1;			//bit	16
		u32 reserved4:2;					//bits	17-18
		u32 cr8_load_exiting:1;				//bit	19
		u32 cr8_store_exiting:1;			//bit	20
		u32 use_tpr_shadow:1;				//bit	21
		u32 nmi_window_exiting:1;			//bit	22
		u32 mov_dr_exiting:1;				//bit	23
		u32 unconditional_io_exiting:1;		//bit	24
		u32 use_io_bitmap:1;				//bit	25
		u32 reserved5:1;					//bit	26
		u32 monitor_trap_flag:1;			//bit	27
		u32 use_msr_bitmap:1;				//bit	28
		u32 monitor_exiting:1;				//bit	29
		u32 pause_exiting:1;				//bit	30
		u32 activate_secondary_controls:1;	//bit	31
	};
	u32 value;
}ia32_vmx_priproc_controls,*ia32_vmx_priproc_controls_p;

typedef union _ia32_vmx_priproc_ctrl_msr
{
	struct
	{
		ia32_vmx_priproc_controls allowed0_settings;
		ia32_vmx_priproc_controls allowed1_settings;
	};
	u64 value;
}ia32_vmx_priproc_ctrl_msr,*ia32_vmx_priproc_ctrl_msr_p;

typedef union _ia32_vmx_2ndproc_controls
{
	struct
	{
		u32 virtualize_apic_accesses:1;			//bit	0
		u32 enable_ept:1;						//bit	1
		u32 descriptor_table_exiting:1;			//bit	2
		u32 enable_rdtscp:1;					//bit	3
		u32 enable_virtualize_x2apic_mode:1;	//bit	4
		u32 enable_vpid:1;						//bit	5
		u32 wbinvd_exiting:1;					//bit	6
		u32 unrestricted_guest:1;				//bit	7
		u32 apic_register_virtualization:1;		//bit	8
		u32 virtual_interrupt_delivery:1;		//bit	9
		u32 pause_loop_exiting:1;				//bit	10
		u32 rdrand_exiting:1;					//bit	11
		u32 enable_invpcid:1;					//bit	12
		u32 enable_vmfunc:1;					//bit	13
		u32 vmcs_shadowing:1;					//bit	14
		u32 enable_encls_exiting:1;				//bit	15
		u32 enable_rdseed_exiting:1;			//bit	16
		u32 enable_pml:1;						//bit	17
		u32 ept_violation_as_exception:1;		//bit	18
		u32 conceal_non_root_from_pt:1;			//bit	19
		u32 enable_xsaves_xrstors:1;			//bit	20
		u32 reserved0:1;						//bit	21
		u32 ept_mode_based_exec_ctrl:1;			//bit	22
		u32 reserved1:2;						//bits	23-24
		u32 use_tsc_scaling:1;					//bit	25
		u32 reserved2:6;						//bits	26-31
	};
	u32 value;
}ia32_vmx_2ndproc_controls,*ia32_vmx_2ndproc_controls_p;

typedef union _ia32_vmx_2ndproc_ctrl_msr
{
	struct
	{
		ia32_vmx_2ndproc_controls allowed0_settings;
		ia32_vmx_2ndproc_controls allowed1_settings;
	};
	u64 value;
}ia32_vmx_2ndproc_ctrl_msr,*ia32_vmx_2ndproc_ctrl_msr_p;

typedef union _ia32_vmx_exit_controls
{
	struct
	{
		u32 reserved0:2;						//bits	0-1
		u32 save_debug_controls:1;				//bit	2
		u32 reserved1:6;						//bits	3-8
		u32 host_address_space_size:1;			//bit	9
		u32 reserved2:2;						//bits	10-11
		u32 load_ia32_perf_global_ctrl:1;		//bit	12
		u32 reserved3:2;						//bits	13-14
		u32 acknowledge_interrupt_on_exit:1;	//bit	15
		u32 reserved4:2;						//bits	16-17
		u32 save_ia32_pat:1;					//bit	18
		u32 load_ia32_pat:1;					//bit	19
		u32 save_ia32_efer:1;					//bit	20
		u32 load_ia32_efer:1;					//bit	21
		u32 save_vmx_preemption_timer:1;			//bit	22
		u32 clear_ia32_bound_cfg:1;				//bit	23
		u32 conceal_vmexit_from_pt:1;			//bit	24
		u32 reserved5:7;						//bits	25-31
	};
	u32 value;
}ia32_vmx_exit_controls,*ia32_vmx_exit_controls_p;

typedef union _ia32_vmx_exit_ctrl_msr
{
	struct
	{
		ia32_vmx_exit_controls allowed0_settings;
		ia32_vmx_exit_controls allowed1_settings;
	};
	u64 value;
}ia32_vmx_exit_ctrl_msr,*ia32_vmx_exit_ctrl_msr_p;

typedef union _ia32_vmx_entry_controls
{
	struct
	{
		u32 reserved0:2;					//bits	0-1
		u32 load_debug_controls:1;			//bit	2
		u32 reserved1:6;					//bits	3-8
		u32 ia32e_mode_guest:1;				//bit	9
		u32 entry_to_smm:1;					//bit	10
		u32 deactivate_dmt:1;				//bit	11
		u32 reserved2:1;					//bit	12
		u32 load_ia32_perf_global_ctrl:1;	//bit	13
		u32 load_ia32_pat:1;				//bit	14
		u32	load_ia32_efer:1;				//bit	15
		u32 load_ia32_bound_cfg:1;			//bit	16
		u32 conceal_vmentry_from_pt:1;		//bit	17
		u32 reserved3:14;					//bit	18-31
	};
	u32 value;
}ia32_vmx_entry_controls,*ia32_vmx_entry_controls_p;

typedef union _ia32_vmx_entry_ctrl_msr
{
	struct
	{
		ia32_vmx_entry_controls allowed0_settings;
		ia32_vmx_entry_controls allowed1_settings;
	};
	u64 value;
}ia32_vmx_entry_ctrl_msr,*ia32_vmx_entry_ctrl_msr_p;

typedef union _ia32_vmx_misc_msr
{
	struct
	{
		u64 tsc_preemption_scale:5;				//bits	0-4
		u64 store_lma_to_entry_on_exit:1;		//bit	5
		u64 supported_activity_state:3;			//bits	6-8
		u64 reserved0:5;						//bits	9-13
		u64 allow_use_pt_in_vmx:1;				//bit	14
		u64 allow_read_ia32_smbase_in_smm:1;	//bit	15
		u64 number_of_cr3_rarget_values:9;		//bits	16-24
		u64 best_max_num_msr_store:3;			//bits	25-27
		u64 allow_unblock_smi:1;				//bit	28
		u64 allow_any_writing_to_vmcs:1;		//bit	29
		u64 allow_null_injection:1;				//bit	30
		u64 reserved1:1;						//bit	31
		u64	mseg_revision_id:32;				//bits	32-63
	};
	u64 value;
}ia32_vmx_misc_msr,*ia32_vmx_misc_msr_p;

typedef union _ia32_vmx_ept_vpid_cap_msr
{
	struct
	{
		u64 support_exec_only_translation:1;	//bit	0
		u64 reserved0:5;						//bits	1-5
		u64 page_walk_length_of4:1;				//bit	6
		u64 reserved1:1;						//bit	7
		u64 support_uc_ept:1;					//bit	8
		u64 reserved2:5;						//bits	9-13
		u64 support_wb_ept:1;					//bit	14
		u64 reserved3:1;						//bit	15
		u64 support_2mb_paging:1;				//bit	16
		u64 support_1gb_paging:1;				//bit	17
		u64 reserved4:2;						//bits	18-19
		u64 support_invept:1;					//bit	20
		u64 support_accessed_dirty_flags:1;		//bit	21
		u64 report_advanced_epf_exit_info:1;	//bit	22
		u64 reserved5:2;						//bits	23-24
		u64 support_single_context_invept:1;	//bit	25
		u64 support_all_context_invept:1;		//bit	26
		u64 reserved6:5;						//bits	27-31
		u64	support_invvpid:1;					//bit	32
		u64	reserved7:7;						//bits	33-39
		u64 supported_invvpid_type:4;			//bits	40-43
		u64 reserved8:20;						//bits	44-63
	};
	u64 value;
}ia32_vmx_ept_vpid_cap_msr,*ia32_vmx_ept_vpid_cap_msr_p;

typedef union _vmx_segment_access_right
{
	struct
	{
		u32 segment_type:4;
		u32 descriptor_type:1;
		u32 dpl:2;
		u32 present:1;
		u32 reserved0:4;
		u32 avl:1;
		u32 long_mode:1;
		u32 op_size:1;
		u32 granularity:1;
		u32 unusable:1;
		u32 reserved1:15;
	};
	u32 value;
}vmx_segment_access_right,*vmx_segment_access_right_p;

u32 inline vmx_lar(u16 attributes)
{
	vmx_segment_access_right ar;
	ar.value=attributes;
	ar.reserved0=0;
	ar.reserved1=0;
	ar.unusable=!ar.present;
	return ar.value;
}

char* vt_error_message[0x20]=
{
	"Invalid Error, Number=0!",										//Error=0
	"vmcall is executed in VMX Root Operation!",					//Error=1
	"vmclear is given invalid physical address as operand!",		//Error=2
	"vmclear is given vmxon region as operand!",					//Error=3
	"vmlaunch is given non-clear vmcs as operand!",					//Error=4
	"vmresume is given non-launched vmcs as operand!",				//Error=5
	"vmresume is executed after vmxoff!",							//Error=6
	"VM-Entry failed due to invalid control fields!",				//Error=7
	"VM-Entry failed due to invalid host state!",					//Error=8
	"vmptrld is given invalid physical address as operand!",		//Error=9
	"vmptrld is given vmxon region as operand!",					//Error=10
	"vmptrld is given vmcs with incorrect revision id!",			//Error=11
	"vmread/vmwrite is given unsupported field as operand!",		//Error=12
	"vmwrite is given read-only field as operand!",					//Error=13
	"Invalid Error, Number=14!",									//Error=14
	"vmxon is executed in VMX Root Operation!",						//Error=15
	"VM-Entry failed due to invalid executive-vmcs!",				//Error=16
	"VM-Entry failed due to non-launched executive-vmcs!",			//Error=17
	//Error=18
	"VM-Entry failed due to executive-vmcs not vmxon region! (Are you attempting to deactivate dual-monitor treatment?)",
	//Error=19
	"VM-Entry failed due to non-clear vmcs! (Are you attempting to deactivate dual-monitor treatment?)",
	"vmcall is given invalid vmexit control fields!",				//Error=20
	"Invalid Error, Number=21!",									//Error=21
	//Error=22
	"vmcall is given incorrect mseg revision id! (Are you attempting to deactivate dual-monitor treatment?)",
	"vmxoff is executed under dual-monitor treatment!",				//Error=23
	//Error=24
	"vmcall is given invalid SMM-Monitor features! (Are you attempting to activate dual-monitor treatment?)",
	//Error=25
	"VM-Entry failed due to invalid VM-Exeuction Control! (Are attempting to return from SMM?)",
	//Error=26,
	"Invalid Error, Number=27!"										//Error=27
	"VM-Entry failed due to events blocked by mov ss!",				//Error=28
	"Invalid Error, Number=29!",									//Error=29
	"Invalid Error, Number=30!",									//Error=30
	"Invalid Error, Number=31!"										//Error=31
};