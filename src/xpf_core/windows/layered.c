/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

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

void __cdecl NoirCvmTracePrint(const char* Format,...)
{
	LARGE_INTEGER SystemTime,LocalTime;
	TIME_FIELDS Time;
	char Buffer[512];
	PSTR LogBuffer;
	SIZE_T LogSize;
	va_list ArgList;
	va_start(ArgList,Format);
	KeQuerySystemTime(&SystemTime);
	ExSystemTimeToLocalTime(&SystemTime,&LocalTime);
	RtlTimeToTimeFields(&LocalTime,&Time);
	RtlStringCbPrintfExA(Buffer,sizeof(Buffer),&LogBuffer,&LogSize,STRSAFE_FILL_BEHIND_NULL,"[NoirVisor - CVM Tracing]\t| %04d-%02d-%02d %02d:%02d:%02d.%03d | ",Time.Year,Time.Month,Time.Day,Time.Hour,Time.Minute,Time.Second,Time.Milliseconds);
	RtlStringCbVPrintfA(LogBuffer,LogSize,Format,ArgList);
	DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_TRACE_LEVEL,Buffer);
	va_end(ArgList);
}

// Lock the table with at least Shared Access before invoking this function!
PVOID NoirReferenceVirtualMachineByHandleUnsafe(IN CVM_HANDLE Handle,IN ULONG_PTR TableCode)
{
	PULONG_PTR Base=(PULONG_PTR)PAGE_ALIGN(TableCode);	// Get the base of current table.
	ULONG32 Levels=BYTE_OFFSET(TableCode);				// Get the level of current table.
	// Get the index that should be used on referencing this table.
	ULONG_PTR RefIndex=(Handle>>(Levels*HandleTableShiftBits))&(HandleTableCapacity-1);
	// Recurse until we are referencing the lowest level of table.
	if(!Levels)return (PVOID)Base[RefIndex];
	return NoirReferenceVirtualMachineByHandleUnsafe(Handle,Base[RefIndex]);
}

// This function does not acquire lock itself.
// Therefore, acquire the table lock with Exclusive Access prior to invoking this function!
NOIR_STATUS NoirCreateHandle(IN ULONG_PTR TableCode,OUT PCVM_HANDLE Handle,IN PVOID ReferencedEntry)
{
	// Check if this is a multi-level table.
	ULONG Levels=BYTE_OFFSET((PVOID)TableCode);
	if(Levels)
	{
		// This is not the lowest table entry. Search underlying levels of tables.
		PULONG_PTR Base=PAGE_ALIGN((PVOID)TableCode);
		for(ULONG i=0;i<HandleTableCapacity;i++)
		{
			NOIR_STATUS st;
			if(Base[i])
			{
				// Search this available table.
				st=NoirCreateHandle(Base[i],Handle,ReferencedEntry);
				// Check if creation is successful.
				if(st==NOIR_SUCCESS)
				{
					// Adjust the handle value by current table level.
					*Handle+=i<<(HandleTableShiftBits*Levels);
					// Return.
					return st;
				}
				// Creation failed, check the next table entry.
			}
			else
			{
				// At this moment, all already-allocated tables are full.
				// Allocate a new table and insert to table.
				Base[i]=(ULONG_PTR)NoirAllocateNonPagedMemory(PAGE_SIZE);
				if(Base[i]==0)return NOIR_INSUFFICIENT_RESOURCES;
				// Initialize the table.
				RtlZeroMemory((PVOID)Base[i],PAGE_SIZE);
				Base[i]|=Levels-1;	// Set the level of the underlying table.
				// Create handle based on this new table.
				st=NoirCreateHandle(Base[i],Handle,ReferencedEntry);
				// Usually, if creation fails, there is insufficient resources to allocate.
				if(st!=NOIR_SUCCESS)return st;
				// Adjust the handle value by current table level.
				*Handle+=i<<(HandleTableShiftBits*Levels);
				return NOIR_SUCCESS;
			}
		}
		// If the code reaches here, this whole table is full.
		// Invoker of this function should increase the table level itself.
		return NOIR_UNSUCCESSFUL;
	}
	else
	{
		// This is the lowest table entry. Search for an available entry.
		PVOID *Base=(PVOID*)TableCode;
		for(ULONG i=0;i<HandleTableCapacity;i++)
		{
			if(Base[i]==NULL)
			{
				Base[i]=ReferencedEntry;
				*Handle=i;
				return NOIR_SUCCESS;
			}
		}
		// At this moment, the table is out of entry.
		*Handle=0;
		return NOIR_UNSUCCESSFUL;
	}
}

