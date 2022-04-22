/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

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

NTSTATUS NoirGetPageInformation(IN PVOID PageAddress,OUT PMEMORY_WORKING_SET_EX_BLOCK Information)
{
	NTSTATUS st;
	SIZE_T RetLen;
	MEMORY_WORKING_SET_EX_INFORMATION WorkSetExInfo;
	WorkSetExInfo.VirtualAddress=PageAddress;
	WorkSetExInfo.VirtualAttributes.Value=0;
	st=ZwQueryVirtualMemory(ZwCurrentProcess(),NULL,MemoryWorkingSetExInformation,&WorkSetExInfo,sizeof(WorkSetExInfo),&RetLen);
	if(NT_SUCCESS(st))
		Information->Value=WorkSetExInfo.VirtualAttributes.Value;
	else
		NoirCvmTracePrint("Failed to query virtual memory for 0x%p! Status=0x%X!\n",PageAddress,st);
	return st;
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
NOIR_STATUS NoirCreateHandleUnsafe(IN ULONG_PTR TableCode,OUT PCVM_HANDLE Handle,IN PVOID ReferencedEntry)
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
				st=NoirCreateHandleUnsafe(Base[i],Handle,ReferencedEntry);
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
				st=NoirCreateHandleUnsafe(Base[i],Handle,ReferencedEntry);
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

// Note that this function acquires the lock, so do what you should do to circumvent deadlocking.
NOIR_STATUS NoirCreateHandle(OUT PCVM_HANDLE Handle,IN PVOID ReferencedEntry)
{
	NOIR_STATUS st=NOIR_UNSUCCESSFUL;
	// Acquire the resource lock in order to add the entry
	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite(&NoirCvmHandleTable.HandleTableLock,TRUE);
	// Create Handle.
	st=NoirCreateHandleUnsafe(NoirCvmHandleTable.TableCode,Handle,ReferencedEntry);
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
			st=NoirCreateHandleUnsafe(NoirCvmHandleTable.TableCode,Handle,ReferencedEntry);
		}
		else
		{
			NoirCvmTracePrint("Failed to increase the level of tables!\n");
			st=NOIR_INSUFFICIENT_RESOURCES;
		}
	}
	if(st==NOIR_SUCCESS)	// Increment the handle counter if success.
	{
		NoirCvmHandleTable.HandleCount++;
		NoirCvmTracePrint("New VM is created successfully! Handle=0x%p\t Object=0x%p\n",*Handle,ReferencedEntry);
	}
	if(*Handle>NoirCvmHandleTable.MaximumHandleValue)
		NoirCvmHandleTable.MaximumHandleValue=*Handle;
	// Release the resource lock for other accesses.
	ExReleaseResourceLite(&NoirCvmHandleTable.HandleTableLock);
	KeLeaveCriticalRegion();
	// Finally, return the status.
	return st;
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
	if(st==NOIR_SUCCESS)st=NoirCreateHandle(VirtualMachine,VM);		// If success, create a handle for VM.
	return st;
}

