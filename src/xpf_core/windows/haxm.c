/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the Intel HAXM Device Dispatcher and CVM API Wrapper.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/windows/haxm.c
*/

// For documentation of this source code, please visit:
// https://github.com/intel/haxm/blob/master/docs/api.md

#include <ntddk.h>
#include <windef.h>
#include <ntstrsafe.h>
#include "custom_vm.h"
#include "haxm.h"

void NoirHaxDestroyVirtualProcessorTunnel(IN PHAX_VP_OBJECT HaxVp)
{
	if(HaxVp->TunnelMdl)
	{
		if(HaxVp->KernelTunnel)MmUnmapLockedPages(HaxVp->KernelTunnel,HaxVp->TunnelMdl);
		if(HaxVp->UserTunnel)MmUnmapLockedPages(HaxVp->UserTunnel,HaxVp->TunnelMdl);
		MmFreePagesFromMdl(HaxVp->TunnelMdl);
		HaxVp->KernelTunnel=HaxVp->UserTunnel=HaxVp->TunnelMdl=NULL;
	}
	if(HaxVp->IoBuffMdl)
	{
		if(HaxVp->KernelIoBuff)MmUnmapLockedPages(HaxVp->KernelIoBuff,HaxVp->IoBuffMdl);
		if(HaxVp->UserIoBuff)MmUnmapLockedPages(HaxVp->UserIoBuff,HaxVp->IoBuffMdl);
		MmFreePagesFromMdl(HaxVp->IoBuffMdl);
		HaxVp->KernelIoBuff=HaxVp->UserIoBuff=HaxVp->TunnelMdl=NULL;
	}
}

NTSTATUS NoirHaxSetupVirtualProcessorTunnel(IN PHAX_VP_OBJECT HaxVp,IN OUT PHAX_TUNNEL_INFO TunnelInfo)
{
	NTSTATUS st=STATUS_INSUFFICIENT_RESOURCES;
	PHYSICAL_ADDRESS L={0};
	PHYSICAL_ADDRESS H={0xFFFFFFFFFFFFFFFF};
	PHYSICAL_ADDRESS S={0};
	// Allocate pages which are mapped to BOTH USER MODE AND KERNEL MODE!
	HaxVp->TunnelMdl=MmAllocatePagesForMdlEx(L,H,S,PAGE_SIZE,MmCached,MM_ALLOCATE_FULLY_REQUIRED);
	HaxVp->IoBuffMdl=MmAllocatePagesForMdlEx(L,H,S,PAGE_SIZE,MmCached,MM_ALLOCATE_FULLY_REQUIRED);
	if(HaxVp->TunnelMdl==NULL || HaxVp->IoBuffMdl==NULL)
		NoirHaxDestroyVirtualProcessorTunnel(HaxVp);
	else
	{
		HaxVp->KernelTunnel=MmMapLockedPagesSpecifyCache(HaxVp->TunnelMdl,KernelMode,MmCached,NULL,FALSE,HighPagePriority);
		HaxVp->KernelIoBuff=MmMapLockedPagesSpecifyCache(HaxVp->TunnelMdl,KernelMode,MmCached,NULL,FALSE,HighPagePriority);
		HaxVp->UserTunnel=MmMapLockedPagesSpecifyCache(HaxVp->TunnelMdl,UserMode,MmCached,NULL,FALSE,HighPagePriority);
		HaxVp->UserIoBuff=MmMapLockedPagesSpecifyCache(HaxVp->TunnelMdl,UserMode,MmCached,NULL,FALSE,HighPagePriority);
		if(HaxVp->KernelTunnel==NULL || HaxVp->KernelIoBuff==NULL || HaxVp->UserTunnel==NULL || HaxVp->UserIoBuff==NULL)
			NoirHaxDestroyVirtualProcessorTunnel(HaxVp);
		else
		{
			TunnelInfo->Va=(ULONG64)HaxVp->UserTunnel;
			TunnelInfo->IoVa=(ULONG64)HaxVp->UserIoBuff;
			NoirDebugPrint("[HAXM] Kernel Tunnel: 0x%p\t Kernel IoBuff: 0x%p\n",HaxVp->KernelTunnel,HaxVp->KernelIoBuff);
			NoirDebugPrint("[HAXM] User Tunnel: 0x%p\t User IoBuff: 0x%p\n",HaxVp->UserTunnel,HaxVp->UserIoBuff);
			TunnelInfo->Size=PAGE_SIZE;
			st=STATUS_SUCCESS;
		}
	}
	return st;
}