// This function does not acquire lock itself.
// Therefore, acquire the table lock with Exclusive Access prior to invoking this function!
void NoirDeleteHandle(IN CVM_HANDLE Handle,IN ULONG_PTR TableCode)
{
	PULONG_PTR Base=(PULONG_PTR)PAGE_ALIGN(TableCode);	// Get the base of current table.
	ULONG32 Levels=BYTE_OFFSET(TableCode);				// Get the level of current table.
	// Get the index that should be used on referencing this table.
	ULONG_PTR RefIndex=(Handle>>(Levels*HandleTableShiftBits))&(HandleTableCapacity-1);
	// Recurse until we are referencing the lowest level of table.
	if(!Levels)
		Base[RefIndex]=0;
	else
		NoirDeleteHandle(Handle,Base[RefIndex]);
}

NOIR_STATUS NoirCreateVirtualMachine(OUT PCVM_HANDLE VirtualMachine)
{
	PVOID VM=NULL;
	NOIR_STATUS st=nvc_create_vm(&VM,PsGetCurrentProcessId());		// Create a Virtual Machine.
	if(st==NOIR_SUCCESS)
	{
		// If success, create a handle for VM.
		// Acquire the resource lock to add the entry.
		KeEnterCriticalRegion();
		ExAcquireResourceExclusiveLite(&NoirCvmHandleTable.HandleTableLock,TRUE);
		// Create Handle.
		st=NoirCreateHandle(NoirCvmHandleTable.TableCode,VirtualMachine,VM);
		if(st==NOIR_UNSUCCESSFUL)
		{
			// The handle table is full. Increase the table level and retry.
			ULONG Levels=BYTE_OFFSET((PVOID)NoirCvmHandleTable.TableCode);
			ULONG_PTR NewBase=(ULONG_PTR)NoirAllocateNonPagedMemory(PAGE_SIZE);
			if(NewBase)
			{
				NoirCvmTracePrint("Adding the level %u table...\n",++Levels);
				RtlZeroMemory((PVOID)NewBase,PAGE_SIZE);
				// Make the top-level table to be underlain.
				*(PULONG_PTR)NewBase=NoirCvmHandleTable.TableCode;
				NoirCvmHandleTable.TableCode=NewBase|Levels;
				// Retry creation.
				st=NoirCreateHandle(NoirCvmHandleTable.TableCode,VirtualMachine,VM);
			}
			else
			{
				NoirCvmTracePrint("Failed to increase the level of tables!\n");
				st=NOIR_INSUFFICIENT_RESOURCES;
			}
		}
		if(st==NOIR_SUCCESS)	// Increment the handle counter if success.
			NoirCvmHandleTable.HandleCount++;
		if(*VirtualMachine>NoirCvmHandleTable.MaximumHandleValue)
			NoirCvmHandleTable.MaximumHandleValue=*VirtualMachine;
		// Release the resource lock for other accesses.
		ExReleaseResourceLite(&NoirCvmHandleTable.HandleTableLock);
		KeLeaveCriticalRegion();
	}
	return st;
}