NOIR_STATUS NoirCreateVirtualMachineEx(OUT PCVM_HANDLE VirtualMachine,IN ULONG32 Properties,IN ULONG32 NumberOfAsid)
{
	PVOID VM=NULL;
	NOIR_STATUS st=nvc_create_vm_ex(&VM,PsGetCurrentProcessId(),Properties,NumberOfAsid);
	if(st==NOIR_SUCCESS)st=NoirCreateHandle(VirtualMachine,VM);
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

NOIR_STATUS NoirQueryGpaAccessingBitmap(IN CVM_HANDLE VirtualMachine,IN ULONG64 GpaStart,IN ULONG32 NumberOfPages,OUT PVOID Bitmap,IN ULONG32 BitmapSize)
{
	NOIR_STATUS st=NOIR_UNSUCCESSFUL;
	PVOID VM=NULL;
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite(&NoirCvmHandleTable.HandleTableLock,TRUE);
	VM=NoirReferenceVirtualMachineByHandleUnsafe(VirtualMachine,NoirCvmHandleTable.TableCode);
	if(VM)st=nvc_query_gpa_accessing_bitmap(VM,GpaStart,NumberOfPages,Bitmap,BitmapSize);
	ExReleaseResourceLite(&NoirCvmHandleTable.HandleTableLock);
	KeLeaveCriticalRegion();
	return st;
}

NOIR_STATUS NoirClearGpaAccessingBits(IN CVM_HANDLE VirtualMachine,IN ULONG64 GpaStart,IN ULONG32 NumberOfPages)
{
	NOIR_STATUS st=NOIR_UNSUCCESSFUL;
	PVOID VM=NULL;
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite(&NoirCvmHandleTable.HandleTableLock,TRUE);
	VM=NoirReferenceVirtualMachineByHandleUnsafe(VirtualMachine,NoirCvmHandleTable.TableCode);
	if(VM)st=nvc_clear_gpa_accessing_bits(VM,GpaStart,NumberOfPages);
	ExReleaseResourceLite(&NoirCvmHandleTable.HandleTableLock);
	KeLeaveCriticalRegion();
	return st;
}

NOIR_STATUS NoirSetMapping(IN CVM_HANDLE VirtualMachine,IN PNOIR_ADDRESS_MAPPING MappingInformation)
{
	NOIR_STATUS st=NOIR_UNSUCCESSFUL;
	PVOID VM=NULL;
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite(&NoirCvmHandleTable.HandleTableLock,TRUE);
	VM=NoirReferenceVirtualMachineByHandleUnsafe(VirtualMachine,NoirCvmHandleTable.TableCode);
	if(VM)st=nvc_set_mapping(VM,MappingInformation);
	ExReleaseResourceLite(&NoirCvmHandleTable.HandleTableLock);
	KeLeaveCriticalRegion();
	return st;
}

NOIR_STATUS NoirViewVirtualProcessorRegisters(IN CVM_HANDLE VirtualMachine,IN ULONG32 VpIndex,IN NOIR_CVM_REGISTER_TYPE RegisterType,OUT PVOID Buffer,IN ULONG32 BufferSize)
{
	NOIR_STATUS st=NOIR_UNSUCCESSFUL;
	PVOID VM=NULL;
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite(&NoirCvmHandleTable.HandleTableLock,TRUE);
	VM=NoirReferenceVirtualMachineByHandleUnsafe(VirtualMachine,NoirCvmHandleTable.TableCode);
	if(VM)
	{
		PVOID VP=nvc_reference_vcpu(VM,VpIndex);
		st=VP==NULL?NOIR_VCPU_NOT_EXIST:nvc_view_vcpu_registers(VP,RegisterType,Buffer,BufferSize);
	}
	ExReleaseResourceLite(&NoirCvmHandleTable.HandleTableLock);
	KeLeaveCriticalRegion();
	return st;
}

NOIR_STATUS NoirEditVirtualProcessorRegisters(IN CVM_HANDLE VirtualMachine,IN ULONG32 VpIndex,IN NOIR_CVM_REGISTER_TYPE RegisterType,IN PVOID Buffer,IN ULONG32 BufferSize)
{
	NOIR_STATUS st=NOIR_UNSUCCESSFUL;
	PVOID VM=NULL;
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite(&NoirCvmHandleTable.HandleTableLock,TRUE);
	VM=NoirReferenceVirtualMachineByHandleUnsafe(VirtualMachine,NoirCvmHandleTable.TableCode);
	if(VM)
	{
		PVOID VP=nvc_reference_vcpu(VM,VpIndex);
		st=VP==NULL?NOIR_VCPU_NOT_EXIST:nvc_edit_vcpu_registers(VP,RegisterType,Buffer,BufferSize);
	}
	ExReleaseResourceLite(&NoirCvmHandleTable.HandleTableLock);
	KeLeaveCriticalRegion();
	return st;
}

NOIR_STATUS NoirSetEventInjection(IN CVM_HANDLE VirtualMachine,IN ULONG32 VpIndex,IN ULONG64 InjectedEvent)
{
	NOIR_STATUS st=NOIR_UNSUCCESSFUL;
	PVOID VM=NULL;
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite(&NoirCvmHandleTable.HandleTableLock,TRUE);
	VM=NoirReferenceVirtualMachineByHandleUnsafe(VirtualMachine,NoirCvmHandleTable.TableCode);
	if(VM)
	{
		PVOID VP=nvc_reference_vcpu(VM,VpIndex);
		st=VP==NULL?NOIR_VCPU_NOT_EXIST:nvc_set_event_injection(VP,InjectedEvent);
	}
	ExReleaseResourceLite(&NoirCvmHandleTable.HandleTableLock);
	KeLeaveCriticalRegion();
	return st;
}

NOIR_STATUS NoirSetVirtualProcessorOptions(IN CVM_HANDLE VirtualMachine,IN ULONG32 VpIndex,IN ULONG32 OptionType,IN ULONG32 Options)
{
	NOIR_STATUS st=NOIR_UNSUCCESSFUL;
	PVOID VM=NULL;
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite(&NoirCvmHandleTable.HandleTableLock,TRUE);
	VM=NoirReferenceVirtualMachineByHandleUnsafe(VirtualMachine,NoirCvmHandleTable.TableCode);
	if(VM)
	{
		PVOID VP=nvc_reference_vcpu(VM,VpIndex);
		st=VP==NULL?NOIR_VCPU_NOT_EXIST:nvc_set_guest_vcpu_options(VP,OptionType,Options);
	}
	ExReleaseResourceLite(&NoirCvmHandleTable.HandleTableLock);
	KeLeaveCriticalRegion();
	return st;
}

NOIR_STATUS NoirRunVirtualProcessor(IN CVM_HANDLE VirtualMachine,IN ULONG32 VpIndex,OUT PVOID ExitContext)
{
	NOIR_STATUS st=NOIR_UNSUCCESSFUL;
	PVOID VM=NULL;
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite(&NoirCvmHandleTable.HandleTableLock,TRUE);
	VM=NoirReferenceVirtualMachineByHandleUnsafe(VirtualMachine,NoirCvmHandleTable.TableCode);
	if(VM)
	{
		PVOID VP=nvc_reference_vcpu(VM,VpIndex);
		st=VP==NULL?NOIR_VCPU_NOT_EXIST:nvc_run_vcpu(VP,ExitContext);
	}
	ExReleaseResourceLite(&NoirCvmHandleTable.HandleTableLock);
	KeLeaveCriticalRegion();
	return st;
}

NOIR_STATUS NoirRescindVirtualProcessor(IN CVM_HANDLE VirtualMachine,IN ULONG32 VpIndex)
{
	NOIR_STATUS st=NOIR_UNSUCCESSFUL;
	PVOID VM=NULL;
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite(&NoirCvmHandleTable.HandleTableLock,TRUE);
	VM=NoirReferenceVirtualMachineByHandleUnsafe(VirtualMachine,NoirCvmHandleTable.TableCode);
	if(VM)
	{
		PVOID VP=nvc_reference_vcpu(VM,VpIndex);
		st=VP==NULL?NOIR_VCPU_NOT_EXIST:nvc_rescind_vcpu(VP);
	}
	ExReleaseResourceLite(&NoirCvmHandleTable.HandleTableLock);
	KeLeaveCriticalRegion();
	return st;
}

NOIR_STATUS NoirCreateVirtualProcessor(IN CVM_HANDLE VirtualMachine,IN ULONG32 VpIndex)
{
	NOIR_STATUS st=NOIR_UNSUCCESSFUL;
	PVOID VM=NULL;
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite(&NoirCvmHandleTable.HandleTableLock,TRUE);
	VM=NoirReferenceVirtualMachineByHandleUnsafe(VirtualMachine,NoirCvmHandleTable.TableCode);
	if(VM)
	{
		PVOID VP=NULL;
		st=nvc_create_vcpu(VM,&VP,VpIndex);
		NoirCvmTracePrint("vCPU Creation Status: 0x%X\t vCPU: 0x%p\n",st,VP);
	}
	ExReleaseResourceLite(&NoirCvmHandleTable.HandleTableLock);
	KeLeaveCriticalRegion();
	return st;
}

NOIR_STATUS NoirReleaseVirtualProcessor(IN CVM_HANDLE VirtualMachine,IN ULONG32 VpIndex)
{
	NOIR_STATUS st=NOIR_UNSUCCESSFUL;
	PVOID VM=NULL;
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite(&NoirCvmHandleTable.HandleTableLock,TRUE);
	VM=NoirReferenceVirtualMachineByHandleUnsafe(VirtualMachine,NoirCvmHandleTable.TableCode);
	if(VM)
	{
		PVOID VP=nvc_reference_vcpu(VM,VpIndex);
		st=VP==NULL?NOIR_VCPU_NOT_EXIST:nvc_release_vcpu(VP);
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
					NoirCvmTracePrint("[Handle Recycle] Terminated PID=%d has created CVM Handle=%d! Terminating VM...\n",Pid,Handle);
					nvc_release_vm(VirtualMachine);
					NoirCvmHandleTable.HandleCount--;
					NoirDeleteHandle(Handle,NoirCvmHandleTable.TableCode);
				}
			}
		}
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
	NTSTATUS st=STATUS_UNSUCCESSFUL;
	PVOID NtKernelBase=NoirLocateImageBaseByName(L"ntoskrnl.exe");
	if(NtKernelBase)
	{
		st=ExInitializeResourceLite(&NoirCvmHandleTable.HandleTableLock);
		ZwQueryVirtualMemory=NoirLocateExportedProcedureByName(NtKernelBase,"ZwQueryVirtualMemory");
		NoirCvmTracePrint("Location of ZwQueryVirtualMemory: 0x%p\n",ZwQueryVirtualMemory);
		if(ZwQueryVirtualMemory==NULL)
			NoirCvmTracePrint("Failed to locate ZwQueryVirtualMemory!\n");
		else
		{
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
		}
	}
	else
		NoirCvmTracePrint("Failed to locate NT Kernel Image!\n");
	return st;
}