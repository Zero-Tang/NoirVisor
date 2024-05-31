/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file defines compiler intrinsics.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/nv_intrin.h
*/

#include <nvdef.h>

#ifdef _llvm
#include <intrin.h>
#endif

#if defined(_msvc) || defined(_llvm)
// bit-test instructions
#define noir_bt		_bittest
#define noir_btc	_bittestandcomplement
#define noir_btr	_bittestandreset
#define noir_bts	_bittestandset

// 64-bit bit-test instructions
#if defined(_amd64)
#define noir_bt64	_bittest64
#define noir_btc64	_bittestandcomplement64
#define noir_btr64	_bittestandreset64
#define noir_bts64	_bittestandset64
#endif

// bit-scan instructions
#define noir_bsf	_BitScanForward
#define noir_bsr	_BitScanReverse
#if defined(_amd64)
#define noir_bsf64	_BitScanForward64
#define noir_bsr64	_BitScanReverse64
#endif

// Read Control Register instructions
#define noir_readcr0	__readcr0
#define noir_readcr2	__readcr2
#define noir_readcr3	__readcr3
#define noir_readcr4	__readcr4

// Write Control Register instructions
#define noir_writecr0	__writecr0
void noir_writecr2(ulong_ptr value);
#define noir_writecr3	__writecr3
#define noir_writecr4	__writecr4

#define noir_xsetbv		_xsetbv
#define noir_xgetbv		_xgetbv

// Read/Write CR8 Register instructions(64-bit only)
#if defined(_amd64)
#define noir_readcr8	__readcr8
#define noir_writecr8	__writecr8
#endif

// Read Debug Register instructions
#define noir_readdr0()		__readdr(0)
#define noir_readdr1()		__readdr(1)
#define noir_readdr2()		__readdr(2)
#define noir_readdr3()		__readdr(3)
#define noir_readdr6()		__readdr(6)
#define noir_readdr7()		__readdr(7)

// Write Debug Register instructions
#define noir_writedr0(x)	__writedr(0,x)
#define noir_writedr1(x)	__writedr(1,x)
#define noir_writedr2(x)	__writedr(2,x)
#define noir_writedr3(x)	__writedr(3,x)
#define noir_writedr6(x)	__writedr(6,x)
#define noir_writedr7(x)	__writedr(7,x)

// Read/Write Model Specific Registers
#define noir_rdmsr		__readmsr
#define noir_wrmsr		__writemsr

// Read/Write Descriptor Tables
#pragma pack(1)
typedef struct _descriptor_register
{
	u16 limit;
	ulong_ptr base;
}descriptor_register,*descriptor_register_p;
#pragma pack()

#define noir_sidt		__sidt
#define noir_lidt		__lidt
void noir_sgdt(descriptor_register_p dest);
void noir_lgdt(descriptor_register_p src);
void noir_sldt(u16p dest);
void noir_lldt(u16 src);
void noir_str(u16p dest);
void noir_ltr(u16 src);

// Store-String instructions.
#define noir_stosb		__stosb
#define noir_stosw		__stosw
#define noir_stosd		__stosd
#if defined(_amd64)
#define noir_stosq		__stosq
#define noir_stosp		__stosq
#else
#define noir_stosp		__stosd
#endif

// Move-String instructions.
#define noir_movsb		__movsb
#define noir_movsw		__movsw
#define noir_movsd		__movsd
#if defined(_amd64)
#define noir_movsq		__movsq
#define noir_movsp		__movsq
#else
#define noir_movsp		__movsd
#endif

// I/O instructions
// Repeat-String I/O intrinsics provided by MSVC lack the direction functionality.
#define noir_inb		__inbyte
#define noir_inw		__inword
#define noir_ind		__indword
void noir_insb(u16 port,u8p buffer,size_t count,bool direction);
void noir_insw(u16 port,u16p buffer,size_t count,bool direction);
void noir_insd(u16 port,u32p buffer,size_t count,bool direction);

#define noir_outb		__outbyte
#define noir_outw		__outword
#define noir_outd		__outdword
void noir_outsb(u16 port,u8p buffer,size_t count,bool direction);
void noir_outsw(u16 port,u16p buffer,size_t count,bool direction);
void noir_outsd(u16 port,u32p buffer,size_t count,bool direction);

// Processor TSC instruction
#define noir_rdtsc		__rdtsc
#define noir_rdtscp		__rdtscp