NOIR_STATUS NoirReleaseVirtualMachine(IN CVM_HANDLE VirtualMachine)
{
	NOIR_STATUS st=NOIR_UNSUCCESSFUL;
	PVOID VM=NULL;
	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite(&NoirCvmHandleTable.HandleTableLock,TRUE);
	VM=NoirReferenceVirtualMachineByHandleUnsafe(VirtualMachine,NoirCvmHandleTable.TableCode);
	if(VM)
	{
		st=nvc_release_vm(VM);
		if(st==NOIR_SUCCESS)
		{
			NoirCvmHandleTable.HandleCount--;
			NoirDeleteHandle(VirtualMachine,NoirCvmHandleTable.TableCode);
		}
	}
	ExReleaseResourceLite(&NoirCvmHandleTable.HandleTableLock);
	KeLeaveCriticalRegion();
	return st;
}

// Use recursion to traverse as many levels of tables as possible.
void NoirFreeHandleTable(ULONG_PTR TableCode)
{
	PULONG_PTR Base=PAGE_ALIGN((PVOID)TableCode);
	ULONG Levels=BYTE_OFFSET((PVOID)TableCode);
	NoirCvmTracePrint("Freeing %u level(s) of table!\n",Levels);
	if(Levels)
		for(ULONG32 i=0;i<HandleTableCapacity;i++)
			if(Base[i])
				NoirFreeHandleTable(Base[i]);
	NoirFreeNonPagedMemory(Base);
}

HANDLE NoirGetVirtualMachineProcessIdByPointer(IN PVOID VirtualMachine)
{
	return nvc_get_vm_pid(VirtualMachine);
}

void static NoirCreateProcessNotifyRoutine(IN HANDLE ParentId,IN HANDLE ProcessId,IN BOOLEAN Create)
{
	// This notify routine is the key to actively maintain the VM list.
	// Remove any VMs that corresponding process is to be terminated.
	// In this regard can we address the issue of resource leaks.
	if(!Create)		// We have no interest in process creation event.
	{
		NoirCvmTracePrint("Process Id=%d is being terminated...\n",ProcessId);
		KeEnterCriticalRegion();	// Acquire the table resource lock with Exclusive Access.
		ExAcquireResourceExclusiveLite(&NoirCvmHandleTable.HandleTableLock,TRUE);
		for(CVM_HANDLE Handle=0;Handle<=NoirCvmHandleTable.MaximumHandleValue;Handle++)
		{
			PVOID VirtualMachine=NoirReferenceVirtualMachineByHandleUnsafe(Handle,NoirCvmHandleTable.TableCode);
			if(VirtualMachine)
			{
				HANDLE Pid=NoirGetVirtualMachineProcessIdByPointer(VirtualMachine);
				if(Pid==ProcessId)
				{
					NoirCvmTracePrint("PID=%d has created CVM Handle=%d! Terminating VM...\n",Pid,Handle);
					nvc_release_vm(VirtualMachine);
					NoirCvmHandleTable.HandleCount--;
					NoirDeleteHandle(Handle,NoirCvmHandleTable.TableCode);
				}
			}
		}
		NoirCvmTracePrint("Remaining CVM Handles: %d...\n",NoirCvmHandleTable.HandleCount);
		ExReleaseResourceLite(&NoirCvmHandleTable.HandleTableLock);
		KeLeaveCriticalRegion();
	}
}

NTSTATUS NoirFinalizeCvmModule()
{
	NTSTATUS st=PsSetCreateProcessNotifyRoutine(NoirCreateProcessNotifyRoutine,TRUE);
	if(NT_SUCCESS(st))
	{
		st=ExDeleteResourceLite(&NoirCvmHandleTable.HandleTableLock);
		if(NT_SUCCESS(st))NoirFreeHandleTable(NoirCvmHandleTable.TableCode);
	}
	return st;
}

