/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file is the basic development kit of NoirVisor.
  Do not include definitions related to virtualization in this header.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/nvbdk.h
*/

#include <nvdef.h>

#pragma once

#define page_size				0x1000
#define page_4kb_size			0x1000
#define page_2mb_size			0x200000
#define page_1gb_size			0x40000000
#define page_512gb_size			0x8000000000
#define page_256tb_size			0x1000000000000

#if defined(_amd64)
#define page_shift_diff			9
#define page_table_entries		512
#define page_entry_index(x)		((x)&511)
#else
#define page_shift_diff			10
#define page_table_entries		1024
#define page_entry_index(x)		((x)&1023)
#endif
#define page_shift_diff32		10
#define page_table_entries32	1024
#define page_entry_index32(x)	((x)&1023)
#define page_shift_diff64		9
#define page_table_entries64	512
#define page_entry_index64(x)	((x)&511)

#define page_shift				12
#define page_4kb_shift			12
#define page_2mb_shift			21
#define page_4mb_shift			22
#define page_1gb_shift			30
#define page_4gb_shift			32
#define page_512gb_shift		39
#define page_256tb_shift		48

#define page_offset(x)			((x)&0xfff)
#define page_4kb_offset(x)		((x)&0xfff)
#define page_2mb_offset(x)		((x)&0x1fffff)
#define page_4mb_offset(x)		((x)&0x3fffff)
#define page_1gb_offset(x)		((x)&0x3fffffff)
#define page_512gb_offset(x)	((x)&0x7fffffffff)
#define page_256tb_offset(x)	((x)&0xffffffffffff)

#define page_count(x)			((x)>>page_shift)
#define page_4kb_count(x)		((x)>>page_4kb_shift)
#define page_2mb_count(x)		((x)>>page_2mb_shift)
#define page_1gb_count(x)		((x)>>page_1gb_shift)
#define page_512gb_count(x)		((x)>>page_512gb_shift)
#define page_256tb_count(x)		((x)>>page_256tb_shift)

#define page_mult(x)			((x)<<page_shift)
#define page_4kb_mult(x)		((x)<<page_4kb_shift)
#define page_2mb_mult(x)		((x)<<page_2mb_shift)
#define page_4mb_mult(x)		((x)<<page_4mb_shift)
#define page_1gb_mult(x)		((x)<<page_1gb_shift)
#define page_4gb_mult(x)		((u64)(x)<<page_4gb_shift)
#define page_512gb_mult(x)		((u64)(x)<<page_512gb_shift)

#define bytes_to_pages(x)		(page_count(x)+(page_offset(x)!=0))
#define bytes_to_4kb_pages(x)	(page_4kb_count(x)+(page_4kb_offset(x)!=0))
#define bytes_to_2mb_pages(x)	(page_2mb_count(x)+(page_2mb_offset(x)!=0))
#define bytes_to_1gb_pages(x)	(page_1gb_count(x)+(page_1gb_offset(x)!=0))
#define bytes_to_512gb_pages(x)	(page_512gb_count(x)+(page_512gb_offset(x)!=0))

#define bytes_span_pages(x)			(page_mult(bytes_to_pages(x)))
#define bytes_span_4kb_pages(x)		(page_4kb_mult(bytes_to_4kb_pages(x)))
#define bytes_span_2mb_pages(x)		(page_2mb_mult(bytes_to_2mb_pages(x)))
#define bytes_span_1gb_pages(x)		(page_1gb_mult(bytes_to_1gb_pages(x)))
#define bytes_span_512gb_pages(x)	(page_512gb_mult(bytes_to_512gb_pages(x)))

