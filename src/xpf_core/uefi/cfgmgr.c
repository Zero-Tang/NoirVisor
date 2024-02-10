/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the configuration manager on UEFI Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/cfgmgr.c
*/

#include <Uefi.h>
#include <Guid/FileInfo.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Protocol/SimpleFileSystem.h>
#include "cfgmgr.h"

void NoirFinalizeConfigurationManager()
{
	if(NoirConfigurationList)
	{
		FreePool(NoirConfigurationList);
		NoirConfigurationList=NULL;
	}
}

EFI_STATUS NoirGetConfigurationRecord(IN CHAR8* RecordName,OUT UINT32* RecordType,OUT VOID* RecordData,IN UINT32 RecordLength,OUT UINT32* OutputLength)
{
	EFI_STATUS st=EFI_NOT_FOUND;
	INT32 Lo=0,Hi=NoirConfigurationList->NumberOfRecords;
	while(Hi>=Lo)
	{
		INT32 Mid=(Hi+Lo)>>1;
		PNOIR_CONFIGURATION_RECORD Cur=(PNOIR_CONFIGURATION_RECORD)((UINTN)NoirConfigurationList+NoirConfigurationList->RecordOffsets[Mid]);
		INTN CompareResult=AsciiStrnCmp(Cur->RecordName,RecordName,Cur->RecordNameLength);
		if(CompareResult>0)
			Hi=Mid-1;
		else if(CompareResult<0)
			Lo=Mid+1;
		else
		{
			UINTN RecordValueOffset=sizeof(NOIR_CONFIGURATION_RECORD)+Cur->RecordNameLength;
			if(RecordValueOffset&3)RecordValueOffset+=4-(RecordValueOffset&3);
			*RecordType=Cur->RecordType;
			if(OutputLength)*OutputLength=Cur->RecordDataLength;
			if(RecordLength<Cur->RecordDataLength)
				st=EFI_BUFFER_TOO_SMALL;
			else
			{
				void* RecordValue=(void*)((UINTN)Cur+RecordValueOffset);
				CopyMem(RecordData,RecordValue,Cur->RecordDataLength);
				st=EFI_SUCCESS;
				break;
			}
		}
	}
	return st;
}

void NoirEnumerateConfigurations()
{
	for(UINT32 i=0;i<NoirConfigurationList->NumberOfRecords;i++)
	{
		PNOIR_CONFIGURATION_RECORD Record=(PNOIR_CONFIGURATION_RECORD)((UINTN)NoirConfigurationList+NoirConfigurationList->RecordOffsets[i]);
		UINTN RecordValueOffset=sizeof(NOIR_CONFIGURATION_RECORD)+Record->RecordNameLength;
		if(RecordValueOffset&3)RecordValueOffset+=4-(RecordValueOffset&3);
		Print(L"Record #%u: Type: %a\t Name: %a\t Value: ",i,RecordTypeNames[Record->RecordType],Record->RecordName);
		switch(Record->RecordType)
		{
			case CONFIG_TYPE_STRING:
			{
				CHAR8* RecordValue=(CHAR8*)((UINTN)Record+RecordValueOffset);
				Print(L"%a\n",RecordValue);
				break;
			}
			case CONFIG_TYPE_INTEGER:
			{
				UINT32 RecordValue=*(UINT32*)((UINTN)Record+RecordValueOffset);
				Print(L"0x%X\n",RecordValue);
				break;
			}
			case CONFIG_TYPE_BOOLEAN:
			{
				BOOLEAN RecordValue=*(BOOLEAN*)((UINTN)Record+RecordValueOffset);
				Print(RecordValue?L"True\n":L"False\n");
				break;
			}
		}
	}
}

EFI_STATUS NoirInitializeConfigurationManager()
{
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSysProtocol=NULL;
	EFI_STATUS st=gBS->HandleProtocol(RootDeviceHandle,&gEfiSimpleFileSystemProtocol,&FileSysProtocol);
	if(st==EFI_SUCCESS)
	{
		EFI_FILE_PROTOCOL* RootFile=NULL;
		st=FileSysProtocol->OpenVolume(FileSysProtocol,&RootFile);
		if(st==EFI_SUCCESS)
		{
			EFI_FILE_PROTOCOL* CfgFile=NULL;
			st=RootFile->Open(RootFile,&CfgFile,CONFIG_FILE_PATH,EFI_FILE_MODE_READ,0);
			if(st==EFI_SUCCESS)
			{
				// Get File Size of configuration file.
				// Seek & Peek is the easiest method. You don't need to probe and allocate stuff for the info.
				UINT64 FileSize;
				st=CfgFile->SetPosition(CfgFile,MAX_UINT64);
				if(st==EFI_SUCCESS)st=CfgFile->GetPosition(CfgFile,&FileSize);
				if(st==EFI_SUCCESS)
				{
					CfgFile->SetPosition(CfgFile,0);
					Print(L"Size of configuration file: %llu Bytes\n",FileSize);
					NoirConfigurationList=AllocateRuntimePool(FileSize);
					if(NoirConfigurationList)
					{
						st=CfgFile->Read(CfgFile,&FileSize,NoirConfigurationList);
						Print(L"Configuration File Read Status: 0x%X\n",st);
					}
				}
				CfgFile->Close(CfgFile);
			}
			else
			{
				Print(L"Failed to open configuration file! Status: 0x%X\n",st);
			}
			RootFile->Close(RootFile);
		}
	}
	return st;
}