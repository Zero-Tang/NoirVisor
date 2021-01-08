/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file is the basic development kit of NoirVisor.
  Do not include definitions related to virtualization in this header.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/nvbdk.h
*/

#include <nvdef.h>

#define page_size				0x1000
#define page_4kb_size			0x1000
#define page_2mb_size			0x200000
#define page_1gb_size			0x40000000

#define page_offset(x)			(x&0xfff)
#define page_4kb_offset(x)		(x&0xfff)
#define page_2mb_offset(x)		(x&0x1fffff)
#define page_1gb_offset(x)		(x&0x3fffffff)

#define page_count(x)			(x>>12)
#define page_4kb_count(x)		(x>>12)
#define page_2mb_count(x)		(x>>21)
#define page_1gb_count(x)		(x>>30)

#define page_mult(x)			(x<<12)
#define page_4kb_mult(x)		(x<<12)
#define page_2mb_mult(x)		(x<<21)
#define page_1gb_mult(x)		(x<<30)

#define selector_rplti_mask		0xfff8

// Classification of CPUID Leaf
#define noir_cpuid_class(x)		(x>>30)
#define noir_cpuid_index(x)		(x&0x3FFFFFFF)

// Classes of Leaves
#define std_leaf_index	0	// CPUID Leaf of Standard	Functions
#define hvm_leaf_index	1	// CPUID Leaf of Hypervisor	Functions
#define ext_leaf_index	2	// CPUID Leaf of Extended	Functions
#define res_leaf_index	3	// CPUID Leaf of Reserved	Functions

// Bases of Classes
#define std_cpuid_base			0x00000000
#define hvm_cpuid_base			0x40000000
#define ext_cpuid_base			0x80000000

// Synthetic MSR Macro
#define noir_is_synthetic_msr(i)	((i>>30)==1)

typedef union _large_integer
{
	struct
	{
		u32 low;
		u32 high;
	};
	u64 value;
}large_integer,*large_integer_p;

// Processor Facility
typedef struct _segment_register
{
	u16 selector;
	u16 attrib;
	u32 limit;
	u64 base;
}segment_register,*segment_register_p;

typedef struct _memory_descriptor
{
	void* virt;
	u64 phys;
}memory_descriptor,*memory_descriptor_p;

typedef struct _noir_processor_state
{
	segment_register cs;
	segment_register ds;
	segment_register es;
	segment_register fs;
	segment_register gs;
	segment_register ss;
	segment_register tr;
	segment_register gdtr;
	segment_register idtr;
	segment_register ldtr;
	ulong_ptr cr0;
	ulong_ptr cr2;
	ulong_ptr cr3;
	ulong_ptr cr4;
#if defined(_amd64)
	u64 cr8;
#endif
	ulong_ptr dr0;
	ulong_ptr dr1;
	ulong_ptr dr2;
	ulong_ptr dr3;
	ulong_ptr dr6;
	ulong_ptr dr7;
	u64 sysenter_cs;
	u64 sysenter_esp;
	u64 sysenter_eip;
	u64 debug_ctrl;
	u64 pat;
	u64 efer;
	u64 star;
	u64 lstar;
	u64 cstar;
	u64 sfmask;
	u64 fsbase;
	u64 gsbase;
	u64 gsswap;
}noir_processor_state,*noir_processor_state_p;

typedef struct _noir_gpr_state
{
	ulong_ptr rax;
	ulong_ptr rcx;
	ulong_ptr rdx;
	ulong_ptr rbx;
	ulong_ptr rsp;
	ulong_ptr rbp;
	ulong_ptr rsi;
	ulong_ptr rdi;
#if defined(_amd64)
	u64 r8;
	u64 r9;
	u64 r10;
	u64 r11;
	u64 r12;
	u64 r13;
	u64 r14;
	u64 r15;
#endif
}noir_gpr_state,*noir_gpr_state_p;

typedef struct _noir_xmm_state
{
	r128 xmm0;
	r128 xmm1;
	r128 xmm2;
	r128 xmm3;
	r128 xmm4;
	r128 xmm5;
	r128 xmm6;
	r128 xmm7;
#if defined(_amd64)
	r128 xmm8;
	r128 xmm9;
	r128 xmm10;
	r128 xmm11;
	r128 xmm12;
	r128 xmm13;
	r128 xmm14;
	r128 xmm15;
#endif
}noir_xmm_state,*noir_xmm_state_p;

typedef struct _noir_cpuid_general_info
{
	u32 eax;
	u32 ebx;
	u32 ecx;
	u32 edx;
}noir_cpuid_general_info,*noir_cpuid_general_info_p;

typedef void (*noir_broadcast_worker)(void* context,u32 processor_id);
typedef i32(cdecl *noir_sorting_comparator)(const void* a,const void*b);

void noir_save_processor_state(noir_processor_state_p);
void noir_generic_call(noir_broadcast_worker worker,void* context);
u32 noir_get_processor_count();
u32 noir_get_current_processor();
u32 noir_get_instruction_length(void* code,bool long_mode);

// Memory Facility
void* noir_alloc_contd_memory(size_t length);
void* noir_alloc_nonpg_memory(size_t length);
void* noir_alloc_paged_memory(size_t length);
void* noir_alloc_2mb_page();
void noir_free_contd_memory(void* virtual_address);
void noir_free_nonpg_memory(void* virtual_address);
void noir_free_paged_memory(void* virtual_address);
void noir_free_2mb_page(void* virtual_address);
u64 noir_get_physical_address(void* virtual_address);
void* noir_map_physical_memory(u64 physical_address,size_t length);
void noir_unmap_physical_memory(void* virtual_address,size_t length);
void* noir_find_virt_by_phys(u64 physical_address);
void noir_copy_memory(void* dest,void* src,u32 cch);

// Debugging Facility
void cdecl nv_dprintf(const char* format,...);
void cdecl nv_tracef(const char* format,...);
void cdecl nv_panicf(const char* format,...);
void cdecl nvci_tracef(const char* format,...);
void cdecl nvci_panicf(const char* format,...);

// Threading Facility
typedef u32 (stdcall *noir_thread_procedure)(void* context);
typedef void* noir_thread;
typedef void* noir_reslock;

noir_thread noir_create_thread(noir_thread_procedure procedure,void* context);
void noir_exit_thread(u32 status);
bool noir_join_thread(noir_thread thread);
bool noir_alert_thread(noir_thread thread);
void noir_sleep(u64 ms);
noir_reslock noir_initialize_reslock();
void noir_finalize_reslock(noir_reslock lock);
void noir_acquire_reslock_shared(noir_reslock lock);
void noir_acquire_reslock_shared_ex(noir_reslock lock);
void noir_acquire_reslock_exclusive(noir_reslock lock);
void noir_release_reslock(noir_reslock lock);

// Miscellaneous
void noir_qsort(void* base,u32 num,u32 width,noir_sorting_comparator comparator);