#if defined(_amd64)
#define page_pae_base(x)		(x&0xffffffffffffffe0)
#define page_base(x)			(x&0xfffffffffffff000)
#define page_4kb_base(x)		(x&0xfffffffffffff000)
#define page_2mb_base(x)		(x&0xffffffffffe00000)
#define page_1gb_base(x)		(x&0xffffffffc0000000)
#define page_512gb_base(x)		(x&0xffffff8000000000)
#define page_256tb_base(x)		(x&0xffff000000000000)
#else
#define page_pae_base(x)		(x&0xffffffe0)
#define page_base(x)			(x&0xfffff000)
#define page_4mb_base(x)		(x&0xffc00000)
#endif

#define selector_rplti_mask		0xfff8

#define field_offset(t,f)		((size_t)(&((t*)0)->f))

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

typedef struct _list_entry
{
	struct _list_entry *next;
	struct _list_entry *prev;
}list_entry,*list_entry_p;

typedef struct _avl_node
{
	struct _avl_node *left;
	struct _avl_node *right;
	i64 height;
}avl_node,*avl_node_p;

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

typedef struct _noir_amd64_interrupt_frame
{
	u64 return_rip;
	u16 return_cs;
	u16 reserved1[3];
	u64 return_rflags;
	u64 return_rsp;
	u16 return_ss;
	u16 reserved2[3];
}noir_amd64_interrupt_frame,*noir_amd64_interrupt_frame_p;

typedef struct _noir_amd64_interrupt_frame_with_error_code
{
	u32 error_code;
	u32 reserved0;
	u64 return_rip;
	u16 return_cs;
	u16 reserved1[3];
	u64 return_rflags;
	u64 return_rsp;
	u16 return_ss;
	u16 reserved2[3];
}noir_amd64_interrupt_frame_with_error_code,*noir_amd64_interrupt_frame_with_error_code_p;

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

typedef struct _noir_ymm_state
{
	r256 ymm0;
	r256 ymm1;
	r256 ymm2;
	r256 ymm3;
	r256 ymm4;
	r256 ymm5;
	r256 ymm6;
	r256 ymm7;
#if defined(_amd64)
	r256 ymm8;
	r256 ymm9;
	r256 ymm10;
	r256 ymm11;
	r256 ymm12;
	r256 ymm13;
	r256 ymm14;
	r256 ymm15;
#endif
}noir_ymm_state,*noir_ymm_state_p;

// 512-byte region of fxsave instruction.
// Including FPU, x87, XMM.
typedef struct _noir_fx_state
{
	struct
	{
		u16 fcw;
		u16 fsw;
		u8 ftw;
		u8 reserved;
		u16 fop;
		u32 fip;
		u16 fcs;
		u16 reserved1;
#if defined(_amd64)
		u64 fdp;
#else
		u32 fdp;
		u16 fds;
		u16 reserved2;
#endif
		u32 mxcsr;
		u32 mxcsr_mask;
	}fpu;
	struct
	{
		u64 mm0;		// st0
		u64 reserved0;
		u64 mm1;		// st1
		u64 reserved1;
		u64 mm2;		// st2
		u64 reserved2;
		u64 mm3;		// st3
		u64 reserved3;
		u64 mm4;		// st4
		u64 reserved4;
		u64 mm5;		// st5
		u64 reserved5;
		u64 mm6;		// st6
		u64 reserved6;
		u64 mm7;		// st7
		u64 reserved7;
	}x87;
	noir_xmm_state sse;
#if defined(_amd64)
	u64 reserved[6];
#else
	u64 reserved[22];
#endif
	// If available[0]==0, FFXSR is disabled.
	u64 available[6];
}noir_fx_state,*noir_fx_state_p;

typedef struct _noir_xsave_header
{
	u64 xsave_bv;
	u64 xcomp_bv;
	u64 reserved[6];
}noir_xsave_header,*noir_xsave_header_p;

typedef union _noir_segment_attributes
{
	struct
	{
		u16 type:4;
		u16 system:1;
		u16 dpl:2;
		u16 present:1;
		u16 limit_hi:4;
		u16 avl:1;
		u16 long_mode:1;
		u16 default_big:1;
		u16 granularity:1;
	};
	u16 value;
}noir_segment_attributes,*noir_segment_attributes_p;