NTSTATUS NoirHaxRemoveVirtualMachineNotification(IN CVM_HANDLE VmHandle)
{
	NTSTATUS st=STATUS_SUCCESS;
	// Handle Recycler detects a removal.
	KeEnterCriticalRegion();
	ExfAcquirePushLockExclusive(&HaxVmDeviceListLock);
	for(ULONG i=0;i<MAX_HAX_VM_COUNT;i++)
	{
		if(HaxVmList[i].Handle==VmHandle)
		{
			NoirDebugPrint("[Hax VM Recycle] Hax VM %u is being recycled!\n",i);
			HaxVmList[i].Handle=(CVM_HANDLE)-1;
			for(ULONG j=0;j<MAX_HAX_VP_COUNT;j++)
				NoirHaxDestroyVirtualProcessorTunnel(&HaxVmList[i].VpList[j]);
			break;
		}
	}
	ExfReleasePushLockExclusive(&HaxVmDeviceListLock);
	KeLeaveCriticalRegion();
	return st;
}

NTSTATUS NoirHaxCreateVirtualMachine(OUT PULONG VmId)
{
	NTSTATUS st=STATUS_INSUFFICIENT_RESOURCES;
	if(VmId==NULL)return STATUS_INVALID_PARAMETER;
	*VmId=0xFFFFFFFF;
	KeEnterCriticalRegion();
	ExfAcquirePushLockExclusive(&HaxVmDeviceListLock);
	for(ULONG i=0;i<MAX_HAX_VM_COUNT;i++)
	{
		if(HaxVmList[i].Handle==(CVM_HANDLE)-1)
		{
			*VmId=i;
			break;
		}
	}
	if(*VmId!=0xFFFFFFFF)
	{
		NOIR_STATUS nst=NoirCreateVirtualMachine(&HaxVmList[*VmId].Handle);
		st=nst==NOIR_SUCCESS?STATUS_SUCCESS:STATUS_UNSUCCESSFUL;
	}
	ExfReleasePushLockExclusive(&HaxVmDeviceListLock);
	KeLeaveCriticalRegion();
	return st;
}

NTSTATUS NoirHaxCreateVirtualProcessor(IN ULONG VmId,IN ULONG VpId)
{
	NOIR_STATUS nst;
	if(VpId>=MAX_HAX_VP_COUNT)return STATUS_INVALID_PARAMETER;
	if(VmId>=MAX_HAX_VM_COUNT)return STATUS_INVALID_PARAMETER;
	KeEnterCriticalRegion();
	ExfAcquirePushLockShared(&HaxVmDeviceListLock);
	nst=NoirCreateVirtualProcessor(HaxVmList[VmId].Handle,VpId);
	ExfReleasePushLockShared(&HaxVmDeviceListLock);
	KeLeaveCriticalRegion();
	if(nst==NOIR_SUCCESS)
	{
		// Intel HAXM has some assumptions about initial vCPU state...
		ULONG64 Xcr0=1;
		NoirEditVirtualProcessorRegisters(HaxVmList[VmId].Handle,VpId,NoirCvmXcr0Register,&Xcr0,sizeof(Xcr0));
	}
	return nst==NOIR_SUCCESS?STATUS_SUCCESS:STATUS_UNSUCCESSFUL;
}

