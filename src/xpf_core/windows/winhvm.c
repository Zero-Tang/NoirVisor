/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file is the HyperVisor Invoker on Windows Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/windows/winhvm.c
*/

#include <ntddk.h>
#include <windef.h>
#include <nvstatus.h>
#include "winhvm.h"

NTSTATUS NoirBuildHypervisor()
{
	noir_status st=nvc_build_hypervisor();
	switch(st)
	{
		case noir_success:
			return STATUS_SUCCESS;
		case noir_insufficient_resources:
			return STATUS_INSUFFICIENT_RESOURCES;
		default:
			return STATUS_UNSUCCESSFUL;
	}
}

void NoirTeardownHypervisor()
{
	nvc_teardown_hypervisor();
}