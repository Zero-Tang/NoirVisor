/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the NoirVisor's Basic Development Kits
  function assets based on XPF-Core.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: ./xpf_core/devkits.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <noirhvm.h>
#include <nv_intrin.h>
#include <stdarg.h>

void noir_initialize_list_entry(list_entry_p entry)
{
	entry->next=entry;
	entry->prev=entry;
}

void noir_insert_to_prev(list_entry_p inserter,list_entry_p insertee)
{
	// Initialize the insertee.
	insertee->prev=inserter->prev;
	insertee->next=inserter;
	// Set the insertee surroundings.
	inserter->prev->next=insertee;
	inserter->prev=insertee;
}

void noir_insert_to_next(list_entry_p inserter,list_entry_p insertee)
{
	// Initialize the insertee.
	insertee->next=inserter->next;
	insertee->prev=inserter;
	// Set the insertee surroundings.
	inserter->next->prev=insertee;
	inserter->next=insertee;
}

void noir_remove_list_entry(list_entry_p entry)
{
	// Remove the entry from sides.
	entry->prev->next=entry->next;
	entry->next->prev=entry->prev;
	// Reset this entry.
	entry->prev=entry->next=entry;
}

u32 noir_find_clear_bit(void* bitmap,u32 limit)
{
	// Confirm the index border for bit scanning.
	u32 result=0xffffffff;
#if defined(_amd64)
	u64 *bmp=(u64*)bitmap;
	u32 limit_index=limit>>6;
	if(limit & 0x3F)limit_index++;
#else
	u32 *bmp=(u64*)bitmap;
	u32 limit_index=limit>>5;
	if(limit & 0x1F)limit_index++;
#endif
	// Search the bitmap.
	for(u32 i=0;i<limit_index;i++)
	{
		// Complement the mask before scanning in
		// that we are looking for a cleared bit.
#if defined(_amd64)
		if(noir_bsf64(&result,~bmp[i]))
		{
			result+=(i<<6);
#else
		if(noir_bsf(&result,~bmp[i]))
		{
			result+=(i<<5);
#endif
			break;
		}
	}
	return result;
}

u32 noir_find_set_bit(void* bitmap,u32 limit)
{
	// Confirm the index border for bit scanning.
	u32 result=0xffffffff;
#if defined(_amd64)
	u64 *bmp=(u64*)bitmap;
	u32 limit_index=limit>>6;
	if(limit & 0x3F)limit_index++;
#else
	u32 *bmp=(u64*)bitmap;
	u32 limit_index=limit>>5;
	if(limit & 0x1F)limit_index++;
#endif
	// Search the bitmap.
	for(u32 i=0;i<limit_index;i++)
	{
		// Use bsf instruction to search.
#if defined(_amd64)
		if(noir_bsf64(&result,bmp[i]))
		{
			result+=(i<<6);
#else
		if(noir_bsf(&result,bmp[i]))
		{
			result+=(i<<5);
#endif
			break;
		}
	}
	return result;
}

// Use the third-party static library for internal debugger.
int rpl_vsnprintf(char *str,size_t size,const char *format,va_list args);

i32 cdecl nv_vsnprintf(char *buffer,size_t size,const char* format,va_list arg_list)
{
	return rpl_vsnprintf(buffer,size,format,arg_list);
}

i32 cdecl nv_snprintf(char* buffer,size_t size,const char* format,...)
{
	int retlen;
	va_list arg_list;
	va_start(arg_list,format);
	retlen=rpl_vsnprintf(buffer,size,format,arg_list);
	va_end(arg_list);
	return retlen;
}