/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file is the configuration manager on UEFI Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/cfgmgr.h
*/

#include <Uefi.h>

#define CONFIG_FILE_PATH	L"NoirVisorConfig.bin"

#define CONFIG_TYPE_STRING		0
#define CONFIG_TYPE_INTEGER		1
#define CONFIG_TYPE_BOOLEAN		2

typedef struct _NOIR_CONFIGURATION_LIST
{
	UINT32 NumberOfRecords;
	UINT32 RecordOffsets[0];
}NOIR_CONFIGURATION_LIST,*PNOIR_CONFIGURATION_LIST;

typedef struct _NOIR_CONFIGURATION_RECORD
{
	UINT32 RecordType;
	UINT32 RecordNameLength;
	UINT32 RecordDataLength;
	CHAR8 RecordName[0];
}NOIR_CONFIGURATION_RECORD,*PNOIR_CONFIGURATION_RECORD;

EFI_GUID gEfiSimpleFileSystemProtocol=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

PNOIR_CONFIGURATION_LIST NoirConfigurationList=NULL;
CHAR8* RecordTypeNames[3]=
{
	"String",
	"Integer",
	"Boolean"
};

extern EFI_HANDLE RootDeviceHandle;

extern EFI_BOOT_SERVICES *gBS;
extern EFI_RUNTIME_SERVICES *gRT;
extern EFI_SYSTEM_TABLE *gST;