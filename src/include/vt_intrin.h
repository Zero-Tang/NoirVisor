/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file defines intrinsics for VMX instructions.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/vt_intrin.h
*/

#include "nvdef.h"

#define ept_single_invd		1
#define ept_global_invd		2

#define vpid_indiva_invd	0
#define vpid_single_invd	1
#define vpid_global_invd	2
#define vpid_sicrgb_invd	3

#define vmx_success			0
#define vmx_fail_valid		1
#define vmx_fail_invalid	2

typedef struct _invept_descriptor
{
	u64 eptp;
	u64 reserved;
}invept_descriptor,*invept_descriptor_p;

typedef struct _invvpid_descriptor
{
	u16 vpid;
	u16 reserved[3];
	u64 linear_address;
}invvpid_descriptor,*invvpid_descriptor_p;

#if defined(_msvc)
//Following intrinsics are defined inside MSVC compiler for x64.
#if defined(_amd64)
#define noir_vt_vmxon		__vmx_on
#define noir_vt_vmptrld		__vmx_vmptrld
#define noir_vt_vmclear		__vmx_vmclear
#define noir_vt_vmread		__vmx_vmread
#define noir_vt_vmwrite		__vmx_vmwrite
#define noir_vt_vmlaunch	__vmx_vmlaunch
#define noir_vt_vmresume	__vmx_vmresume
#define noir_vt_vmread64	__vmx_vmread
#define noir_vt_vmwrite64	__vmx_vmwrite
#else
u8 noir_vt_vmxon(u64* vmxon_pa);
u8 noir_vt_vmptrld(u64* vmcs_pa);
u8 noir_vt_vmclear(u64* vmcs_pa);
u8 noir_vt_vmread(u32 field,u32* value);
u8 noir_vt_vmwrite(u32 field,u32 value);
u8 noir_vt_vmread64(u32 field,u64* value);
u8 noir_vt_vmwrite64(u32 field,u64 value);
u8 noir_vt_vmlaunch();
u8 noir_vt_vmresume();
#endif
#define noir_vt_vmxoff		__vmx_off
#define noir_vt_vmptrst		__vmx_vmptrst
#endif

u8 noir_vt_invept(size_t type,invept_descriptor_p descriptor);
u8 noir_vt_invvpid(size_t type,invvpid_descriptor_p descriptor);
u8 noir_vt_vmcall(u32 function,ulong_ptr context);