/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

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

#define selector_rplti_mask		0xfff8

typedef union _large_integer
{
	struct
	{
		u32 low;
		u32 high;
	};
	u64 value;
}large_integer,*large_integer_p;

//Processor Facility
typedef struct _segment_register
{
	u16 selector;
	u16 attrib;
	u32 limit;
	u64 base;
}segment_register,*segment_register_p;

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

typedef void (*noir_broadcast_worker)(void* context,u32 processor_id);

void noir_save_processor_state(noir_processor_state_p);
void noir_generic_call(noir_broadcast_worker worker,void* context);
u32 noir_get_processor_count();
u32 noir_get_current_processor();

//Memory Facility
void* noir_alloc_contd_memory(size_t length);
void* noir_alloc_nonpg_memory(size_t length);
void* noir_alloc_paged_memory(size_t length);
void noir_free_contd_memory(void* virtual_address);
void noir_free_nonpg_memory(void* virtual_address);
void noir_free_paged_memory(void* virtual_address);
u64 noir_get_physical_address(void* virtual_address);
void* noir_map_physical_memory(u64 physical_address,size_t length);
void noir_unmap_physical_memory(void* virtual_address,size_t length);
void noir_copy_memory(void* dest,void* src,u32 cch);

//Debugging Facility
void cdecl nv_dprintf(const char* format,...);
void cdecl nv_tracef(const char* format,...);
void cdecl nv_panicf(const char* format,...);