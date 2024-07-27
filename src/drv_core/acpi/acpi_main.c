/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file is the ACPI driver core for NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /drv_core/acpi/acpi_main.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <nv_intrin.h>
#include <acpi.h>
#include "acpi_main.h"

noir_status nvc_acpi_search_table(u32 signature,acpi_common_description_header_p prev,acpi_common_description_header_p *table)
{
	noir_status st=noir_uninitialized;
	if(acpi_rsdt_ptr)
	{
		st=noir_acpi_no_such_table;
		bool found_prev_instance=(prev==null);
		for(u32 i=0;i<acpi_description_table_count;i++)
		{
			acpi_common_description_header_p table_head=acpi_description_tables[i];
			if(table_head->signature==signature)
			{
				if(found_prev_instance)
				{
					nvd_printf("Found %.4s table at 0x%p!\n",&signature,table_head);
					*table=table_head;
					st=noir_success;
				}
				else if(table_head==prev)
				{
					// Found the previous instance of this table.
					found_prev_instance=true;
					// Locate the next table in further iterations.
				}
				break;
			}
		}
	}
	return st;
}

noir_status nvc_acpi_initialize()
{
	noir_status st=noir_unsuccessful;
	acpi_rsdt_ptr=noir_locate_acpi_rsdt(&acpi_rsdt_len);
	if(acpi_rsdt_ptr)
	{
		acpi_common_description_header_p ptr_head=acpi_rsdt_ptr;
		nvd_printf("RSDT Signature: %.4s\n",&ptr_head->signature);
		if(ptr_head->signature=='TDSX' || ptr_head->signature=='TDSR')
		{
			ulong_ptr list_ptr=(ulong_ptr)&ptr_head[1];
			u32 increment,count=ptr_head->length-sizeof(acpi_common_description_header);
			if(ptr_head->signature=='TDSX')			// Extended System Desciption Table uses 64-bit pointers.
				increment=8;
			else if(ptr_head->signature=='TDSR')	// Root System Description Table uses 32-bit pointers.
				increment=4;
			count/=increment;
			acpi_description_table_count=count;
			nv_dprintf("Number of ACPI Description Tables: %u\n",count);
			st=noir_insufficient_resources;
			acpi_description_tables=noir_alloc_nonpg_memory(sizeof(void*)*count);
			if(acpi_description_tables)
			{
				for(u32 i=0;i<count;i++)
				{
					acpi_common_description_header_p temp;
					u64 pointer;
					noir_movsb(&pointer,(char*)(list_ptr+i*increment),increment);
					temp=noir_map_physical_memory(pointer,sizeof(acpi_common_description_header));
					if(temp)
					{
						u32 size=temp->length;
						noir_unmap_physical_memory(temp,sizeof(acpi_common_description_header));
						acpi_description_tables[i]=noir_map_physical_memory(pointer,size);
						if(acpi_description_tables[i])nv_dprintf("Mapped ACPI Table: 0x%p (%.4s)\n",acpi_description_tables[i],&acpi_description_tables[i]->signature);
					}
				}
				st=noir_success;
			}
		}
		else
			nvd_printf("Unknown Signature for Root System Description Table!");
	}
	return st;
}

void nvc_acpi_finalize()
{
#if !defined(_hv_type1)
	if(acpi_description_tables)
	{
		for(u32 i=0;i<acpi_description_table_count;i++)
		{
			acpi_common_description_header_p table=acpi_description_tables[i];
			if(table)noir_unmap_physical_memory(table,table->length);
		}
		noir_free_nonpg_memory(acpi_description_tables);
	}
	if(acpi_rsdt_ptr)noir_free_nonpg_memory(acpi_rsdt_ptr);
#endif
}