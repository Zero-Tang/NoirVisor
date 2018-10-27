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

//Debugging Facility
void cdecl nv_dprintf(const char* format,...);
void cdecl nv_tracef(const char* format,...);
void cdecl nv_panicf(const char* format,...);