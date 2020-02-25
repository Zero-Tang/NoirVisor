/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the kernel-mode driver framework of Windows.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /booting/windrv/driver.c
*/

#include <ntddk.h>
#include <windef.h>
#include "driver.h"

PVOID static NoirGetInputBuffer(IN PIRP Irp)
{
	PIO_STACK_LOCATION irpsp=IoGetCurrentIrpStackLocation(Irp);
	if(irpsp->MajorFunction==IRP_MJ_DEVICE_CONTROL || irpsp->MajorFunction==IRP_MJ_INTERNAL_DEVICE_CONTROL)
	{
		ULONG Method=METHOD_FROM_CTL_CODE(irpsp->Parameters.DeviceIoControl.IoControlCode);
		if(Method==METHOD_NEITHER)
			return irpsp->Parameters.DeviceIoControl.Type3InputBuffer;
		else
			return Irp->AssociatedIrp.SystemBuffer;
	}
	return NULL;
}

PVOID static NoirGetOutputBuffer(IN PIRP Irp)
{
	PIO_STACK_LOCATION irpsp=IoGetCurrentIrpStackLocation(Irp);
	if(irpsp->MajorFunction==IRP_MJ_DEVICE_CONTROL || irpsp->MajorFunction==IRP_MJ_INTERNAL_DEVICE_CONTROL)
	{
		ULONG Method=METHOD_FROM_CTL_CODE(irpsp->Parameters.DeviceIoControl.IoControlCode);
		if(Method==METHOD_BUFFERED)
			return Irp->AssociatedIrp.SystemBuffer;
		else if(Method==METHOD_OUT_DIRECT)
			return MmGetSystemAddressForMdlSafe(Irp->MdlAddress,HighPagePriority);
		else
			return Irp->UserBuffer;
	}
	return NULL;
}

void NoirDriverUnload(IN PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING uniLinkName=RTL_CONSTANT_STRING(LINK_NAME);
	NoirTeardownHookedPages();
	NoirTeardownProtectedFile();
	NoirFinalizeCodeIntegrity();
	NoirFinalizePowerStateCallback();
	IoDeleteSymbolicLink(&uniLinkName);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS NoirDispatchCreateClose(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp)
{
	Irp->IoStatus.Status=STATUS_SUCCESS;
	Irp->IoStatus.Information=0;
	IoCompleteRequest(Irp,IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS NoirDispatchIoControl(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp)
{
	NTSTATUS st=STATUS_INVALID_DEVICE_REQUEST;
	PIO_STACK_LOCATION irpsp=IoGetCurrentIrpStackLocation(Irp);
	ULONG IoCtrlCode=irpsp->Parameters.DeviceIoControl.IoControlCode;
	PVOID InputBuffer=NoirGetInputBuffer(Irp);
	PVOID OutputBuffer=NoirGetOutputBuffer(Irp);
	ULONG InputSize=irpsp->Parameters.DeviceIoControl.InputBufferLength;
	ULONG OutputSize=irpsp->Parameters.DeviceIoControl.OutputBufferLength;
	switch(IoCtrlCode)
	{
		case IOCTL_Subvert:
		{
			NoirSetProtectedPID((ULONG)PsGetCurrentProcessId());
			NoirBuildHypervisor();
			break;
		}
		case IOCTL_Restore:
		{
			NoirTeardownHypervisor();
			break;
		}
		case IOCTL_SetPID:
		{
			NoirSetProtectedPID(*(PULONG)InputBuffer);
			break;
		}
		case IOCTL_SetVs:
		{
			RtlCopyMemory(virtual_vstr,InputBuffer,12);
			break;
		}
		case IOCTL_SetNs:
		{
			RtlCopyMemory(virtual_nstr,InputBuffer,48);
			break;
		}
		case IOCTL_SetName:
		{
			NoirSetProtectedFile((PWSTR)InputBuffer);
			break;
		}
		case IOCTL_NvVer:
		{
			st=STATUS_SUCCESS;
			*(PULONG)OutputBuffer=NoirVisorVersion();
			break;
		}
		case IOCTL_CpuVs:
		{
			st=STATUS_SUCCESS;
			NoirGetVendorString(OutputBuffer);
			break;
		}
		case IOCTL_CpuPn:
		{
			st=STATUS_SUCCESS;
			NoirGetProcessorName(OutputBuffer);
			break;
		}
		case IOCTL_OsVer:
		{
			st=STATUS_SUCCESS;
			NoirGetSystemVersion(OutputBuffer,OutputSize);
			break;
		}
		case IOCTL_VirtCap:
		{
			st=STATUS_SUCCESS;
			*(PULONG)OutputBuffer=NoirQueryVirtualizationSupportability();
			break;
		}
		default:
		{
			break;
		}
	}
	if(st==STATUS_SUCCESS)
		Irp->IoStatus.Information=OutputSize;
	else
		Irp->IoStatus.Information=0;
	Irp->IoStatus.Status=st;
	IoCompleteRequest(Irp,IO_NO_INCREMENT);
	return st;
}

void static NoirDriverReinitialize(IN PDRIVER_OBJECT DriverObject,IN PVOID Context OPTIONAL,IN ULONG Count)
{
	NoirInitializeCodeIntegrity(DriverObject->DriverStart);
	NoirLocatePsLoadedModule(DriverObject);
	system_cr3=__readcr3();
	orig_system_call=(ULONG_PTR)__readmsr(0xC0000082);
	NoirGetNtOpenProcessIndex();
	NoirSaveImageInfo(DriverObject);
	NoirBuildHookedPages();
	NoirBuildProtectedFile();
	NoirInitializePowerStateCallback();
}

NTSTATUS NoirDriverEntry(IN PDRIVER_OBJECT DriverObject,IN PUNICODE_STRING RegistryPath)
{
	NTSTATUS st=STATUS_UNSUCCESSFUL;
	PDEVICE_OBJECT DeviceObject=NULL;
	UNICODE_STRING uniDevName=RTL_CONSTANT_STRING(DEVICE_NAME);
	UNICODE_STRING uniLinkName=RTL_CONSTANT_STRING(LINK_NAME);
	//Setup Dispatch Routines
	DriverObject->MajorFunction[IRP_MJ_CREATE]=DriverObject->MajorFunction[IRP_MJ_CLOSE]=NoirDispatchCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]=NoirDispatchIoControl;
	DriverObject->DriverUnload=NoirDriverUnload;
	//Create Device and corresponding Symbolic Name
	IoRegisterDriverReinitialization(DriverObject,NoirDriverReinitialize,NULL);
	st=IoCreateDevice(DriverObject,0,&uniDevName,FILE_DEVICE_UNKNOWN,FILE_DEVICE_SECURE_OPEN,FALSE,&DeviceObject);
	if(NT_SUCCESS(st))
	{
		st=IoCreateSymbolicLink(&uniLinkName,&uniDevName);
		if(NT_ERROR(st))IoDeleteDevice(DeviceObject);
	}
	return st;
}