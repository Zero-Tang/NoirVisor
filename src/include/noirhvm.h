/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the central HyperVisor of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/noirhvm.h
*/

#include "nvdef.h"

#include "mshv_hvm.h"
#include "cvm_hvm.h"
#include "hax_hvm.h"
#if defined(_vt_core)
#include "vt_hvm.h"
#elif defined(_svm_core)
#include "svm_hvm.h"
#endif

// Known Processor Manufacturer Encoding
// Notice that some manufacturers of x86 processors might also
// introduce hardware-accelerated virtualization technology.
#define intel_processor			0		// Intel Processor (Intel VT-x)
#define amd_processor			1		// AMD Processor (AMD-V)
#define via_processor			2		// VIA Processor (Intel VT-x)
#define zhaoxin_processor		3		// Zhaoxin Processor (Intel VT-x)
#define hygon_processor			4		// Hygon Processor (AMD-V)
#define centaur_processor		5		// Centaur Processor (Intel VT-x)
#define cyrix_processor			6		// Cyrix Processor (N/A)
#define trmeta_processor		7		// Transmeta Processor (N/A)
#define nexgen_processor		8		// NexGen Processor (N/A)
#define sis_processor			9		// SiS Processor (N/A)
#define nsc_processor			10		// National Semiconductor Processor (N/A)
#define rise_processor			11		// Rise Processor (N/A)
#define umc_processor			12		// UMC Processor (N/A)
#define vortex_processor		13		// Vortex Processor (N/A)
#define unknown_processor		0xff

// Determine which core to use.
#define use_nothing				0
#define use_vt_core				1
#define use_svm_core			2
#define use_unknown_core		0xff

// Processor State of Virtualization
#define noir_virt_off			0
#define noir_virt_on			1
#define noir_virt_trans			2
#define noir_virt_nesting		3

// Stack Size = 16KB
#define nvc_stack_size			0x4000
#define nvc_stack_pages			4

// Define Generic Hypercall Codes for NoirVisor management.
#define noir_hypercall_callexit					0x1
#define noir_hypercall_readphys					0x2
#define noir_hypercall_writephys				0x3

// Define Generic Hypercall Codes for Customizable VM.
#define noir_cvm_run_vcpu					0x10001
#define noir_cvm_dump_vcpu_vmcb				0x10002
#define noir_cvm_set_vcpu_options			0x10003
#define noir_cvm_guest_memory_operation		0x10004

// Define the ownership purposes on Reverse Mapping Table.
#define noir_nsv_rmt_subverted_host			0x00
#define noir_nsv_rmt_noirvisor				0x01
#define noir_nsv_rmt_insecure_guest			0x02
#define noir_nsv_rmt_secure_guest			0x03

struct _noir_cvm_virtual_machine;

// Reverse Mapping Table Entry (Candidate 1 Design)
/*
typedef struct _noir_rmt_entry
{
	union
	{
		struct
		{
			u32 asid;			// Bits	0-31
			u32 reserved;		// Bits	32-63
		};
		u64 value;
	}v1;
	union
	{
		struct
		{
			u64 reserved:24;	// Bits	64-87
			u64 psize:2;		// Bits	88-89
			u64 ownership:6;	// Bits	90-95
			u64 length:32;		// Bits	96-127
		};
		u64 value;
	}v2;
	union
	{
		struct
		{
			u64 shared:1;		// Bit	128
			u64 reserved:11;	// Bits	129-139
			u64 hpa:52;			// Bits	140-191
		};
		u64 value;
	}v3;
	union
	{
		struct
		{
			u64 reserved:12;	// Bits	192-203
			u64 hpa:52;			// Bits	204-255
		};
		u64 value;
	}v4;
}noir_rmt_entry,*noir_rmt_entry_p;
*/

// Reverse Mapping Table Entry (Candidate 2 Design)
typedef struct _noir_rmt_entry
{
	union
	{
		struct
		{
			u64 asid:32;		// Bits	0-31
			u64 reserved:21;	// Bits	32-54
			u64 shared:1;		// Bit	55
			u64 ownership:8;	// Bits	56-63
		};
		u64 value;
	}low;
	union
	{
		struct
		{
			u64 reserved:12;	// Bits	64-75
			u64 guest_pfn:52;	// Bits	76-127
		};
		u64 value;
	}high;
}noir_rmt_entry,*noir_rmt_entry_p;

