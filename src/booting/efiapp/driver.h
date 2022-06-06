/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the UEFI Runtime Driver Framework.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /booting/efiapp/driver.h
*/

#include <Uefi.h>
#include <Pi/PiMultiPhase.h>
#include <Protocol/DevicePathFromText.h>
#include <Protocol/DevicePathToText.h>
#include <Protocol/DevicePathUtilities.h>
#include <Protocol/HiiFont.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/UgaDraw.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/MpService.h>
#include <Guid/Acpi.h>
#include <Guid/GlobalVariable.h>
#include <Library/UefiLib.h>

#define EFI_NOIRVISOR_VENDOR_GUID	{0x2B1F2A1E,0xDBDF,0x44AC,{0xDA,0xBC,0xC7,0xA1,0x30,0xE2,0xE7,0x1E}}

typedef struct _HYPERVISOR_SYSTEM_IDENTITY
{
	UINT32 BuildNumber;
	union
	{
		struct
		{
			UINT32 MinorVersion:16;
			UINT32 MajorVersion:16;
		};
		UINT32 Version;
	};
	UINT32 ServicePack;
	union
	{
		struct
		{
			UINT32 ServiceNumber:24;
			UINT32 ServiceBranch:8;
		};
		UINT32 Service;
	};
}HYPERVISOR_SYSTEM_IDENTITY,*PHYPERVISOR_SYSTEM_IDENTITY;

typedef struct _HYPERVISOR_LAYERING_PASSCODE
{
	UINT8 Length;
	CHAR8 Text[255];
}HYPERVISOR_LAYERING_PASSCODE,*PHYPERVISOR_LAYERING_PASSCODE;

EFI_STATUS EFIAPI UefiBootServicesTableLibConstructor(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS EFIAPI UefiRuntimeServicesTableLibConstructor(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS EFIAPI UefiLibConstructor(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS EFIAPI DevicePathLibConstructor(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable);

void NoirInitializeSerialPort(IN UINTN ComPort,IN UINT16 PortBase);
void __cdecl NoirDebugPrint(IN CONST CHAR8 *Format,...);
void NoirDisplayProcessorState();
EFI_STATUS NoirBuildHostEnvironment();
UINT32 NoirBuildHypervisor();
BOOLEAN NoirInitializeCodeIntegrity(IN VOID* ImageBase);
void NoirFinalizeCodeIntegrity();
void NoirSuppressImageRelocation(IN VOID* ImageBase);

EFI_GUID gEfiMpServicesProtocolGuid=EFI_MP_SERVICES_PROTOCOL_GUID;
EFI_GUID gEfiSimpleFileSystemProtocolGuid=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
EFI_GUID gEfiGraphicsOutputProtocolGuid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
EFI_GUID gEfiUgaDrawProtocolGuid=EFI_UGA_DRAW_PROTOCOL_GUID;
EFI_GUID gEfiHiiFontProtocolGuid=EFI_HII_FONT_PROTOCOL_GUID;
EFI_GUID gEfiGlobalVariableGuid=EFI_GLOBAL_VARIABLE;
EFI_GUID gEfiSimpleTextOutProtocolGuid=EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;
EFI_GUID gEfiDevicePathProtocolGuid=EFI_DEVICE_PATH_PROTOCOL_GUID;
EFI_GUID gEfiDevicePathUtilitiesProtocolGuid=EFI_DEVICE_PATH_UTILITIES_PROTOCOL_GUID;
EFI_GUID gEfiDevicePathToTextProtocolGuid=EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID;
EFI_GUID gEfiDevicePathFromTextProtocolGuid=EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL_GUID;
EFI_GUID gEfiLoadedImageProtocolGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_GUID gEfiAcpi10TableGuid=ACPI_10_TABLE_GUID;
EFI_GUID gEfiAcpi20TableGuid=EFI_ACPI_20_TABLE_GUID;
EFI_GUID gEfiNoirVisorVendorGuid=EFI_NOIRVISOR_VENDOR_GUID;

HYPERVISOR_SYSTEM_IDENTITY HvSysId={0};
HYPERVISOR_LAYERING_PASSCODE Passcode={0};
CHAR8 LayeringPasscode[]="NoirVisor#ZT#14y3r3d####Nv#1";

EFI_SIMPLE_TEXT_INPUT_PROTOCOL *StdIn=NULL;
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdOut=NULL;
EFI_EVENT NoirEfiExitBootServicesNotification;
BOOLEAN NoirEfiInRuntimeStage=FALSE;
EFI_MP_SERVICES_PROTOCOL *MpServices=NULL;

extern EFI_BOOT_SERVICES *gBS;
extern EFI_RUNTIME_SERVICES *gRT;
extern EFI_SYSTEM_TABLE *gST;