NTSTATUS NoirHaxSetMapping(IN PHAX_VM_OBJECT HaxVm,IN PHAX_SET_RAM_INFO MapInfo)
{
	NOIR_STATUS nst=NOIR_INVALID_PARAMETER;
	PVOID VM;
	if(BYTE_OFFSET(MapInfo->GpaStart))return STATUS_INVALID_PARAMETER;
	if(BYTE_OFFSET(MapInfo->HvaStart))return STATUS_INVALID_PARAMETER;
	if(BYTE_OFFSET(MapInfo->Size))return STATUS_INVALID_PARAMETER;
	VM=NoirReferenceVirtualMachineByHandle(HaxVm->Handle);
	if(VM)
	{
		nst=nvc_hax_set_mapping(VM,MapInfo);
		NoirDebugPrint("[HAXM] Mapping (GPA=0x%p HVA=0x%p Bytes=0x%08X Flags=0x%02X) Status: 0x%X\n",MapInfo->GpaStart,MapInfo->HvaStart,MapInfo->Size,MapInfo->Flags.Value,nst);
	}
	return nst==NOIR_SUCCESS?STATUS_SUCCESS:STATUS_UNSUCCESSFUL;
}

NTSTATUS NoirHaxDispatchCreate(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp)
{
	PHAX_DEVICE_EXTENSION DevExt=(PHAX_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	switch(DevExt->Type)
	{
		case HAX_DEVEXT_TYPE_UP:
		{
			InterlockedIncrement(&DevExt->HaxDeviceExtension.Counter);
			break;
		}
		default:
		{
			break;
		}
	}
	Irp->IoStatus.Status=STATUS_SUCCESS;
	Irp->IoStatus.Information=0;
	IoCompleteRequest(Irp,IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS NoirHaxDispatchClose(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp)
{
	NTSTATUS st=STATUS_SUCCESS;
	PHAX_DEVICE_EXTENSION DevExt=(PHAX_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PSTR DevTypeName=DevExt->Type<4?HaxDeviceTypeString[DevExt->Type]:"HaxNone";
	NoirDebugPrint("HaxClose Device 0x%p (Type=%s) at PID=%d\n",DeviceObject,DevTypeName,(ULONG32)PsGetCurrentProcessId());
	switch(DevExt->Type)
	{
		case HAX_DEVEXT_TYPE_UP:
		{
			InterlockedDecrement(&DevExt->HaxDeviceExtension.Counter);
			break;
		}
		case HAX_DEVEXT_TYPE_VM:
		{
			CVM_HANDLE Handle=DevExt->HaxVmExtension.Hax->Handle;
			NoirDebugPrint("Closing VM %llu...\n",Handle);
			if(NoirDecrementVirtualMachineReference(Handle)!=NOIR_SUCCESS)
				st=STATUS_UNSUCCESSFUL;
			break;
		}
		case HAX_DEVEXT_TYPE_VCPU:
		{
			CVM_HANDLE Handle=DevExt->HaxVpExtension.HaxVm->Handle;
			ULONG32 VpId=DevExt->HaxVpExtension.VpId;
			NoirDebugPrint("Closing VM %llu, vCPU %u...\n",Handle,VpId);
			if(NoirDecrementVirtualProcessorReference(Handle,VpId)!=NOIR_SUCCESS)
				st=STATUS_UNSUCCESSFUL;
			break;
		}
		default:
		{
			NoirDebugPrint("Invalid device type (%d: %s) closure request!\n",DevExt->Type,DevTypeName);
			break;
		}
	}
	Irp->IoStatus.Status=st;
	Irp->IoStatus.Information=0;
	IoCompleteRequest(Irp,IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS NoirHaxVpControl(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp)
{
	NTSTATUS st=STATUS_INVALID_DEVICE_REQUEST;
	PIO_STACK_LOCATION irpsp=IoGetCurrentIrpStackLocation(Irp);
	ULONG IoCtrlCode=irpsp->Parameters.DeviceIoControl.IoControlCode;
	PVOID InputBuffer=NoirGetInputBuffer(Irp);
	PVOID OutputBuffer=NoirGetOutputBuffer(Irp);
	ULONG InputSize=irpsp->Parameters.DeviceIoControl.InputBufferLength;
	ULONG OutputSize=irpsp->Parameters.DeviceIoControl.OutputBufferLength;
	ULONG ReturnSize=0;
	PHAX_DEVICE_EXTENSION DevExt=(PHAX_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PHAX_VM_OBJECT HaxVm=DevExt->HaxVpExtension.HaxVm;
	PHAX_VP_OBJECT HaxVp=DevExt->HaxVpExtension.HaxVp;
	ULONG VmId=DevExt->HaxVmExtension.VmId;
	ULONG VpId=DevExt->HaxVpExtension.VpId;
	switch(IoCtrlCode)
	{
		NOIR_STATUS nst;
		case HAX_VCPU_IOCTL_SETUP_TUNNEL:
		{
			PHAX_TUNNEL_INFO TunInfo=(PHAX_TUNNEL_INFO)OutputBuffer;
			if(OutputSize<sizeof(HAX_TUNNEL_INFO))
				st=STATUS_INVALID_PARAMETER;
			else
			{
				PVOID VM=NoirReferenceVirtualMachineByHandle(HaxVm->Handle);
				if(VM)
				{
					PVOID VP=nvc_reference_vcpu(VM,VpId);
					st=NoirHaxSetupVirtualProcessorTunnel(HaxVp,TunInfo);
					if(NT_SUCCESS(st))nvc_hax_set_tunnel(VP,HaxVp->KernelTunnel,HaxVp->KernelIoBuff);
				}
				ReturnSize=sizeof(HAX_TUNNEL_INFO);
			}
			break;
		}
		case HAX_VCPU_IOCTL_SET_REGS:
		{
			PVOID VM=NoirReferenceVirtualMachineByHandle(HaxVm->Handle);
			if(VM)
			{
				PVOID VP=nvc_reference_vcpu(VM,VpId);
				if(VP)nst=nvc_hax_set_vcpu_registers(VP,InputBuffer,InputSize,&ReturnSize);
			}
			NoirDebugPrint("[HAXM] VM %u vCPU %u set registers status=0x%X\n",VmId,VpId,nst);
			st=nst==NOIR_SUCCESS?STATUS_SUCCESS:STATUS_UNSUCCESSFUL;
			break;
		}
		case HAX_VCPU_IOCTL_GET_REGS:
		{
			PVOID VM=NoirReferenceVirtualMachineByHandle(HaxVm->Handle);
			if(VM)
			{
				PVOID VP=nvc_reference_vcpu(VM,VpId);
				if(VP)nst=nvc_hax_get_vcpu_registers(VP,OutputBuffer,OutputSize,&ReturnSize);
			}
			NoirDebugPrint("[HAXM] VM %u vCPU %u get registers status=0x%X\n",VmId,VpId,nst);
			st=nst==NOIR_SUCCESS?STATUS_SUCCESS:STATUS_UNSUCCESSFUL;
			break;
		}
		case HAX_VCPU_IOCTL_SET_MSRS:
		{
			PVOID VM=NoirReferenceVirtualMachineByHandle(HaxVm->Handle);
			if(VM)
			{
				PVOID VP=nvc_reference_vcpu(VM,VpId);
				if(VP)nst=nvc_hax_set_msrs(VP,InputBuffer,InputSize,&ReturnSize);
			}
			NoirDebugPrint("[HAXM] VM %u vCPU %u set MSR status=0x%X\n",VmId,VpId,nst);
			st=nst==NOIR_SUCCESS?STATUS_SUCCESS:STATUS_UNSUCCESSFUL;
			break;
		}
		case HAX_VCPU_IOCTL_GET_MSRS:
		{
			PVOID VM=NoirReferenceVirtualMachineByHandle(HaxVm->Handle);
			if(VM)
			{
				PVOID VP=nvc_reference_vcpu(VM,VpId);
				if(VP)nst=nvc_hax_get_msrs(VP,InputBuffer,InputSize,&ReturnSize);
			}
			NoirDebugPrint("[HAXM] VM %u vCPU %u set MSR status=0x%X\n",VmId,VpId,nst);
			st=nst==NOIR_SUCCESS?STATUS_SUCCESS:STATUS_UNSUCCESSFUL;
			break;
		}
		case HAX_VCPU_IOCTL_SET_FPU:
		{
			PVOID VM=NoirReferenceVirtualMachineByHandle(HaxVm->Handle);
			if(VM)
			{
				PVOID VP=nvc_reference_vcpu(VM,VpId);
				if(VP)nst=nvc_hax_set_fpu_state(VP,InputBuffer,InputSize,&ReturnSize);
			}
			NoirDebugPrint("[HAXM] VM %u vCPU %u set FPU status=0x%X\n",VmId,VpId,nst);
			st=nst==NOIR_SUCCESS?STATUS_SUCCESS:STATUS_UNSUCCESSFUL;
			break;
		}
		case HAX_VCPU_IOCTL_GET_FPU:
		{
			PVOID VM=NoirReferenceVirtualMachineByHandle(HaxVm->Handle);
			if(VM)
			{
				PVOID VP=nvc_reference_vcpu(VM,VpId);
				if(VP)nst=nvc_hax_get_fpu_state(VP,OutputBuffer,OutputSize,&ReturnSize);
			}
			NoirDebugPrint("[HAXM] VM %u vCPU %u get FPU status=0x%X\n",VmId,VpId,nst);
			st=nst==NOIR_SUCCESS?STATUS_SUCCESS:STATUS_UNSUCCESSFUL;
			break;
		}
		case HAX_VCPU_IOCTL_RUN:
		{
			PVOID VM=NoirReferenceVirtualMachineByHandle(HaxVm->Handle);
			if(VM)
			{
				PVOID VP=nvc_reference_vcpu(VM,VpId);
				if(VP)nst=nvc_hax_run_vcpu(VP);
			}
			st=nst==NOIR_SUCCESS?STATUS_SUCCESS:STATUS_UNSUCCESSFUL;
			break;
		}
		default:
		{
			NoirDebugPrint("[HAXM] Unknown HAX VP Device I/O Request! (%X)\n",IoCtrlCode);
			break;
		}
	}
	Irp->IoStatus.Status=st;
	Irp->IoStatus.Information=ReturnSize;
	IoCompleteRequest(Irp,IO_NO_INCREMENT);
	return st;
}

NTSTATUS NoirHaxVmControl(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp)
{
	NTSTATUS st=STATUS_INVALID_DEVICE_REQUEST;
	PIO_STACK_LOCATION irpsp=IoGetCurrentIrpStackLocation(Irp);
	ULONG IoCtrlCode=irpsp->Parameters.DeviceIoControl.IoControlCode;
	PVOID InputBuffer=NoirGetInputBuffer(Irp);
	PVOID OutputBuffer=NoirGetOutputBuffer(Irp);
	ULONG InputSize=irpsp->Parameters.DeviceIoControl.InputBufferLength;
	ULONG OutputSize=irpsp->Parameters.DeviceIoControl.OutputBufferLength;
	ULONG ReturnSize=0;
	PHAX_DEVICE_EXTENSION DevExt=(PHAX_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PHAX_VM_OBJECT HaxVm=DevExt->HaxVmExtension.Hax;
	ULONG VmId=DevExt->HaxVmExtension.VmId;
	switch(IoCtrlCode)
	{
		case HAX_VM_IOCTL_VCPU_CREATE:
		{
			NoirDebugPrint("[HAXM] VM %u is creating vCPU...\n",VmId);
			if(InputSize<sizeof(ULONG32))
				st=STATUS_INVALID_PARAMETER;
			else
			{
				ULONG32 VpId=*(PULONG32)InputBuffer;
				st=NoirHaxCreateVirtualProcessor(VmId,VpId);
			}
			break;
		}
		case HAX_VM_IOCTL_ALLOC_RAM:
		{
			if(InputSize<sizeof(HAX_ALLOC_RAM_INFO))
				st=STATUS_INVALID_PARAMETER;
			else
			{
				PVOID VM=NoirReferenceVirtualMachineByHandle(HaxVm->Handle);
				if(VM)
				{
					PHAX_ALLOC_RAM_INFO Info=(PHAX_ALLOC_RAM_INFO)InputBuffer;
					// NOIR_STATUS nst=nvc_register_memblock(VM,Info->VA,(ULONG64)Info->Size);
					NOIR_STATUS nst=NOIR_SUCCESS;
					st=nst==NOIR_SUCCESS?STATUS_SUCCESS:STATUS_UNSUCCESSFUL;
				}
			}
			break;
		}
		case HAX_VM_IOCTL_ADD_RAMBLOCK:
		{
			if(InputSize<sizeof(HAX_RAMBLOCK_INFO))
				st=STATUS_INVALID_PARAMETER;
			else
			{
				PVOID VM=NoirReferenceVirtualMachineByHandle(HaxVm->Handle);
				if(VM)
				{
					PHAX_RAMBLOCK_INFO Info=(PHAX_RAMBLOCK_INFO)InputBuffer;
					// NOIR_STATUS nst=nvc_register_memblock(VM,Info->StartVa,Info->Size);
					NOIR_STATUS nst=NOIR_SUCCESS;
					st=nst==NOIR_SUCCESS?STATUS_SUCCESS:STATUS_UNSUCCESSFUL;
				}
			}
			break;
		}
		case HAX_VM_IOCTL_SET_RAM:
		{
			NoirDebugPrint("[HAXM] VM %u is setting RAM...\n",VmId);
			if(InputSize<sizeof(HAX_SET_RAM_INFO))
				st=STATUS_INVALID_PARAMETER;
			else
				st=NoirHaxSetMapping(HaxVm,(PHAX_SET_RAM_INFO)InputBuffer);
			break;
		}
		case HAX_VM_IOCTL_NOTIFY_QEMU_VERSION:
		{
			// Notify QEMU Version.
			if(InputSize<sizeof(HAX_QEMU_VERSION))
				st=STATUS_INVALID_PARAMETER;
			else
			{
				PHAX_QEMU_VERSION VerInfo=(PHAX_QEMU_VERSION)InputBuffer;
				NoirDebugPrint("[HAXM] VM %u is QEMU (Least Version: %u\t Current Version: %u)\n",VmId,VerInfo->LeastVersion,VerInfo->CurrentVersion);
				st=STATUS_SUCCESS;
			}
			break;
		}
		default:
		{
			NoirDebugPrint("[HAXM] Unknown HAX VM Device I/O Request! (%X)\n",IoCtrlCode);
			break;
		}
	}
	Irp->IoStatus.Status=st;
	Irp->IoStatus.Information=ReturnSize;
	IoCompleteRequest(Irp,IO_NO_INCREMENT);
	return st;
}

NTSTATUS NoirHaxIoControl(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp)
{
	NTSTATUS st=STATUS_INVALID_DEVICE_REQUEST;
	PIO_STACK_LOCATION irpsp=IoGetCurrentIrpStackLocation(Irp);
	ULONG IoCtrlCode=irpsp->Parameters.DeviceIoControl.IoControlCode;
	PVOID InputBuffer=NoirGetInputBuffer(Irp);
	PVOID OutputBuffer=NoirGetOutputBuffer(Irp);
	ULONG InputSize=irpsp->Parameters.DeviceIoControl.InputBufferLength;
	ULONG OutputSize=irpsp->Parameters.DeviceIoControl.OutputBufferLength;
	ULONG ReturnSize=0;
	switch(IoCtrlCode)
	{
		case HAX_IOCTL_VERSION:
		{
			NoirDebugPrint("[HAXM] Querying info...\n");
			if(nvc_hax_get_version_info(OutputBuffer,OutputSize,&ReturnSize)==NOIR_BUFFER_TOO_SMALL)
				st=STATUS_BUFFER_TOO_SMALL;
			else
				st=STATUS_SUCCESS;
			break;
		}
		case HAX_IOCTL_CREATE_VM:
		{
			NoirDebugPrint("[HAXM] Creating VM...\n");
			if(OutputSize<sizeof(ULONG32))
				st=STATUS_BUFFER_TOO_SMALL;
			else
			{
				st=NoirHaxCreateVirtualMachine(OutputBuffer);
				NoirDebugPrint("Hax VM Creation Status=0x%X\n",st);
				ReturnSize=sizeof(ULONG32);
			}
			break;
		}
		case HAX_IOCTL_CAPABILITY:
		{
			NoirDebugPrint("[HAXM] Querying capability...\n");
			if(nvc_hax_get_capability(OutputBuffer,OutputSize,&ReturnSize)==NOIR_BUFFER_TOO_SMALL)
				st=STATUS_BUFFER_TOO_SMALL;
			else
				st=STATUS_SUCCESS;
			break;
		}
		case HAX_IOCTL_SET_MEMLIMIT:
		{
			NoirDebugPrint("[HAXM] Setting memory limit...\n");
			if(nvc_hax_set_memlimit(OutputBuffer,OutputSize,&ReturnSize)==NOIR_BUFFER_TOO_SMALL)
				st=STATUS_BUFFER_TOO_SMALL;
			else
				st=STATUS_SUCCESS;
			break;
		}
		default:
		{
			NoirDebugPrint("[HAXM] Unknown HAX UP Device I/O Request! (%X)\n",IoCtrlCode);
			break;
		}
	}
	Irp->IoStatus.Status=st;
	Irp->IoStatus.Information=ReturnSize;
	IoCompleteRequest(Irp,IO_NO_INCREMENT);
	return st;
}

NTSTATUS NoirHaxDispatchIoControl(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp)
{
	NTSTATUS st=STATUS_INVALID_DEVICE_REQUEST;
	PHAX_DEVICE_EXTENSION DevExt=(PHAX_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	switch(DevExt->Type)
	{
		case HAX_DEVEXT_TYPE_UP:
		{
			st=NoirHaxIoControl(DeviceObject,Irp);
			break;
		}
		case HAX_DEVEXT_TYPE_VM:
		{
			st=NoirHaxVmControl(DeviceObject,Irp);
			break;
		}
		case HAX_DEVEXT_TYPE_VCPU:
		{
			st=NoirHaxVpControl(DeviceObject,Irp);
			break;
		}
	}
	return st;
}

void NoirHaxFinalizeVpDevice(IN PHAX_VP_OBJECT HaxVp)
{
	IoDeleteSymbolicLink(&HaxVp->LinkName);
	IoDeleteDevice(HaxVp->DeviceObject);
}

NTSTATUS NoirHaxInitializeVpDevice(IN ULONG VmIndex,IN ULONG VpIndex)
{
	PHAX_VP_OBJECT HaxVp=&HaxVmList[VmIndex].VpList[VpIndex];
	NTSTATUS st=STATUS_UNSUCCESSFUL;
	// Create VP Device
	UNICODE_STRING uniDevName;
	WCHAR DevNameBuff[32];
	RtlStringCbPrintfW(DevNameBuff,sizeof(DevNameBuff),HAX_VP_DEVICE_FORMAT,VmIndex,VpIndex);
	RtlInitUnicodeString(&uniDevName,DevNameBuff);
	st=IoCreateDevice(NoirDriverObject,sizeof(HAX_DEVICE_EXTENSION),&uniDevName,FILE_DEVICE_UNKNOWN,FILE_DEVICE_SECURE_OPEN,FALSE,&HaxVp->DeviceObject);
	if(NT_ERROR(st))
		NoirDebugPrint("[HAXM] Failed to create device for %ws!\n",DevNameBuff);
	else
	{
		RtlStringCbPrintfW(HaxVp->LinkNameBuffer,sizeof(HaxVp->LinkNameBuffer),HAX_VP_LINK_FORMAT,VmIndex,VpIndex);
		RtlInitUnicodeString(&HaxVp->LinkName,HaxVp->LinkNameBuffer);
		st=IoCreateSymbolicLink(&HaxVp->LinkName,&uniDevName);
		if(NT_ERROR(st))
			NoirDebugPrint("[HAXM] Failed to create symbolic link for %ws!\n",HaxVp->LinkName);
		else
		{
			PHAX_DEVICE_EXTENSION DevExt=(PHAX_DEVICE_EXTENSION)HaxVp->DeviceObject->DeviceExtension;
			DevExt->Type=HAX_DEVEXT_TYPE_VCPU;
			DevExt->HaxVpExtension.VmId=VmIndex;
			DevExt->HaxVpExtension.VpId=VpIndex;
			DevExt->HaxVpExtension.HaxVm=&HaxVmList[VmIndex];
			DevExt->HaxVpExtension.HaxVp=HaxVp;
		}
	}
	return st;
}

void NoirHaxFinalizeVmDevice(IN PHAX_VM_OBJECT HaxVm)
{
	for(ULONG i=0;i<MAX_HAX_VP_COUNT;i++)
		NoirHaxFinalizeVpDevice(&HaxVm->VpList[i]);
	IoDeleteSymbolicLink(&HaxVm->LinkName);
	IoDeleteDevice(HaxVm->DeviceObject);
}

NTSTATUS NoirHaxInitializeVmDevice(IN ULONG Index)
{
	PHAX_VM_OBJECT HaxVm=&HaxVmList[Index];
	NTSTATUS st=STATUS_UNSUCCESSFUL;
	// Create VM Device
	UNICODE_STRING uniDevName;
	WCHAR DevNameBuff[32];
	RtlStringCbPrintfW(DevNameBuff,sizeof(DevNameBuff),HAX_VM_DEVICE_FORMAT,Index);
	RtlInitUnicodeString(&uniDevName,DevNameBuff);
	st=IoCreateDevice(NoirDriverObject,sizeof(HAX_DEVICE_EXTENSION),&uniDevName,FILE_DEVICE_UNKNOWN,FILE_DEVICE_SECURE_OPEN,FALSE,&HaxVm->DeviceObject);
	if(NT_ERROR(st))
		NoirDebugPrint("[HAXM] Failed to create device for %ws!\n",DevNameBuff);
	else
	{
		RtlStringCbPrintfW(HaxVm->LinkNameBuffer,sizeof(HaxVm->LinkNameBuffer),HAX_VM_LINK_FORMAT,Index);
		RtlInitUnicodeString(&HaxVm->LinkName,HaxVm->LinkNameBuffer);
		st=IoCreateSymbolicLink(&HaxVm->LinkName,&uniDevName);
		if(NT_ERROR(st))
			NoirDebugPrint("[HAXM] Failed to create symbolic link for %ws!\n",HaxVm->LinkName);
		else
		{
			PHAX_DEVICE_EXTENSION DevExt=(PHAX_DEVICE_EXTENSION)HaxVm->DeviceObject->DeviceExtension;
			HaxVm->Handle=(CVM_HANDLE)-1;
			DevExt->Type=HAX_DEVEXT_TYPE_VM;
			DevExt->HaxVmExtension.VmId=Index;
			DevExt->HaxVmExtension.Hax=HaxVm;
			for(ULONG i=0;i<MAX_HAX_VP_COUNT;i++)
			{
				st=NoirHaxInitializeVpDevice(Index,i);
			}
		}
	}
	return st;
}

void NoirHaxFinalizeDeviceExtension()
{
	UNICODE_STRING uniLinkName=RTL_CONSTANT_STRING(HAX_LINK_NAME);
	for(ULONG i=0;i<MAX_HAX_VM_COUNT;i++)
		NoirHaxFinalizeVmDevice(&HaxVmList[i]);
	IoDeleteSymbolicLink(&uniLinkName);
	IoDeleteDevice(HaxDeviceObject);
}

NTSTATUS NoirHaxInitializeDeviceExtension(IN PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING uniDevName=RTL_CONSTANT_STRING(HAX_DEVICE_NAME);
	NTSTATUS st=IoCreateDevice(DriverObject,sizeof(HAX_DEVICE_EXTENSION),&uniDevName,FILE_DEVICE_UNKNOWN,FILE_DEVICE_SECURE_OPEN,FALSE,&HaxDeviceObject);
	if(NT_SUCCESS(st))
	{
		UNICODE_STRING uniLinkName=RTL_CONSTANT_STRING(HAX_LINK_NAME);
		st=IoCreateSymbolicLink(&uniLinkName,&uniDevName);
		if(NT_SUCCESS(st))
		{
			PHAX_DEVICE_EXTENSION DevExt=(PHAX_DEVICE_EXTENSION)HaxDeviceObject->DeviceExtension;
			DevExt->Type=HAX_DEVEXT_TYPE_UP;
			DevExt->HaxDeviceExtension.Counter=0;
			RtlZeroMemory(HaxVmList,sizeof(HaxVmList));
			for(ULONG i=0;i<MAX_HAX_VM_COUNT;i++)
			{
				st=NoirHaxInitializeVmDevice(i);
				if(NT_ERROR(st))
				{
					NoirHaxFinalizeDeviceExtension();
					break;
				}
			}
		}
	}
	return st;
}