NTSTATUS NoirInitializeCvmModule()
{
	NTSTATUS st=ExInitializeResourceLite(&NoirCvmHandleTable.HandleTableLock);
	if(NT_SUCCESS(st))
	{
		PVOID TableBase=NoirAllocateNonPagedMemory(PAGE_SIZE);
		if(TableBase)
		{
			RtlZeroMemory(TableBase,PAGE_SIZE);
			// At initialization, leave only one level of table.
			NoirCvmHandleTable.TableCode=(ULONG_PTR)TableBase;
			// Register a processor creation callback to recycle VMs.
			st=PsSetCreateProcessNotifyRoutine(NoirCreateProcessNotifyRoutine,FALSE);
			if(NT_ERROR(st))
			{
				NoirFreeNonPagedMemory(TableBase);
				ExDeleteResourceLite(&NoirCvmHandleTable.HandleTableLock);
			}
		}
		else
		{
			ExDeleteResourceLite(&NoirCvmHandleTable.HandleTableLock);
			st=STATUS_INSUFFICIENT_RESOURCES;
		}
	}
	return st;
}

/*
NTSTATUS NoirGetReservedAsidCount()
{
	NTSTATUS st=STATUS_INSUFFICIENT_RESOURCES;
	PKEY_VALUE_PARTIAL_INFORMATION KvPartInf=NoirAllocatePagedMemory(PAGE_SIZE);
	if(KvPartInf)
	{
		HANDLE hKey=NULL;
		UNICODE_STRING uniKeyName=RTL_CONSTANT_STRING(L"\\Registry\\Machine\\Software\\Zero-Tang\\NoirVisor");
		OBJECT_ATTRIBUTES oa;
		InitializeObjectAttributes(&oa,&uniKeyName,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,NULL,NULL);
		st=ZwOpenKey(&hKey,GENERIC_READ,&oa);
		if(NT_SUCCESS(st))
		{
			UNICODE_STRING uniKvName=RTL_CONSTANT_STRING(L"AsidCvmLimit");
			ULONG RetLen=0;
			st=ZwQueryValueKey(hKey,&uniKvName,KeyValuePartialInformation,KvPartInf,PAGE_SIZE,&RetLen);
			if(NT_SUCCESS(st))NoirCvmReservedAsidCount=*(PULONG32)KvPartInf->Data;
			if(NoirCvmReservedAsidCount>=NoirPhysicalAsidLimit-2)
				NoirCvmReservedAsidCount=NoirPhysicalAsidLimit>>1;	// Divide by half if ASID limit is exceeded.
			NoirCvmTracePrint("Reserved ASID Range for CVM: %d\n",NoirCvmReservedAsidCount);
			ZwClose(hKey);
		}
		NoirFreePagedMemory(KvPartInf);
	}
	return st;
}

ULONG NoirAllocateAsid()
{
	BOOLEAN AcquireStatus=ExAcquireResourceExclusiveLite(&NoirCvmAsidBitmapResource,TRUE);
	if(AcquireStatus)
	{
		ULONG Asid=RtlFindClearBitsAndSet(&NoirCvmAsidBitmap,1,0);
		if(Asid==0xFFFFFFFF)Asid=0;		// Check if allocation failed.
		ExReleaseResourceLite(&NoirCvmAsidBitmapResource);
		return Asid;
	}
	return 0;
}

void NoirReleaseAsid(IN ULONG Asid)
{
	BOOLEAN AcquireStatus=ExAcquireResourceExclusiveLite(&NoirCvmAsidBitmapResource,TRUE);
	if(AcquireStatus)
	{
		RtlClearBit(&NoirCvmAsidBitmap,Asid);
		ExReleaseResourceLite(&NoirCvmAsidBitmapResource);
	}
}

NTSTATUS NoirCreateVirtualMachine(OUT PNOIR_CUSTOM_VIRTUAL_MACHINE *VirtualMachine)
{
	// First, Acquire the resource lock for exclusive (write) access.
	NTSTATUS st=STATUS_UNSUCCESSFUL;
	BOOLEAN AcquireStatus=ExAcquireResourceExclusiveLite(&NoirCvmVirtualMachineListResource,TRUE);
	if(AcquireStatus)
	{
		// Allocate Non-Paged Memory for storing VM structure.
		PNOIR_CUSTOM_VIRTUAL_MACHINE VM=NoirAllocateNonPagedMemory(sizeof(NOIR_CUSTOM_VIRTUAL_MACHINE));
		st=VM?STATUS_SUCCESS:STATUS_INSUFFICIENT_RESOURCES;
		if(VM)
		{
			ULONG32 nst;
			// Initialize the VM structure.
			RtlZeroMemory(VM,sizeof(NOIR_CUSTOM_VIRTUAL_MACHINE));
			// Initialize the Resource Lock for vCPUs.
			st=ExInitializeResourceLite(&VM->VirtualProcessor.Lock);
			if(NT_ERROR(st))goto CreationFailure;
			VM->Asid=NoirAllocateAsid();	// Allocate ASID for the virtual machine.
			if(VM->Asid==0)
			{
				// Out of ASIDs, VM will be unable to run with secluded TLBs.
				NoirCvmTracePrint("Failed to assign ASID for VM Creation!\n");
				st=STATUS_INSUFFICIENT_RESOURCES;
				goto CreationFailure;
			}
			// Create the Core-Specific Structure for VM.
			nst=nvc_create_vm(&VM->Core);
			if(nst)
			{
				NoirCvmTracePrint("Failed to create Core Structure for VM! Noir-Status=0x%X\n",nst);
				st=STATUS_UNSUCCESSFUL;
				goto CreationFailure;
			}
			// VM Creation Information. Useful for debugging and tracing.
			KeQuerySystemTime(&VM->CreationTime);
			VM->Process=PsGetCurrentProcess();
			// Finally, insert the VM structure to the list.
			InsertTailList(&NoirCvmIdleVirtualMachine.ActiveVmList,&VM->ActiveVmList);
			goto CreationEnd;
CreationFailure:
			if(VM->Asid)NoirReleaseAsid(VM->Asid);
			NoirFreeNonPagedMemory(VM);
		}
CreationEnd:
		// Creation Complete. Release the resource lock.
		ExReleaseResourceLite(&NoirCvmVirtualMachineListResource);
		*VirtualMachine=VM;
	}
	return st;
}

NTSTATUS NoirTerminateVirtualMachine(IN PNOIR_CUSTOM_VIRTUAL_MACHINE VirtualMachine)
{
	// First, Acquire the resource lock for exclusive (write) access.
	NTSTATUS st=STATUS_UNSUCCESSFUL;
	BOOLEAN AcquireStatus=ExAcquireResourceExclusiveLite(&NoirCvmVirtualMachineListResource,TRUE);
	if(AcquireStatus)
	{
		// Remove the VM from the doubly-linked list.
		RemoveEntryList(&VirtualMachine->ActiveVmList);
		// Release the assigned ASID.
		NoirReleaseAsid(VirtualMachine->Asid);
		// Free the Core-Specific Structure for VM.
		nvc_release_vm(VirtualMachine->Core);
		// Terminate all vCPUs.
		AcquireStatus=ExAcquireResourceExclusiveLite(&VirtualMachine->VirtualProcessor.Lock,TRUE);
		if(AcquireStatus)
		{
			PNOIR_CUSTOM_VIRTUAL_PROCESSOR Vcpu=VirtualMachine->VirtualProcessor.Head;
			while(Vcpu)
			{
				PNOIR_CUSTOM_VIRTUAL_PROCESSOR Tmp=Vcpu->Next;
				NoirFreeNonPagedMemory(Vcpu);
				Vcpu=Tmp;
			}
			ExReleaseResourceLite(&VirtualMachine->VirtualProcessor.Lock);
			ExDeleteResourceLite(&VirtualMachine->VirtualProcessor.Lock);
		}
		// Free the VM structure.
		NoirFreeNonPagedMemory(VirtualMachine);
		// Release Complete. Release the resource lock.
		ExReleaseResourceLite(&NoirCvmVirtualMachineListResource);
		st=STATUS_SUCCESS;
	}
	return st;
}

NTSTATUS NoirCreateVirtualProcessor(IN PNOIR_CUSTOM_VIRTUAL_MACHINE VirtualMachine,OUT PNOIR_CUSTOM_VIRTUAL_PROCESSOR *CustomProcessor)
{
	NTSTATUS st=STATUS_UNSUCCESSFUL;
	// Before creating virtual processor, ensure thread-safety.
	BOOLEAN AcquireStatus=ExAcquireResourceExclusiveLite(&VirtualMachine->VirtualProcessor.Lock,TRUE);
	if(AcquireStatus)
	{
		PNOIR_CUSTOM_VIRTUAL_PROCESSOR Vcpu=NoirAllocateNonPagedMemory(sizeof(NOIR_CUSTOM_VIRTUAL_PROCESSOR));
		st=Vcpu?STATUS_SUCCESS:STATUS_INSUFFICIENT_RESOURCES;
		if(Vcpu)
		{
			// Initialize the vCPU structure.
			RtlZeroMemory(Vcpu,sizeof(NOIR_CUSTOM_VIRTUAL_PROCESSOR));
			// Insert the vCPU to the VM's vCPU list.
			if(VirtualMachine->VirtualProcessor.Head)
				VirtualMachine->VirtualProcessor.Head=VirtualMachine->VirtualProcessor.Tail=Vcpu;
			else
			{
				VirtualMachine->VirtualProcessor.Tail->Next=Vcpu;
				VirtualMachine->VirtualProcessor.Tail=Vcpu;
			}
			VirtualMachine->NumberOfVirtualProcessors++;
		}
		ExReleaseResourceLite(&VirtualMachine->VirtualProcessor.Lock);
	}
	return st;
}

ULONG32 NoirGetAsidCountForNestedVirtualization()
{
	return NoirPhysicalAsidLimit-NoirCvmReservedAsidCount;
}

ULONG32 NoirQueryPhysicalAsidLimit()
{
	return nvc_query_physical_asid_limit(NoirProcessorVendorString);
}

void NoirGetProcessorVendorString(OUT PSTR VendorString)
{
	noir_get_vendor_string(VendorString);
}

NTSTATUS NoirInitializeCvmModule()
{
	NTSTATUS st=NoirGetReservedAsidCount();
	NoirGetProcessorVendorString(NoirProcessorVendorString);
	NoirPhysicalAsidLimit=NoirQueryPhysicalAsidLimit();
	if(NT_ERROR(st))return st;
	st=ExInitializeResourceLite(&NoirCvmVirtualMachineListResource);
	if(NT_ERROR(st))return st;
	st=ExInitializeResourceLite(&NoirCvmAsidBitmapResource);
	if(NT_ERROR(st))return st;
	st=STATUS_INSUFFICIENT_RESOURCES;
	NoirCvmAsidBitmapBuffer=NoirAllocateNonPagedMemory((NoirCvmReservedAsidCount>>3)+1);
	if(NoirCvmAsidBitmapBuffer)
	{
		RtlZeroMemory(NoirCvmAsidBitmapBuffer,(NoirCvmReservedAsidCount>>3)+1);
		RtlInitializeBitMap(&NoirCvmAsidBitmap,NoirCvmAsidBitmapBuffer,NoirCvmReservedAsidCount);
		st=STATUS_SUCCESS;
	}
	if(NT_ERROR(st))
	{
		if(NoirCvmAsidBitmapBuffer)NoirFreeNonPagedMemory(NoirCvmAsidBitmapBuffer);
		ExDeleteResourceLite(&NoirCvmVirtualMachineListResource);
		ExDeleteResourceLite(&NoirCvmAsidBitmapResource);
	}
	InitializeListHead(&NoirCvmIdleVirtualMachine.ActiveVmList);
	return st;
}
*/