#pragma pack(1)
typedef struct _noir_segment_descriptor
{
	u16 limit_lo;
	u16 base_lo;
	u8 base_mid1;
	noir_segment_attributes attributes;
	u8 base_mid2;
	u32 base_hi;
	u32 reserved;
}noir_segment_descriptor,*noir_segment_descriptor_p;
#pragma pack()

#pragma pack(1)
typedef struct _noir_gate_descriptor
{
	u16 offset_lo;
	u16 selector;
	union
	{
		struct
		{
			u16 ist:3;
			u16 reserved:5;
			u16 type:5;
			u16 dpl:2;
			u16 present:1;
		};
		u16 value;
	}attrib;
	u16 offset_mid;
	u32 offset_hi;
	u32 reserved;
}noir_gate_descriptor,*noir_gate_descriptor_p;
#pragma pack()

#pragma pack(1)
typedef struct _noir_tss64
{
	u32 reserved0;
	u64 rsp0;
	u64 rsp1;
	u64 rsp2;
	u64 reserved1;
	u64 ist1;
	u64 ist2;
	u64 ist3;
	u64 ist4;
	u64 ist5;
	u64 ist6;
	u64 ist7;
	u64 reserved2;
	u16 reserved3;
	u16 iomap_base;
}noir_tss64,*noir_tss64_p;
#pragma pack()

typedef struct _noir_cpuid_general_info
{
	u32 eax;
	u32 ebx;
	u32 ecx;
	u32 edx;
}noir_cpuid_general_info,*noir_cpuid_general_info_p;

typedef struct _noir_npiep_operand
{
	struct
	{
		u64 reg:4;
		u64 reserved:59;
		u64 use_register:1;
	}flags;
	u64 memory_address;
}noir_npiep_operand,*noir_npiep_operand_p;

typedef union _noir_addr32_translator
{
	struct
	{
		u32 offset:12;
		u32 pte:10;
		u32 pde:10;
	};
	u32 pointer;
}noir_addr32_translator,*noir_addr32_translator_p;

typedef union _noir_addr64_translator
{
	struct
	{
		u64 offset:12;
		u64 pte:9;
		u64 pde:9;
		u64 pdpte:9;
		u64 pml4e:9;
		u64 pml5e:9;
		u64 sign_extend:7;
	};
	u64 pointer;
}noir_addr64_translator,*noir_addr64_translator_p;

typedef union _noir_page_fault_error_code
{
	struct
	{
		u32 present:1;
		u32 write:1;
		u32 user:1;
		u32 reserved_bits:1;
		u32 execute:1;
		u32 protection_key:1;
		u32 shadow_stack:1;
		u32 reserved:25;
	};
	u32 value;
}noir_page_fault_error_code,*noir_page_fault_error_code_p;

typedef union _noir_paging64_general_entry
{
	struct
	{
		u64 present:1;
		u64 write:1;
		u64 user:1;
		u64 pwt:1;
		u64 pcd:1;
		u64 accessed:1;
		u64 dirty:1;
		u64 psize:1;
		u64 global:1;
		u64 available1:3;
		u64 base:40;
		u64 available2:7;
		u64 page_key:4;
		u64 no_execute:1;
	};
	u64 value;
}noir_paging64_general_entry,*noir_paging64_general_entry_p;

typedef union _noir_paging32_general_entry
{
	struct
	{
		u32 present:1;
		u32 write:1;
		u32 user:1;
		u32 pwt:1;
		u32 pcd:1;
		u32 accessed:1;
		u32 dirty:1;
		u32 pat:1;
		u32 global:1;
		u32 available:1;
		u32 base:20;
	}pte;
	struct
	{
		u32 present:1;
		u32 write:1;
		u32 user:1;
		u32 pwt:1;
		u32 pcd:1;
		u32 accessed:1;
		u32 ignored0:1;
		u32 psize:1;
		u32 ignored1:1;
		u32 avl:3;
		u32 pte_base:20;
	}pde;
	struct
	{
		u32 present:1;
		u32 write:1;
		u32 user:1;
		u32 pwt:1;
		u32 pcd:1;
		u32 accessed:1;
		u32 dirty:1;
		u32 psize:1;
		u32 global:1;
		u32 avl:3;
		u32 pat:1;
		u32 base_hi:8;
		u32 reserved:1;
		u32 base_lo:10;
	}large_pde;
	u32 value;
}noir_paging32_general_entry,*noir_paging32_general_entry_p;

