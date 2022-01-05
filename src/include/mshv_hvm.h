/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file defines structures for Microsoft's Specification of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/mshv_hvm.h
*/

#include "nvdef.h"

#define noir_mshv_npiep_prevent_sgdt		0
#define noir_mshv_npiep_prevent_sidt		1
#define noir_mshv_npiep_prevent_sldt		2
#define noir_mshv_npiep_prevent_str			3

#define noir_mshv_npiep_prevent_sgdt_bit	0x1
#define noir_mshv_npiep_prevent_sidt_bit	0x2
#define noir_mshv_npiep_prevent_sldt_bit	0x4
#define noir_mshv_npiep_prevent_str_bit		0x8

#define noir_mshv_npiep_prevent_all			0xF

typedef struct _noir_mshv_vcpu
{
	void* root_vcpu;
	u64 npiep_config;
}noir_mshv_vcpu,*noir_mshv_vcpu_p;