// Hypervisor Structure
typedef struct _noir_hypervisor
{
#if defined(_vt_core)
	noir_vt_vcpu_p virtual_cpu;
	noir_vt_hvm_p relative_hvm;
#elif defined(_svm_core)
	noir_svm_vcpu_p virtual_cpu;
	noir_svm_hvm_p relative_hvm;
#else
	void* virtual_cpu;
	void* relative_hvm;
#endif
#if !defined(_hv_type1)
	// Only Type-II (Layered) HyperVisor can schedule CVM.
	struct _noir_cvm_virtual_machine *idle_vm;
#endif
	struct
	{
		union
		{
			void* asid_pool;
			void* vpid_pool;
		};
		union
		{
			noir_reslock asid_pool_lock;
			noir_reslock vpid_pool_lock;
		};
		u32 start;
		u32 limit;
	}tlb_tagging;
#if defined(_hv_type1)
	// In Type-I Hypervisor model (i.e: NoirVisor is loaded as an RT driver in UEFI),
	// Layered hypervisor is to be loaded in the guest and subject to register.
	struct
#else
	// In Type-II Hypervisor model (i.e: NoirVisor is loaded as a KM driver in OS),
	// Layered hypervisor is the hypervisor which subverted the system itself.
	union
#endif
	{
		struct
		{
			ulong_ptr base;
			u32 size;
		}hv_image;
		struct
		{
			ulong_ptr base;
			u32 size;
		}layered_hv_image;
	};
	union
	{
		struct
		{
			u64 stealth_msr_hook:1;
			u64 stealth_inline_hook:1;
			u64 cpuid_hv_presence:1;
			u64 disable_patchguard:1;
			u64 nested_virtualization:1;
			u64 kva_shadow_presence:1;
			u64 tlfs_passthrough:1;
			u64 hide_from_pt:1;
			u64 reserved:56;
		};
		u64 value;
	}options;		// Enable certain features.
	union
	{
		struct
		{
			u64 msr_quick_path:1;
			u64 cpuid_quick_path:1;
			u64 hidden_tf:1;
			u64 multi_memmap:1;
			u64 builtin_apic:1;
			u64 builtin_x2apic:1;
			u64 large_page:1;
			u64 huge_page:1;
			u64 reserved:56;
		};
		u64 value;
	}cvm_cap;
	struct
	{
		large_integer support_mask;
		u32 supported_size_max;
		u32 enabled_size_max;
		u32 supported_instructions;
		u32 supported_xss_bits;
		// The following offsets would be calculated.
		u32 offset_avx;
		u32 offset_bndregs;
		u32 offset_bndcsr;
		u32 offset_opmask;
		u32 offset_zmmhi256;
		u32 offset_hi16zmm;
		u32 offset_ptrace;
		u32 offset_mpk;
		u32 offset_cetu;
		u32 offset_cets;
		u32 offset_hdc;
		u32 offset_hwp;
		u32 offset_lwp;
	}xfeat;
	union
	{
		u64 value;
		u8 list[8];
	}host_pat;
	struct
	{
		memory_descriptor hcr3;
		memory_descriptor pdpt;
	}host_memmap;
	struct
	{
#if defined(_hv_type1)
		u16 pm1a;
		u16 pm1b;
#endif
		u16 serial;
	}protected_ports;
	struct
	{
		memory_descriptor table;
		u64 size;
		noir_pushlock lock;
	}rmt;
	u32 cpu_count;
	char vendor_string[13];
	u8 cpu_manuf;
	u8 selected_core;
	// Use a 64-bit notation for an 8-byte alignment.
	u64 reserved[0x10];	// Reserve 128 bytes for Relative HVM.
}noir_hypervisor,*noir_hypervisor_p;

#if !defined(_central_hvm) && !defined(_code_integrity)
typedef struct _noir_hook_page
{
	memory_descriptor orig;
	memory_descriptor hook;
	void* pte_descriptor;
	void* reserved;
}noir_hook_page,*noir_hook_page_p;
extern noir_hook_page_p noir_hook_pages;
extern u32 noir_hook_pages_count;
#endif

// Functions exclusive for Type-I hypervisors
#if defined(_hv_type1)
bool noir_query_pm1_port_address(u16p pm1a,u16p pm1b);
#endif
bool noir_query_serial_port_base(u16p base);

