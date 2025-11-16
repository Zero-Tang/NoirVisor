/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2025, Zero Tang. All rights reserved.

  This file is the kernel-mode driver framework of Windows.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /booting/windrv/driver.c
*/

#include <ntddk.h>
#include <windef.h>
#include <isa_availability.h>
#include "driver.h"

// Required for being compatible with Rust.
PVOID __CxxFrameHandler3(PVOID ExceptionRecord,PVOID RegistrationNode,PVOID Context,PVOID DispatcherContext)
{
	return NULL;
}

PVOID NoirGetInputBuffer(IN PIRP Irp)
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

PVOID NoirGetOutputBuffer(IN PIRP Irp)
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
	NoirTeardownHypervisor();
	NoirTeardownHookedPages();
	NoirTeardownProtectedFile();
	NoirFinalizeCodeIntegrity();
	NoirFinalizePowerStateCallback();
	NoirAcpiFinalize();
	NoirFreeAllAllocatedPages();
	NoirReportMemoryIntrospectionCounter();
	IoDeleteSymbolicLink(&uniLinkName);
	IoDeleteDevice(NoirDeviceObject);
}

NTSTATUS NoirDispatchCreate(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp)
{
	Irp->IoStatus.Status=STATUS_SUCCESS;
	Irp->IoStatus.Information=0;
	IoCompleteRequest(Irp,IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS NoirDispatchClose(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp)
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
			NoirSetProtectedPID((ULONG)(ULONG_PTR)PsGetCurrentProcessId());
			SubversionProcess=PsGetCurrentProcess();
			NoirBuildHypervisor();
			NoirReportWindowsVersion();
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_Restore:
		{
			st=STATUS_SUCCESS;
			if(PsGetCurrentProcess()!=SubversionProcess)
				st=STATUS_ACCESS_DENIED;
			else
				NoirTeardownHypervisor();
			break;
		}
		case IOCTL_SetPID:
		{
			st=STATUS_SUCCESS;
			NoirSetProtectedPID(*(PULONG)InputBuffer);
			break;
		}
		case IOCTL_SetVs:
		{
			st=STATUS_SUCCESS;
			RtlCopyMemory(virtual_vstr,InputBuffer,12);
			break;
		}
		case IOCTL_SetNs:
		{
			st=STATUS_SUCCESS;
			RtlCopyMemory(virtual_nstr,InputBuffer,48);
			break;
		}
		case IOCTL_SetName:
		{
			st=STATUS_SUCCESS;
			NoirSetProtectedFile((PWSTR)InputBuffer);
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
		case IOCTL_VirtEn:
		{
			st=STATUS_SUCCESS;
			*(PBOOLEAN)OutputBuffer=NoirIsVirtualizationEnabled();
			break;
		}
		case IOCTL_CvmCreateVm:
		{
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmDeleteVm:
		{
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmSetMapping:
		{
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmQueryGpaAdMap:
		{
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmClearGpaAdBit:
		{
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmQueryHvStatus:
		{
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmCreateVcpu:
		{
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmDeleteVcpu:
		{
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmRunVcpu:
		{
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmViewVcpuReg:
		{
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmEditVcpuReg:
		{
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmRescindVcpu:
		{
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmInjectEvent:
		{
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmSetVcpuOptions:
		{
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmQueryVcpuStats:
		{
			st=STATUS_SUCCESS;
			break;
		}
		default:
		{
			break;
		}
	}
	Irp->IoStatus.Information=st==STATUS_SUCCESS?OutputSize:0;
	Irp->IoStatus.Status=st;
	IoCompleteRequest(Irp,IO_NO_INCREMENT);
	return st;
}

void static NoirDriverReinitialize(IN PDRIVER_OBJECT DriverObject,IN PVOID Context OPTIONAL,IN ULONG Count)
{
	NoirPrintCompilerVersion();
	NoirConfigureInternalDebugger();
	NoirInitializeLogger();
	NoirInitializeDisassembler();
	NoirInitializeCodeIntegrity(DriverObject->DriverStart);
	NoirDebugPrint("CI is initialized!\n");
	NoirLocatePsLoadedModule(DriverObject);
	NoirDebugPrint("PsLoadedModuleList is located!\n");
	orig_system_call=(ULONG_PTR)__readmsr(0xC0000082);
	NoirGetNtOpenProcessIndex();
	NoirSaveImageInfo(DriverObject);
	NoirGetUefiHypervisionStatus();
	NoirBuildHookedPages();
	NoirBuildProtectedFile();
	NoirInitializePowerStateCallback();
	NoirSubvertSystemOnDriverLoad(&SubvertOnDriverLoad);
	NoirAcpiInitialize();
	if(SubvertOnDriverLoad)
		if(NoirQueryVirtualizationSupportability())
			if(NoirIsVirtualizationEnabled())
				NoirBuildHypervisor();
}

NTSTATUS NoirDriverEntry(IN PDRIVER_OBJECT DriverObject,IN PUNICODE_STRING RegistryPath)
{
	NTSTATUS st=STATUS_DEVICE_CONFIGURATION_ERROR;
	UNICODE_STRING uniDevName=RTL_CONSTANT_STRING(DEVICE_NAME);
	UNICODE_STRING uniLinkName=RTL_CONSTANT_STRING(LINK_NAME);
	// Initialize Microsoft-optimized CRT constants (for memcpy, memset, etc.)
	__isa_available_init();
	// Purposefully disable AVX when we use memcpy, memset, etc.
	if(__isa_available>__ISA_AVAILABLE_SSE42)__isa_available=__ISA_AVAILABLE_SSE42;
	// Setup Dispatch Routines
	DriverObject->MajorFunction[IRP_MJ_CREATE]=NoirDispatchCreate;
	DriverObject->MajorFunction[IRP_MJ_CLOSE]=NoirDispatchClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]=NoirDispatchIoControl;
	DriverObject->DriverUnload=NoirDriverUnload;
	// Create Device and corresponding Symbolic Name
	IoRegisterDriverReinitialization(DriverObject,NoirDriverReinitialize,NULL);
	st=IoCreateDevice(DriverObject,0,&uniDevName,FILE_DEVICE_UNKNOWN,FILE_DEVICE_SECURE_OPEN,FALSE,&NoirDeviceObject);
	if(NT_SUCCESS(st))
	{
		st=IoCreateSymbolicLink(&uniLinkName,&uniDevName);
		if(NT_ERROR(st))IoDeleteDevice(NoirDeviceObject);
		NoirDriverObject=DriverObject;
		st=STATUS_SUCCESS;
	}
	return st;
}