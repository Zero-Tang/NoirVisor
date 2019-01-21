/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

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
		ULONG IoCode=irpsp->Parameters.DeviceIoControl.IoControlCode;
		if(IoCode & METHOD_BUFFERED || IoCode & METHOD_IN_DIRECT || IoCode & METHOD_OUT_DIRECT)
			return Irp->AssociatedIrp.SystemBuffer;
		else
			return irpsp->Parameters.DeviceIoControl.Type3InputBuffer;
	}
	return NULL;
}

PVOID static NoirGetOutputBuffer(IN PIRP Irp)
{
	PIO_STACK_LOCATION irpsp=IoGetCurrentIrpStackLocation(Irp);
	if(irpsp->MajorFunction==IRP_MJ_DEVICE_CONTROL || irpsp->MajorFunction==IRP_MJ_INTERNAL_DEVICE_CONTROL)
	{
		ULONG IoCode=irpsp->Parameters.DeviceIoControl.IoControlCode;
		if(IoCode & METHOD_BUFFERED)
			return Irp->AssociatedIrp.SystemBuffer;
		if(IoCode & METHOD_OUT_DIRECT)
			return MmGetSystemAddressForMdlSafe(Irp->MdlAddress,HighPagePriority);
		if(IoCode & METHOD_NEITHER)
			return Irp->UserBuffer;
	}
	return NULL;
}

void NoirDriverUnload(IN PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING uniLinkName=RTL_CONSTANT_STRING(LINK_NAME);
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
			NoirBuildHypervisor();
			break;
		}
		case IOCTL_Restore:
		{
			NoirTeardownHypervisor();
			break;
		}
		case IOCTL_NvVer:
		{
			*(PULONG)OutputBuffer=NoirVisorVersion();
			break;
		}
		case IOCTL_CpuVs:
		{
			NoirGetVendorString(OutputBuffer);
			break;
		}
		case IOCTL_CpuPn:
		{
			NoirGetProcessorName(OutputBuffer);
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
	st=IoCreateDevice(DriverObject,0,&uniDevName,FILE_DEVICE_UNKNOWN,FILE_DEVICE_SECURE_OPEN,FALSE,&DeviceObject);
	if(NT_SUCCESS(st))
	{
		st=IoCreateSymbolicLink(&uniLinkName,&uniDevName);
		if(NT_ERROR(st))IoDeleteDevice(DeviceObject);
	}
	return st;
}