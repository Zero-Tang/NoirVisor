/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the kernel-mode driver framework of Windows.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /booting/windrv/driver.c
*/

#include <ntddk.h>
#include <windef.h>
#include "driver.h"

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
	NoirReportMemoryIntrospectionCounter();
	NoirFinalizeAsyncDebugPrinter();
	IoDeleteSymbolicLink(&uniLinkName);
	IoDeleteDevice(NoirDeviceObject);
	NoirHaxFinalizeDeviceExtension();
}

NTSTATUS NoirDispatchCreate(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp)
{
	if(DeviceObject==HaxDeviceObject)return NoirHaxDispatchCreate(DeviceObject,Irp);
	Irp->IoStatus.Status=STATUS_SUCCESS;
	Irp->IoStatus.Information=0;
	IoCompleteRequest(Irp,IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS NoirDispatchClose(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp)
{
	if(DeviceObject==HaxDeviceObject)return NoirHaxDispatchClose(DeviceObject,Irp);
	Irp->IoStatus.Status=STATUS_SUCCESS;
	Irp->IoStatus.Information=0;
	IoCompleteRequest(Irp,IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS NoirVisorDispatchIoControl(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp)
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
		case IOCTL_VirtEn:
		{
			st=STATUS_SUCCESS;
			*(PBOOLEAN)OutputBuffer=NoirIsVirtualizationEnabled();
			break;
		}
		case IOCTL_CvmCreateVm:
		{
			PCVM_HANDLE VmHandle=(PCVM_HANDLE)((ULONG_PTR)OutputBuffer+sizeof(CVM_HANDLE));
			*(PULONG32)OutputBuffer=NoirCreateVirtualMachine(VmHandle);
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmDeleteVm:
		{
			CVM_HANDLE VmHandle=*(PCVM_HANDLE)InputBuffer;
			*(PULONG32)OutputBuffer=NoirReleaseVirtualMachine(VmHandle);
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmSetMapping:
		{
			PNOIR_ADDRESS_MAPPING MapInfo=(PNOIR_ADDRESS_MAPPING)InputBuffer;
			CVM_HANDLE VmHandle=*(PCVM_HANDLE)((ULONG_PTR)InputBuffer+sizeof(NOIR_ADDRESS_MAPPING));
			*(PULONG32)OutputBuffer=NoirSetMapping(VmHandle,MapInfo);
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmQueryGpaAdMap:
		{
			PNOIR_QUERY_ADBITMAP_CONTEXT Param=(PNOIR_QUERY_ADBITMAP_CONTEXT)InputBuffer;
			*(PULONG32)OutputBuffer=NoirQueryGpaAccessingBitmap(Param->VirtualMachine,Param->GpaStart,Param->NumberOfPages,(PVOID)Param->BitmapBuffer,Param->BitmapLength);
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmClearGpaAdBit:
		{
			PNOIR_QUERY_ADBITMAP_CONTEXT Param=(PNOIR_QUERY_ADBITMAP_CONTEXT)InputBuffer;
			*(PULONG32)OutputBuffer=NoirClearGpaAccessingBits(Param->VirtualMachine,Param->GpaStart,Param->NumberOfPages);
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmCreateVmEx:
		{
			PULONG32 Input=(PULONG32)InputBuffer;
			PCVM_HANDLE VmHandle=(PCVM_HANDLE)((ULONG_PTR)OutputBuffer+sizeof(CVM_HANDLE));
			*(PULONG32)OutputBuffer=NoirCreateVirtualMachineEx(VmHandle,Input[0]);
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmQueryHvStatus:
		{
			ULONG64 StType=*(PULONG64)((ULONG_PTR)InputBuffer);
			PVOID Status=(PULONG64)((ULONG_PTR)OutputBuffer+8);
			*(PULONG32)OutputBuffer=NoirQueryHypervisorStatus(StType,Status);
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmCreateVcpu:
		{
			CVM_HANDLE VmHandle=*(PCVM_HANDLE)InputBuffer;
			ULONG32 VpIndex=*(PULONG32)((ULONG_PTR)InputBuffer+sizeof(CVM_HANDLE));
			*(PULONG32)OutputBuffer=NoirCreateVirtualProcessor(VmHandle,VpIndex);
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmDeleteVcpu:
		{
			CVM_HANDLE VmHandle=*(PCVM_HANDLE)InputBuffer;
			ULONG32 VpIndex=*(PULONG32)((ULONG_PTR)InputBuffer+sizeof(CVM_HANDLE));
			*(PULONG32)OutputBuffer=NoirReleaseVirtualProcessor(VmHandle,VpIndex);
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmRunVcpu:
		{
			st=STATUS_SUCCESS;
			if(OutputSize<noir_cvm_exit_context_size)
			{
				if(OutputSize<sizeof(ULONG64))
					st=STATUS_INSUFFICIENT_RESOURCES;
				else
				{
					*(PULONG32)OutputBuffer=NOIR_INSUFFICIENT_RESOURCES;
					*(PULONG32)((ULONG_PTR)OutputBuffer+4)=noir_cvm_exit_context_size;
				}
			}
			else
			{
				CVM_HANDLE VmHandle=*(PCVM_HANDLE)InputBuffer;
				ULONG32 VpIndex=*(PULONG32)((ULONG_PTR)InputBuffer+sizeof(CVM_HANDLE));
				PVOID ExitContext=(PVOID)((ULONG_PTR)OutputBuffer+sizeof(ULONG64));
				*(PULONG32)OutputBuffer=NoirRunVirtualProcessor(VmHandle,VpIndex,ExitContext);
			}
			break;
		}
		case IOCTL_CvmViewVcpuReg:
		{
			PNOIR_VIEW_EDIT_REGISTER_CONTEXT Context=(PNOIR_VIEW_EDIT_REGISTER_CONTEXT)InputBuffer;
			PVOID Buffer=(PVOID)((ULONG_PTR)OutputBuffer+8);
			*(PULONG32)OutputBuffer=NoirViewVirtualProcessorRegisters(Context->VirtualMachine,Context->VpIndex,Context->RegisterType,Buffer,OutputSize-8);
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmEditVcpuReg:
		{
			PNOIR_VIEW_EDIT_REGISTER_CONTEXT Context=(PNOIR_VIEW_EDIT_REGISTER_CONTEXT)InputBuffer;
			PVOID Buffer=(PVOID)&Context->DummyBuffer;
			*(PULONG32)OutputBuffer=NoirEditVirtualProcessorRegisters(Context->VirtualMachine,Context->VpIndex,Context->RegisterType,Buffer,InputSize-sizeof(NOIR_VIEW_EDIT_REGISTER_CONTEXT)+sizeof(PVOID));
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmRescindVcpu:
		{
			CVM_HANDLE VmHandle=*(PCVM_HANDLE)InputBuffer;
			ULONG32 VpIndex=*(PULONG32)((ULONG_PTR)InputBuffer+sizeof(CVM_HANDLE));
			*(PULONG32)OutputBuffer=NoirRescindVirtualProcessor(VmHandle,VpIndex);
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmInjectEvent:
		{
			CVM_HANDLE VmHandle=*(PCVM_HANDLE)InputBuffer;
			ULONG32 VpIndex=*(PULONG32)((ULONG_PTR)InputBuffer+sizeof(CVM_HANDLE));
			ULONG64 InjectedEvent=*(PULONG64)((ULONG_PTR)InputBuffer+16);
			*(PULONG32)OutputBuffer=NoirSetEventInjection(VmHandle,VpIndex,InjectedEvent);
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmSetVcpuOptions:
		{
			CVM_HANDLE VmHandle=*(PCVM_HANDLE)InputBuffer;
			ULONG32 VpIndex=*(PULONG32)((ULONG_PTR)InputBuffer+sizeof(CVM_HANDLE));
			ULONG32 OptionType=*(PULONG32)((ULONG_PTR)InputBuffer+16);
			ULONG32 OptionData=*(PULONG32)((ULONG_PTR)InputBuffer+20);
			*(PULONG32)OutputBuffer=NoirSetVirtualProcessorOptions(VmHandle,VpIndex,OptionType,OptionData);
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmQueryVcpuStats:
		{
			CVM_HANDLE VmHandle=*(PCVM_HANDLE)InputBuffer;
			ULONG32 VpIndex=*(PULONG32)((ULONG_PTR)InputBuffer+sizeof(CVM_HANDLE));
			ULONG32 BufferSize=*(PULONG32)((ULONG_PTR)InputBuffer+sizeof(CVM_HANDLE)+4);
			PVOID StatsBuffer=*(PVOID*)((ULONG_PTR)InputBuffer+sizeof(CVM_HANDLE)+8);
			*(PULONG32)OutputBuffer=NoirQueryVirtualProcessorStatistics(VmHandle,VpIndex,StatsBuffer,BufferSize);
			st=STATUS_SUCCESS;
			break;
		}
		case IOCTL_CvmTryEmuExit:
		{
			CVM_HANDLE VmHandle=*(PCVM_HANDLE)InputBuffer;
			ULONG32 VpIndex=*(PULONG32)((ULONG_PTR)InputBuffer+sizeof(CVM_HANDLE));
			PVOID Emulation=*(PVOID*)((ULONG_PTR)InputBuffer+sizeof(CVM_HANDLE)*2);
			*(PULONG32)OutputBuffer=NoirTryCvExitEmulation(VmHandle,VpIndex,Emulation);
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

NTSTATUS NoirDispatchIoControl(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp)
{
	return DeviceObject==NoirDeviceObject?NoirVisorDispatchIoControl(DeviceObject,Irp):NoirHaxDispatchIoControl(DeviceObject,Irp);
}

void static NoirDriverReinitialize(IN PDRIVER_OBJECT DriverObject,IN PVOID Context OPTIONAL,IN ULONG Count)
{
	NTSTATUS st=NoirInitializeAsyncDebugPrinter();
	NoirPrintCompilerVersion();
	NoirInitializeDisassembler();
	NoirInitializeCodeIntegrity(DriverObject->DriverStart);
	NoirLocatePsLoadedModule(DriverObject);
	system_cr3=__readcr3();
	orig_system_call=(ULONG_PTR)__readmsr(0xC0000082);
	NoirGetNtOpenProcessIndex();
	NoirSaveImageInfo(DriverObject);
	NoirGetUefiHypervisionStatus();
	NoirBuildHookedPages();
	NoirBuildProtectedFile();
	NoirInitializePowerStateCallback();
	NoirSubvertSystemOnDriverLoad(&SubvertOnDriverLoad);
	NoirConfigureInternalDebugger();
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
	// Create a HAX Device in order to expose Intel HAXM functions.
	st=NoirHaxInitializeDeviceExtension(DriverObject);
	if(NT_ERROR(st))
	{
		IoDeleteSymbolicLink(&uniLinkName);
		IoDeleteDevice(NoirDeviceObject);
	}
	return st;
}