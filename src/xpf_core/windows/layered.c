/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2025, Zero Tang. All rights reserved.

  This file is the core engine of layered hypervisor for Windows Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/windows/layered.c
*/

#include <ntddk.h>
#include <windef.h>
#include <stdarg.h>
#include <ntstrsafe.h>
#include "custom_vm.h"

BOOLEAN noir_create_memory_slot(IN PVOID uva,IN SIZE_T length,OUT PMDL *slot)
{
	PMDL pMdl=IoAllocateMdl(uva,(ULONG)length,FALSE,FALSE,NULL);
	if(pMdl)
	{
		__try
		{
			MmProbeAndLockPages(pMdl,KernelMode,IoWriteAccess);
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			IoFreeMdl(pMdl);
			return FALSE;
		}
		*slot=pMdl;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void noir_remove_memory_slot(IN PMDL slot)
{
	if(slot)
	{
		MmUnlockPages(slot);
		IoFreeMdl(slot);
	}
}

BOOLEAN noir_get_pfn_from_memory_slot(IN PMDL slot,IN SIZE_T offset,OUT PULONG64 result)
{
	if(slot)
	{
		ULONG limit=MmGetMdlByteCount(slot);
		if(offset>=limit)
			return FALSE;
		else
		{
			PPFN_NUMBER pfn_list=MmGetMdlPfnArray(slot);
			*result=pfn_list[BYTES_TO_PAGES(offset)];
		}
	}
	return FALSE;
}

void static NoirCreateProcessNotifyRoutine(IN HANDLE ParentId,IN HANDLE ProcessId,IN BOOLEAN Create)
{
	// This notify routine is the key to actively maintain the VM list.
	// Remove any VMs that corresponding process is to be terminated.
	// In this regard can we address the issue of resource leaks.
	if(!Create)		// We have no interest in process creation event.
	{
		// TODO: Call into the VMM to delete VM with `ProcessId`.
	}
}

NTSTATUS NoirFinalizeCvmModule()
{
	return PsSetCreateProcessNotifyRoutine(NoirCreateProcessNotifyRoutine,TRUE);
}

NTSTATUS NoirInitializeCvmModule()
{
	NTSTATUS st=PsSetCreateProcessNotifyRoutine(NoirCreateProcessNotifyRoutine,FALSE);
	if(NT_ERROR(st))
		NoirDebugPrint("Failed to set CreateProcess notification callback! Status=0x%X\n",st);
	// This number is required for building memslots.
	if(*NtBuildNumber<6000)
		noir_maximum_memslot_shift=MAXIMUM_MEMSLOT_SHIFT_WINXP;
	else if(*NtBuildNumber<7600)
		noir_maximum_memslot_shift=MAXIMUM_MEMSLOT_SHIFT_VISTA;
	else
		noir_maximum_memslot_shift=MAXIMUM_MEMSLOT_SHIFT_WIN7;
	return st;
}