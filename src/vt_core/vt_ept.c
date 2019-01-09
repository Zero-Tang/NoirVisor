/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file is the basic driver of Intel EPT.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /vt_core/vt_ept.c
*/

#include <nvdef.h>
#include <nvstatus.h>
#include <nvbdk.h>
#include <vt_intrin.h>
#include <noirhvm.h>
#include <intrin.h>
#include <ia32.h>
#include "vt_vmcs.h"
#include "vt_ept.h"
#include "vt_def.h"

u64 nvc_gpa_to_hpa(noir_ept_manager_p eptm,u64 gpa)
{
	ia32_addr_translator trans;
	u64 hpa;
	trans.value=gpa;
	if(trans.pml4e_offset)
		nv_dprintf("Access to Address higher than 512GB!\n");
	nv_dprintf("GPA=0x%llX to HPA translation requested!\n",gpa);
	nv_dprintf("PML4E Offset=0x%X\t Record=0x%llX\n",trans.pml4e_offset,eptm->eptp.virt[trans.pml4e_offset].value);
	nv_dprintf("PDPTE Offset=0x%X\t Record=0x%llX\n",trans.pdpte_offset,eptm->pdpt.virt[trans.pdpte_offset].value);
	nv_dprintf("PDE Offset=0x%X\t Record=0x%llX\n",trans.pde_offset,eptm->pde.virt[trans.pdpte_offset][trans.pde_offset].value);
	hpa=eptm->pde.virt[trans.pdpte_offset][trans.pde_offset].page_offset<<21|trans.pte_offset<<12|trans.page_offset;
	nv_dprintf("Final Result: HPA=0x%llX\n",hpa);
	return hpa;
}

void nvc_ept_cleanup(noir_ept_manager_p eptm)
{
	if(eptm)
	{
		if(eptm->eptp.virt)
			noir_free_contd_memory(eptm->eptp.virt);
		if(eptm->pdpt.virt)
			noir_free_contd_memory(eptm->pdpt.virt);
		if(eptm->pde.phys)
			noir_free_nonpg_memory(eptm->pde.phys);
		if(eptm->pde.virt)
		{
			u32 i=0;
			for(;i<512;i++)
			{
				if(eptm->pde.virt[i])
					noir_free_contd_memory(eptm->pde.virt[i]);
				else
					break;
			}
			noir_free_nonpg_memory(eptm->pde.virt);
		}
	}
}

/*
  Introduction to Identity map:

  Intel EPT introduces a built-in GPA to HPA technology.
  For NoirVisor, we translate GPA to HPA with GPA=HPA term.
  Essentials: {GPA=HPA} (Important).
  This is key purpose of identity map.
  We use EPT for advanced feature - access filtering.

  NoirVisor's designation supports 512GB physical memory in total.

  Memory Consumption in Paging of each vCPU:
  4KB for 1 PML4E page - only 1 entry is used for mapping 512GB physical memory.
  4KB for 1 PDPTE page - all 512 entries are used for mapping 512*1GB=512GB physical memory.
  2MB for 512 PDE pages - all 262144 entries are used for mapping 512*512*2MB=512GB physical memory
*/
noir_ept_manager_p nvc_ept_build_identity_map()
{
	bool alloc_success=false;
	noir_ept_manager_p eptm=noir_alloc_nonpg_memory(sizeof(noir_ept_manager));
	if(eptm)
	{
		eptm->eptp.virt=noir_alloc_contd_memory(page_size);
		if(eptm->eptp.virt)
		{
			eptm->pdpt.virt=noir_alloc_contd_memory(page_size);
			if(eptm->pdpt.virt)
			{
				eptm->pde.virt=noir_alloc_nonpg_memory(sizeof(void*)*512);
				eptm->pde.phys=noir_alloc_nonpg_memory(page_size);
				if(eptm->pde.virt && eptm->pde.phys)
				{
					u32 i=0;
					alloc_success=true;
					for(;i<512;i++)
					{
						eptm->pde.virt[i]=noir_alloc_contd_memory(page_size);
						if(eptm->pde.virt[i]==null)
						{
							alloc_success=false;
							break;
						}
					}
				}
			}
		}
	}
	if(alloc_success)
	{
		u32 i=0;
		for(;i<512;i++)
		{
			u32 j=0;
			for(;j<512;j++)
			{
				//Build Page-Directory Entries (PDEs)
				eptm->pde.virt[i][j].value=0;
				eptm->pde.virt[i][j].page_offset=(i<<9)+j;
				eptm->pde.virt[i][j].read=1;
				eptm->pde.virt[i][j].write=1;
				eptm->pde.virt[i][j].execute=1;
				eptm->pde.virt[i][j].memory_type=ia32_write_back;
				eptm->pde.virt[i][j].large_pde=1;
			}
			//Build Page-Directory-Pointer-Table Entries (PDPTEs)
			eptm->pde.phys[i]=noir_get_physical_address(eptm->pde.virt[i]);
			eptm->pdpt.virt[i].value=0;
			eptm->pdpt.virt[i].pde_offset=eptm->pde.phys[i]>>12;
			eptm->pdpt.virt[i].read=1;
			eptm->pdpt.virt[i].write=1;
			eptm->pdpt.virt[i].execute=1;
		}
		//Build Page Map Level-4 Entry (PML4E)
		eptm->pdpt.phys=noir_get_physical_address(eptm->pdpt.virt);
		eptm->eptp.virt->value=0;
		eptm->eptp.virt->pdpte_offset=eptm->pdpt.phys>>12;
		eptm->eptp.virt->read=1;
		eptm->eptp.virt->write=1;
		eptm->eptp.virt->execute=1;
		//Make EPT-Pointer (EPTP)
		eptm->eptp.phys.value=noir_get_physical_address(eptm->eptp.virt);
		eptm->eptp.phys.memory_type=ia32_write_back;
		eptm->eptp.phys.walk_length=3;
	}
	else
	{
		nvc_ept_cleanup(eptm);
		nv_dprintf("Allocation Failure! Failed to build EPT paging structure!\n");
		eptm=null;
	}
	return eptm;
}