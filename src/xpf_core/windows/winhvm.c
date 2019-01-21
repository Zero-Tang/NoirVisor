/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018, Zero Tang. All rights reserved.

  This file is the HyperVisor Invoker on Windows Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/windows/winhvm.c
*/

#include <ntddk.h>
#include <windef.h>
<<<<<<< HEAD
#include <nvstatus.h>
=======
>>>>>>> intel_vt-x_dev
#include "winhvm.h"

ULONG NoirBuildHypervisor()
{
	return nvc_build_hypervisor();
}

void NoirTeardownHypervisor()
{
	nvc_teardown_hypervisor();
}

<<<<<<< HEAD
ULONG NoirVisorVersion()
{
	return noir_visor_version();
}

void NoirGetVendorString(OUT PSTR VendorString)
{
	noir_get_vendor_string(VendorString);
}

void NoirGetProcessorName(OUT PSTR ProcessorName)
{
	noir_get_processor_name(ProcessorName);
=======
void NoirSaveImageInfo(IN PDRIVER_OBJECT DriverObject)
{
	if(DriverObject)
	{
		NvImageBase=DriverObject->DriverStart;
		NvImageSize=DriverObject->DriverSize;
	}
}

void nvc_store_image_info(OUT PVOID* Base,OUT PULONG Size)
{
	if(Base)*Base=NvImageBase;
	if(Size)*Size=NvImageSize;
>>>>>>> intel_vt-x_dev
}