#if defined(_central_hvm)
// Functions from VT Core.
bool nvc_is_vt_supported();
bool nvc_is_ept_supported();
bool nvc_is_vmcs_shadowing_supported();
bool nvc_is_vt_enabled();
u32 nvc_vt_get_avail_vpid();
bool nvc_vt_subvert_system(noir_hypervisor_p hvm);
void nvc_vt_restore_system(noir_hypervisor_p hvm);
// Functions from SVM Core.
bool nvc_is_svm_supported();
bool nvc_is_npt_supported();
bool nvc_is_acnested_svm_supported();
bool nvc_is_svm_disabled();
u32 nvc_svm_get_avail_asid();
bool nvc_svm_subvert_system(noir_hypervisor_p hvm);
void nvc_svm_restore_system(noir_hypervisor_p hvm);
// Central Hypervisor Structure.
void nvc_store_image_info(ulong_ptr* base,u32* size);
noir_hypervisor hvm_t={0};
noir_hypervisor_p hvm_p=&hvm_t;
ulong_ptr system_cr3=0;
ulong_ptr orig_system_call=0;
char virtual_vstr[13]="AuthenticAMD\0";
char virtual_nstr[49]="AMD Ryzen 7 1700 Eight-Core Processor\0";
#else
bool nvc_build_reverse_mapping_table();
void nvc_configure_reverse_mapping(u64 hpa,u64 gpa,u32 asid,bool shared,u8 ownership);
bool nvc_validate_rmt_reassignment(u64p hpa,u64p gpa,u32 pages,u32 asid,bool shared,u8 ownership);
extern noir_hypervisor_p hvm_p;
extern ulong_ptr system_cr3;
extern ulong_ptr orig_system_call;
#endif
#if defined(_mshv_core)
void nvc_svm_reconfigure_npiep_interceptions(void* vcpu);
#elif defined(_vt_core)
#elif defined(_svm_core)
void nvc_svm_reconfigure_npiep_interceptions(noir_svm_vcpu_p vcpu);
#endif
// Functions from MSHV Core.
u32 fastcall nvc_mshv_build_cpuid_handlers();
void fastcall nvc_mshv_teardown_cpuid_handlers();
u64 fastcall nvc_mshv_rdmsr_handler(noir_mshv_vcpu_p vcpu,u32 index);
void fastcall nvc_mshv_wrmsr_handler(noir_mshv_vcpu_p vcpu,u32 index,u64 val);

// Functions from NoirVisor internal debugger.
noir_status noir_configure_serial_port_debugger(u8 port_number,u16 port_base,u32 baudrate);
noir_status noir_dbgport_read(void* buffer,size_t length);
noir_status noir_dbgport_write(void* buffer,size_t length);

// Functions from NoirVisor Emulator.
u8 nvc_emu_try_vmexit_write_memory(noir_gpr_state_p gpr_state,noir_seg_state_p seg_state,u8p instruction,void* operand,size_t *size);

// Miscellaneous
u64 noir_query_enabled_features_in_system();
void noir_system_call(void);

u64 nvc_translate_address_l5(u64 cr3,u64 gva,bool write,bool *fault);
u64 nvc_translate_address_l4(u64 cr3,u64 gva,bool write,bool *fault);
u64 nvc_translate_address_l3(u64 cr3,u32 gva,bool write,bool *fault);
u64 nvc_translate_address_l2(u64 cr3,u32 gva,bool write,bool *fault);

#if defined(_central_hvm)
#define known_vendor_strings	16
// This list is sorted for acceleration through binary search.
char* vendor_string_list[known_vendor_strings]=
{
	" Shanghai ",		// Zhaoxin
	"AMDisbetter!",		// Early ES of AMD-K5
	"AuthenticAMD",		// AMD
	"CentaurHauls",		// Centaur (includes Early VIA and Zhaoxin)
	"CyrixInstead",		// Cyrix
	"GenuineIntel",		// Intel
	"GenuineTMx86",		// Transmeta
	"Geode by NSC",		// National Semiconductor
	"HygonGenuine",		// Hygon (AMD-China-Joint)
	"NexGenDriven",		// NexGen
	"RiseRiseRise",		// Rise
	"SiS SiS SiS ",		// SiS
	"TransmetaCPU",		// Transmeta
	"UMC UMC UMC ",		// UMC
	"VIA VIA VIA ",		// VIA
	"Vortex86 SoC",		// Vortex
};

u8 cpu_manuf_list[known_vendor_strings]=
{
	zhaoxin_processor,
	amd_processor,
	amd_processor,
	centaur_processor,
	cyrix_processor,
	intel_processor,
	trmeta_processor,
	nsc_processor,
	hygon_processor,
	nexgen_processor,
	rise_processor,
	sis_processor,
	trmeta_processor,
	umc_processor,
	via_processor,
	vortex_processor
};
#endif

typedef void (fastcall *noir_mshv_cpuid_handler)
(
 noir_cpuid_general_info_p param
);

typedef u64 (fastcall *noir_mshv_msr_handler)
(
 bool write,
 u64 val
);

// Handlers of MSHV-Core
#if defined(_mshv_cpuid)
noir_hvdata noir_mshv_cpuid_handler*	hvm_cpuid_handlers=null;
noir_hvdata noir_mshv_msr_handler*		hvm_msr_handlers=null;
#else
extern noir_mshv_cpuid_handler*	hvm_cpuid_handlers;
extern noir_mshv_msr_handler*	hvm_msr_handlers;
#endif

// Globle Variables for MSHV-Core
#if defined(_mshv_msr)
noir_hvdata u64v noir_mshv_guest_os_id=0;
noir_hvdata u64v noir_mshv_hypercall_ctrl=0;
#endif