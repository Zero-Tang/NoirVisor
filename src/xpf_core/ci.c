/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2019, Zero Tang. All rights reserved.

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

// Use CRC32 Castagnoli Algorithm.
u32 static noir_crc32_page_std(ulong_ptr page)
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

u32 static stdcall noir_ci_enforcement_worker(void* context)
{
	noir_ci_context_p ncie=(noir_ci_context_p)context;
	while(noir_locked_cmpxchg(&ncie->signal,1,1)==0)
	{
		u32 i=ncie->selected++;
		ulong_ptr page=ncie->base+(i<<12);
		u32 crc=noir_crc32_page(page);
		nv_tracef("[CI] Page 0x%p scanned. CRC32C=0x%08X\n",page,crc);
		if(crc!=ncie->page_crc[i])
			nv_panicf("[CI] CI detected corruption in Page 0x%p!\n",page);
		if(ncie->selected==ncie->pages)
			ncie->selected=0;
		noir_sleep(5000);
	}
	noir_exit_thread(0);
	return 0;
}

bool noir_initialize_ci(void* section,u32 size)
{
	u32 page_num=size>>12;
	if(size & 0xfff)page_num++;
	// Check supportability of SSE4.2
	if(noir_check_sse42())
		noir_crc32_page=noir_crc32_page_sse;
	else
		noir_crc32_page=noir_crc32_page_std;
	noir_ci=noir_alloc_nonpg_memory(sizeof(noir_ci_context)+page_num*4);
	if(noir_ci)
	{
		u32 i=0;
		noir_ci->pages=page_num;
		noir_ci->base=(ulong_ptr)section;
		for(;i<page_num;i++)
			noir_ci->page_crc[i]=noir_crc32_page(noir_ci->base+(i<<12));
		noir_ci->ci_thread=noir_create_thread(noir_ci_enforcement_worker,noir_ci);
		if(noir_ci->ci_thread)
			return true;
		else
			noir_free_nonpg_memory(noir_ci);
	}
	return false;
}

void noir_finalize_ci()
{
	noir_locked_inc(&noir_ci->signal);
	noir_join_thread(noir_ci->ci_thread);
	noir_free_nonpg_memory(noir_ci);
	noir_ci=null;
}