typedef void (*noir_mmio_handler)
(
 bool direction,
 u64 address,
 u64 size,
 u64p value,
 void* context
);

typedef struct _noir_mmio_region
{
	noir_mmio_handler handler;
	void* context;
	u64 phys;
	u64 size;
}noir_mmio_region,*noir_mmio_region_p;

typedef void (*noir_pio_handler)
(
 bool direction,
 u16 port,
 u16 size,
 u32p value,
 void* context
);

typedef struct _noir_pio_region
{
	noir_pio_handler handler;
	void* context;
	u16 port;
	u16 size;
}noir_pio_region,*noir_pio_region_p;

typedef void (*noir_broadcast_worker)(void* context,u32 processor_id);

void noir_save_processor_state(noir_processor_state_p state);
u16 noir_get_segment_attributes(ulong_ptr gdt_base,u16 selector);
void noir_generic_call(noir_broadcast_worker worker,void* context);
void* noir_get_host_idt_base(u32 processor_number);
u32 noir_get_processor_count();
u32 noir_get_current_processor();
u32 noir_get_instruction_length(void* code,bool long_mode);
u32 noir_get_instruction_length_ex(void* code,u8 bits);
u32 noir_disasm_instruction(void* code,char* mnemonic,size_t mnemonic_length,u8 bits,u64 virtual_address);

void* noir_get_host_gdt_base(u32 processor_number);
void* noir_get_host_idt_base(u32 processor_number);

bool noir_is_under_hvm();

// Integer Facility
u64 u64_max(const u64 n,...);
i64 i64_max(const u64 n,...);

// Doubly-Linked List Facility
void noir_initialize_list_entry(list_entry_p entry);
void noir_insert_to_prev(list_entry_p inserter,list_entry_p insertee);
void noir_insert_to_next(list_entry_p inserter,list_entry_p insertee);
void noir_remove_list_entry(list_entry_p entry);

// AVL-Tree Facility
typedef i32(cdecl *noir_sorting_comparator)(const void* a,const void*b);
typedef i32(cdecl *noir_bst_search_comparator)(avl_node_p node,const void* item);

avl_node_p noir_insert_avl_node(avl_node_p parent,avl_node_p node,noir_sorting_comparator compare_fn);
avl_node_p noir_search_avl_node(avl_node_p root,const void* item,noir_bst_search_comparator compare_fn);

// Bitmap Facility
u32 noir_find_clear_bit(void* bitmap,u32 limit);
u32 noir_find_set_bit(void* bitmap,u32 limit);

// Memory-Mapped I/O Facility
u8 noir_mmio_read8(u64 ptr);
u16 noir_mmio_read16(u64 ptr);
u32 noir_mmio_read32(u64 ptr);
u64 noir_mmio_read64(u64 ptr);
void noir_mmio_write8(u64 ptr,u8 val);
void noir_mmio_write16(u64 ptr,u16 val);
void noir_mmio_write32(u64 ptr,u32 val);
void noir_mmio_write64(u64 ptr,u64 val);

