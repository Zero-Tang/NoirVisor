/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2019, Zero Tang. All rights reserved.

  This file is the CI (Code Integrity) Component.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/ci.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <intrin.h>
#include "ci.h"

bool noir_check_sse42()
{
	u32 ecx;
	noir_cpuid(1,0,null,null,&ecx,null);
	return noir_bt(&ecx,20);
}

// Use CRC32 Castagnoli Algorithm.
u32 noir_crc32_page_std(void* page)
{
	u8* buf=(u8*)page;
	u32 crc=0xffffffff;
	u32 i=0;
	for(;i<page_size;i++)
	{
		u8 crc_index=(u8)(crc^buf[i]&0xff);
        crc=crc32c_table[crc_index]^(crc>>8);
    }
    return crc;
}

// SSE4.2 Accelerated CRC32-Castagnoli
u32 noir_crc32_page_sse(void* page)
{
	u32 crc=0xffffffff;
	u32 i=0;
#if defined(_amd64)
	u64* buf=(u64*)page;
	for(;i<page_size>>3;i++)
		crc=noir_crc32_u64(crc,buf[i]);
#else
	u32* buf=(u32*)page;
	for(;i<page_size>>2;i++)
		crc=noir_crc32_u32(crc,buf[i]);
#endif
	return crc;
}

bool noir_initialize_ci(void* section,u32 size)
{
	const u32 page_num=size>>12;
	// Sections not aligned on page is not allowed.
	if((ulong_ptr)section&0xfff)return false;
	if((ulong_ptr)section&0xfff)return false;
	// Sections smaller than one page is not allowed.
	if(page_num==0)return false;
	// Check supportability of SSE4.2
	if(noir_check_sse42())
		noir_crc32_page_func=noir_crc32_page_sse;
	else
		noir_crc32_page_func=noir_crc32_page_std;
	noir_ci=noir_alloc_nonpg_memory(sizeof(noir_ci_context)+page_num*4);
	if(noir_ci)
	{
		u32 i=0;
		noir_ci->pages=page_num;
		noir_ci->page_crc=(u32*)((ulong_ptr)noir_ci+sizeof(noir_ci_context));
		for(;i<page_num;i++)
		return true;
	}
	return false;
}

void noir_finalize_ci()
{
	noir_free_nonpg_memory(noir_ci);
	noir_ci=null;
}