// Memory Barrier instructions.
#define noir_load_fence		_mm_lfence
#define noir_store_fence	_mm_sfence
#define noir_memory_fence	_mm_mfence

// NOP instructions
#define noir_nop		__nop
#define noir_pause		_mm_pause

// Invalidate TLBs
#define noir_invlpg	__invlpg

// Clear/Set RFlags.IF
#define noir_cli	_disable
#define noir_sti	_enable

// Debug-Break & Assertion
#define noir_int3		__debugbreak
#define noir_assert(s)	if(!s)__int2c()

// Generate #UD exception
#define noir_ud2		__ud2

// Invalidate Processor Cache
#define noir_wbinvd		__wbinvd

// Atomic Operations
#define noir_locked_add			_InterlockedAdd
#define noir_locked_inc			_InterlockedIncrement
#define noir_locked_dec			_InterlockedDecrement
#define noir_locked_and			_InterlockedAnd
#define noir_locked_or			_InterlockedOr
#define noir_locked_xor			_InterlockedXor
#define noir_locked_xchg		_InterlockedExchange
#define noir_locked_cmpxchg		_InterlockedCompareExchange
#define noir_locked_bts			_interlockedbittestandset
#define noir_locked_btr			_interlockedbittestandreset

// 64-Bit Atomic Operations
#if defined(_amd64)
#define noir_locked_add64		_InterlockedAdd64
#define noir_locked_inc64		_InterlockedIncrement64
#define noir_locked_dec64		_InterlockedDecrement64
#define noir_locked_and64		_InterlockedAnd64
#define noir_locked_or64		_InterlockedOr64
#define noir_locked_xor64		_InterlockedXor64
#define noir_locked_xchg64		_InterlockedExchange64
#define noir_locked_cmpxchg64	_InterlockedCompareExchange64
#define noir_locked_bts64		_interlockedbittestandset64
#define noir_locked_btr64		_interlockedbittestandreset64
#endif

// FS & GS Operations
#if defined(_amd64)
#define noir_rdvcpu8		__readgsbyte
#define noir_rdvcpu16		__readgsword
#define noir_rdvcpu32		__readgsdword
#define noir_rdvcpu64		__readgsqword
#define noir_rdvcpuptr		__readgsqword
#else
#define noir_rdvcpu8		__readfsbyte
#define noir_rdvcpu16		__readfsword
#define noir_rdvcpu32		__readfsdword
#define noir_rdvcpu64		__readfsqword
#define noir_rdvcpuptr		__readfsqword
#endif
#endif

// Optimization Intrinsics for Branch-Prediction
#if defined(_llvm) || defined(_gcc)
// Clang and GCC supports optimizing branches by intrinsics.
#define likely(x)		__builtin_expect((x),1)
#define unlikely(x)		__builtin_expect((x),0)
#else
#define likely(x)		(x)
#define unlikely(x)		(x)
#endif

#if defined(_llvm)
#define strchr	__builtin_strchr
#define strcmp	__builtin_strcmp
#define strlen	__builtin_strlen
#define strncmp	__builtin_strncmp
#endif

// Unreference Parameters/Variables
#define unref_var(x)	(x=x)
#define unref_param(x)	(x=x)

// The rest are done by inline functions.

// Simplify the cpuid instruction invoking.
void inline noir_cpuid(u32 ia,u32 ic,u32* a,u32* b,u32* c,u32* d)
{
	u32 info[4];
#if defined(_msvc) || defined(_llvm)
	__cpuidex((int*)info,ia,ic);
#endif
	if(a)*a=info[0];
	if(b)*b=info[1];
	if(c)*c=info[2];
	if(d)*d=info[3];
}

u8 inline noir_set_bitmap(void* bitmap,u32 bit_position)
{
	u32* bmp=(u32*)bitmap;
	u32 i=bit_position>>5,j=bit_position&0x1F;
	// bmp[i]|=(1<<j);
	return noir_bts(&bmp[i],j);
}

u8 inline noir_reset_bitmap(void* bitmap,u32 bit_position)
{
	u32* bmp=(u32*)bitmap;
	u32 i=bit_position>>5,j=bit_position&0x1F;
	// bmp[i]&=~(1<<j);
	return noir_btr(&bmp[i],j);
}

u8 inline noir_complement_bitmap(void* bitmap,u32 bit_position)
{
	u32* bmp=(u32*)bitmap;
	u32 i=bit_position>>5,j=bit_position&0x1F;
	return noir_btc(&bmp[i],j);
}