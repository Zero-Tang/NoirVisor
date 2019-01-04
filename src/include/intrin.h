/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file defines compiler intrinsics.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/intrin.h
*/

#include <nvdef.h>

#if defined(_msvc)
//bit-test instructions
#define noir_bt		_bittest
#define noir_btc	_bittestandcomplement
#define noir_btr	_bittestandreset
#define noir_bts	_bittestandset

//64-bit bit-test instructions
#if defined(_amd64)
#define noir_bt64	_bittest64
#define noir_btc64	_bittestandcomplement64
#define noir_btr64	_bittestandreset64
#define noir_bts64	_bittestandset64
#endif

//Read Control Register instructions
#define noir_readcr0	__readcr0
#define noir_readcr2	__readcr2
#define noir_readcr3	__readcr3
#define noir_readcr4	__readcr4

//Write Control Register instructions
#define noir_writecr0	__writecr0
#define noir_writecr3	__writecr3
#define noir_writecr4	__writecr4

//Read/Write CR8 Register instructions(64-bit only)
#if defined(_amd64)
#define noir_readcr8	__readcr8
#define noir_writecr8	__readcr8
#endif

//Read Debug Register instructions
#define noir_readdr0	__readdr(0)
#define noir_readdr1	__readdr(1)
#define noir_readdr2	__readdr(2)
#define noir_readdr3	__readdr(3)
#define noir_readdr6	__readdr(6)
#define noir_readdr7	__readdr(7)

//Write Debug Register instructions
#define noir_writedr0	__writedr(0)
#define noir_writedr1	__writedr(1)
#define noir_writedr2	__writedr(2)
#define noir_writedr3	__writedr(3)
#define noir_writedr6	__writedr(6)
#define noir_writedr7	__writedr(7)

//Read/Write Model Specific Registers
#define noir_rdmsr		__readmsr
#define noir_wrmsr		__writemsr

//Store-String instructions.
#define noir_stosb		__stosb
#define noir_stosw		__stosw
#define noir_stosd		__stosd
#if defined(_amd64)
#define noir_stosq		__stosq
#define noir_stosp		__stosq
#else
#define noir_stosp		__stosd
#endif

//Clear/Set RFlags.IF
#define noir_cli	_disable
#define noir_sti	_enable

//Debug-Break
#define noir_int3	__debugbreak

//Invalidate Processor Cache
#define noir_wbinvd		__wbinvd
#endif

//The rest are done by inline functions.

//Simplify the cpuid instruction invoking.
void inline noir_cpuid(u32 ia,u32 ic,u32* a,u32* b,u32* c,u32* d)
{
	u32 info[4];
#if defined(_msvc)
	__cpuidex(info,ia,ic);
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
	//bmp[i]|=(1<<j);
	return noir_bts(&bmp[i],j);
}

u8 inline noir_reset_bitmap(void* bitmap,u32 bit_position)
{
	u32* bmp=(u32*)bitmap;
	u32 i=bit_position>>5,j=bit_position&0x1F;
	//bmp[i]&=~(1<<j);
	return noir_btr(&bmp[i],j);
}

u8 inline noir_complement_bitmap(void* bitmap,u32 bit_position)
{
	u32* bmp=(u32*)bitmap;
	u32 i=bit_position>>5,j=bit_position&0x1F;
	return noir_btc(&bmp[i],j);
}