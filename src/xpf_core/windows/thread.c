/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the Multi-Threading facility for NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: ./xpf_core/windows/thread.c
*/

#include <ntifs.h>
#include <windef.h>

NTSYSAPI NTSTATUS NTAPI ZwAlertThread(IN HANDLE ThreadHandle);

// Essential Multi-Threading Facility.
HANDLE noir_create_thread(IN PKSTART_ROUTINE StartRoutine,IN PVOID Context)
{
	OBJECT_ATTRIBUTES oa;
	HANDLE hThread=NULL;
	InitializeObjectAttributes(&oa,NULL,OBJ_KERNEL_HANDLE,NULL,NULL);
	PsCreateSystemThread(&hThread,SYNCHRONIZE,&oa,NULL,NULL,StartRoutine,Context);
	return hThread;
}

void noir_exit_thread(IN NTSTATUS Status)
{
	PsTerminateSystemThread(Status);
}

BOOLEAN noir_join_thread(IN HANDLE ThreadHandle)
{
	NTSTATUS st=ZwWaitForSingleObject(ThreadHandle,FALSE,NULL);
	if(st==STATUS_SUCCESS)
	{
		ZwClose(ThreadHandle);
		return TRUE;
	}
	return FALSE;
}

BOOLEAN noir_alert_thread(IN HANDLE ThreadHandle)
{
	NTSTATUS st=ZwAlertThread(ThreadHandle);
	if(st==STATUS_SUCCESS)
		return TRUE;
	return FALSE;
}

// Sleep
void noir_sleep(IN ULONG64 ms)
{
	LARGE_INTEGER Time;
	Time.QuadPart=ms*(-10000);
	KeDelayExecutionThread(KernelMode,TRUE,&Time);
}

// Resource Lock (R/W Lock)
PERESOURCE noir_initialize_reslock()
{
	PERESOURCE Res=ExAllocatePool(NonPagedPool,sizeof(ERESOURCE));
	if(Res)
	{
		NTSTATUS st=ExInitializeResourceLite(Res);
		if(NT_ERROR(st))
		{
			ExFreePool(Res);
			Res=NULL;
		}
	}
	return Res;
}

void noir_finalize_reslock(IN PERESOURCE Resource)
{
	if(Resource)
	{
		ExDeleteResourceLite(Resource);
		ExFreePool(Resource);
	}
}

void noir_acquire_reslock_shared(IN PERESOURCE Resource)
{
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite(Resource,TRUE);
}

void noir_acquire_reslock_shared_ex(IN PERESOURCE Resource)
{
	KeEnterCriticalRegion();
	ExAcquireSharedStarveExclusive(Resource,TRUE);
}

void noir_acquire_reslock_exclusive(IN PERESOURCE Resource)
{
	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite(Resource,TRUE);
}

void noir_release_reslock(IN PERESOURCE Resource)
{
	ExReleaseResourceLite(Resource);
	KeLeaveCriticalRegion();
}