// Processor Extension Register Context Instructions
// Use when switching from/to customizable VMs.
void noir_fxsave(noir_fx_state_p state);
void noir_fxrestore(noir_fx_state_p state);
void noir_ffxsave(noir_fx_state_p state);		// Ignore the XMM registers.
void noir_ffxrestore(noir_fx_state_p state);	// Ignore the XMM registers.
void noir_xmmsave(noir_xmm_state_p state);
void noir_xmmrestore(noir_xmm_state_p state);
void noir_ymmsave(noir_ymm_state_p state);
void noir_ymmrestore(noir_ymm_state_p state);
void noir_xsave(void* state,u64 bv_mask);
void noir_xrestore(void* state,u64 bv_mask);
void noir_xsaves(void* state,u64 bv_mask);
void noir_xrestores(void* state,u64 bv_mask);

// Memory Facility
typedef void (*noir_physical_range_callback)
(
	u64 start,
	u64 length,
	void* context
);

void* noir_alloc_contd_memory(size_t length);
void* noir_alloc_nonpg_memory(size_t length);
void* noir_alloc_paged_memory(size_t length);
void* noir_alloc_2mb_page();
void noir_free_contd_memory(void* virtual_address,size_t length);
void noir_free_nonpg_memory(void* virtual_address);
void noir_free_paged_memory(void* virtual_address);
void noir_free_2mb_page(void* virtual_address);
u64 noir_get_user_physical_address(void* virtual_address);
u64 noir_get_physical_address(void* virtual_address);
u64 noir_get_current_process_cr3();
void* noir_lock_pages(void* virt,size_t bytes,u64p phys);
void noir_unlock_pages(void* locker);
void noir_get_locked_range(void* locker,void** virt,u32p bytes);
void* noir_map_physical_memory(u64 physical_address,size_t length);
void* noir_map_uncached_memory(u64 physical_address,size_t length);
void noir_unmap_physical_memory(void* virtual_address,size_t length);
void* noir_find_virt_by_phys(u64 physical_address);
bool noir_query_page_attributes(void* virtual_address,bool *valid,bool *locked,bool *large_page);
void noir_copy_memory(void* dest,void* src,u32 cch);
void noir_enum_physical_memory_ranges(noir_physical_range_callback callback_routine,void* context);

// String Facility
i32 cdecl nv_snprintf(char* buffer,size_t limit,const char* format,...);

// Debugging Facility
void cdecl nv_dprintf(const char* format,...);
void cdecl nv_tracef(const char* format,...);
void cdecl nv_panicf(const char* format,...);
void cdecl nvci_tracef(const char* format,...);
void cdecl nvci_panicf(const char* format,...);
void cdecl nv_dprintf_unprefixed(const char* format,...);
void cdecl nv_dprintf2(bool datetime,bool proc_id,const char* func_name,const char* format,...);

void cdecl nvd_printf_fn(const char* src_file,const u32 src_ln,const char* format,...);
void cdecl nvd_printf_raw(const char* format,...);

#define nvd_printf(fmt,...)		nvd_printf_fn(__FILE__,__LINE__,fmt,##__VA_ARGS__)

void cdecl nvd_panicf(const char* format,...);

void noir_hbreak(void);

// Threading Facility
typedef u32 (stdcall *noir_thread_procedure)(void* context);
typedef void* noir_thread;
typedef void* noir_reslock;
typedef ulong_ptr noir_pushlock;

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
void noir_acquire_pushlock_shared(noir_pushlock *lock);
void noir_acquire_pushlock_exclusive(noir_pushlock *lock);
void noir_release_pushlock_shared(noir_pushlock *lock);
void noir_release_pushlock_exclusive(noir_pushlock *lock);

// Crypto Facility
typedef u32 (stdcall *noir_crc32_page_func)(void* page);

extern noir_crc32_page_func noir_crc32_page;

void noir_aes128_expand_key(u8p key,bool expand_encrypt,u8p expanded_keys);
void noir_aes128_encrypt_pages(void* page_base,u8p expanded_keys,u64 pages,u8p key);
void noir_aes128_decrypt_pages(void* page_base,u8p expanded_keys,u64 pages,u8p key);

// Miscellaneous
void noir_qsort(void* base,u32 num,u32 width,noir_sorting_comparator comparator);
u64 noir_get_system_time();