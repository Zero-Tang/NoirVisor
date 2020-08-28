/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the cpuid Exit Handler of Intel VT-x.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /vt_core/vt_cpuid.c
*/

#include <nvdef.h>
#include <nvstatus.h>
#include <nvbdk.h>
#include <vt_intrin.h>
#include <noirhvm.h>
#include <nv_intrin.h>
#include <ia32.h>
#include "vt_vmcs.h"
#include "vt_def.h"
#include "vt_exit.h"
#include "vt_cpuid.h"

/*
  The CPUID leaf can be divided into following:
  Bits 30-31: Leaf Class.
  Bits 00-29: Leaf Index.

  Class 0: Standard Leaf Function		Range: 0x00000000-0x3FFFFFFF
  Class 1: Hypervisor Leaf Function		Range: 0x40000000-0x7FFFFFFF
  Class 2: Extended Leaf Function		Range: 0x80000000-0xBFFFFFFF
  Class 3: Reserved Leaf Function		Range: 0xC0000000-0xFFFFFFFF
*/

// Default Function Leaf
void static fastcall nvc_vt_default_cpuid_handler(u32 leaf,u32 subleaf,u32* info)
{
	noir_cpuid(leaf,subleaf,&info[0],&info[1],&info[2],&info[3]);
}

/*
  Standard Leaf Functions:

  In this section, handler functions should be regarding
  the standard function leaves. The range starts from 0.
*/
void fastcall nvc_vt_cpuid_std_proc_feature_leaf(u32 leaf,u32 subleaf,u32* info)
{
	noir_cpuid(leaf,subleaf,&info[0],&info[1],&info[2],&info[3]);
	noir_bts(&info[2],ia32_cpuid_vmx);
	noir_bts(&info[2],ia32_cpuid_hv_presence);
}

/*
  Hypervisor Leaf Functions:

  In this section, handler functions should be regarding the
  hypervisor specific functions. At this time, we are supposed
  to conform the Microsoft Hypervisor TLFS. However, NoirVisor,
  by now, is not in conformation.
*/
void fastcall nvc_vt_cpuid_hvm_max_num_vstr_leaf(u32 leaf,u32 subleaf,u32* info)
{
	info[0]=hvm_cpuid_base+hvm_interface_id;
	noir_movsb((u8*)&info[1],"NoirVisor ZT",12);
}

void fastcall nvc_vt_cpuid_hvm_interface_id_leaf(u32 leaf,u32 subleaf,u32* info)
{
	info[0]='0#vH';
	noir_stosd(&info[1],0,3);
}

bool nvc_vt_build_cpuid_handler()
{
	vt_cpuid_handlers=noir_alloc_nonpg_memory(sizeof(void*)*4);
	if(vt_cpuid_handlers)
	{
		u32 std_count,ext_count;
		u32 hvm_count=hvm_cpuid_base+hvm_interface_id;
		noir_cpuid(std_cpuid_base,0,&std_count,null,null,null);
		noir_cpuid(ext_cpuid_base,0,&ext_count,null,null,null);
		std_count-=std_cpuid_base;
		hvm_count-=hvm_cpuid_base;
		ext_count-=ext_cpuid_base;
		hvm_p->relative_hvm->cpuid_max_leaf[std_leaf_index]=++std_count;
		hvm_p->relative_hvm->cpuid_max_leaf[hvm_leaf_index]=++hvm_count;
		hvm_p->relative_hvm->cpuid_max_leaf[ext_leaf_index]=++ext_count;
		vt_cpuid_handlers[std_leaf_index]=noir_alloc_nonpg_memory(sizeof(void*)*std_count);
		vt_cpuid_handlers[hvm_leaf_index]=noir_alloc_nonpg_memory(sizeof(void*)*hvm_count);
		vt_cpuid_handlers[ext_leaf_index]=noir_alloc_nonpg_memory(sizeof(void*)*ext_count);
		if(vt_cpuid_handlers[std_leaf_index] && vt_cpuid_handlers[hvm_leaf_index] && vt_cpuid_handlers[ext_leaf_index])
		{
			// Initialize handlers with default handlers.
			noir_stosp(vt_cpuid_handlers[std_leaf_index],(ulong_ptr)nvc_vt_default_cpuid_handler,std_count);
			noir_stosp(vt_cpuid_handlers[hvm_leaf_index],(ulong_ptr)nvc_vt_default_cpuid_handler,hvm_count);
			noir_stosp(vt_cpuid_handlers[ext_leaf_index],(ulong_ptr)nvc_vt_default_cpuid_handler,ext_count);
			// Default handlers are set. Setup the customized handlers here.
			vt_cpuid_handlers[std_leaf_index][std_proc_feature]=nvc_vt_cpuid_std_proc_feature_leaf;
			vt_cpuid_handlers[hvm_leaf_index][hvm_max_num_vstr]=nvc_vt_cpuid_hvm_max_num_vstr_leaf;
			vt_cpuid_handlers[hvm_leaf_index][hvm_interface_id]=nvc_vt_cpuid_hvm_interface_id_leaf;
			return true;
		}
	}
	return false;
}

void nvc_vt_teardown_cpuid_handler()
{
	if(vt_cpuid_handlers)
	{
		if(vt_cpuid_handlers[std_leaf_index])
			noir_free_nonpg_memory(vt_cpuid_handlers[std_leaf_index]);
		if(vt_cpuid_handlers[hvm_leaf_index])
			noir_free_nonpg_memory(vt_cpuid_handlers[hvm_leaf_index]);
		if(vt_cpuid_handlers[ext_leaf_index])
			noir_free_nonpg_memory(vt_cpuid_handlers[ext_leaf_index]);
		if(vt_cpuid_handlers[res_leaf_index])
			noir_free_nonpg_memory(vt_cpuid_handlers[res_leaf_index]);
		noir_free_nonpg_memory(vt_cpuid_handlers);
		vt_cpuid_handlers=null;
	}
}