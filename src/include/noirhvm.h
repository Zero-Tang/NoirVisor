/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file is the central HyperVisor of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/noirhvm.h
*/

#include "nvdef.h"

#include "mshv_hvm.h"
#include "cvm_hvm.h"
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

struct _noir_cvm_virtual_machine;

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
		u32 limit;
	}tlb_tagging;
#endif
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
			u64 reserved:60;
		};
		u64 value;
	}options;		// Enable certain features.
	struct
	{
		large_integer support_mask;
		u32 supported_size_max;
		u32 enabled_size_max;
	}xfeat;
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
noir_hypervisor_p hvm_p=null;
ulong_ptr system_cr3=0;
ulong_ptr orig_system_call=0;
char virtual_vstr[13]="AuthenticAMD\0";
char virtual_nstr[49]="AMD Ryzen 7 1700 Eight-Core Processor\0";
#else
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

// Miscellaneous
u64 noir_query_enabled_features_in_system();
void noir_system_call(void);

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
noir_mshv_cpuid_handler*	hvm_cpuid_handlers=null;
noir_mshv_msr_handler*		hvm_msr_handlers=null;
#else
extern noir_mshv_cpuid_handler*	hvm_cpuid_handlers;
extern noir_mshv_msr_handler*	hvm_msr_handlers;
#endif

// Globle Variables for MSHV-Core
#if defined(_mshv_msr)
u64v noir_mshv_guest_os_id=0;
u64v noir_mshv_hypercall_ctrl=0;
#endif