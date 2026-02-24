/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2026, Zero Tang. All rights reserved.

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
	if(IS_CTL_CODE_CUSTOM(IoCtrlCode))
	{
		ULONG FuncCode=FUNCTION_FROM_CTL_CODE(IoCtrlCode);
		ULONG i=FuncCode>>4,j=FuncCode&15;
		if(i<16)
		{
			IO_DISPATCH_ROUTINE DispatchFn=NoirDispatchIoGroups[i][j];
			st=DispatchFn(InputBuffer,InputSize,OutputBuffer,OutputSize);
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