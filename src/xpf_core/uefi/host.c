/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file builds the host environment for NoirVisor on UEFI.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/host.c
*/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Pi/PiMultiPhase.h>
#include <Protocol/MpService.h>
#include <Register/Intel/ArchitecturalMsr.h>
#include <Register/Intel/Cpuid.h>
#include <stdarg.h>
#include "host.h"

// The format specifiers defined by EDK II are different from the standard library.
void static NoirConvertStandardFormatToEdk2Format(CONST CHAR8* StandardFormat,CHAR8* Edk2Format,UINTN FormatBufferSize)
{
	for(UINTN i=0;i<FormatBufferSize;i++)
	{
		Edk2Format[i]=StandardFormat[i];
		if(Edk2Format[i]=='\0')break;
		if(Edk2Format[i]=='%')
		{
			// Try to translate format specifiers.
			Edk2Format[i++]='%';
			while(i<FormatBufferSize)
			{
				BOOLEAN StopTraverse=TRUE;
				switch(StandardFormat[i])
				{
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
					case 'l':
					case 'z':
					case '.':
					case '*':
					{
						StopTraverse=FALSE;
						break;
					}
					case 's':
					{
						StopTraverse=TRUE;
						Edk2Format[i]='a';
						break;
					}
					default:
					{
						StopTraverse=TRUE;
						Edk2Format[i]=StandardFormat[i];;
						break;
					}
				}
				if(StopTraverse)break;
				Edk2Format[i]=StandardFormat[i];
				i++;
			}
		}
	}
}

void __cdecl nv_dprintf(const char* format,...)
{
	CHAR8 TempBuffer[512]="[NoirVisor] ";
	CHAR8 FormatBuffer[320];
	UINTN Size,Start;
	va_list arg_list;
	NoirConvertStandardFormatToEdk2Format(format,FormatBuffer,sizeof(FormatBuffer));
	va_start(arg_list,format);
	Start=AsciiStrnLenS(TempBuffer,sizeof(TempBuffer));
	Size=AsciiVSPrint(&TempBuffer[Start],sizeof(TempBuffer)-Start,FormatBuffer,arg_list);
	va_end(arg_list);
	if(!NoirEfiInRuntimeStage)AsciiPrint(TempBuffer,Size);
}

void __cdecl nv_panicf(const char* format,...)
{
	CHAR8 TempBuffer[512]="[NoirVisor - Panic] ";
	CHAR8 FormatBuffer[320];
	UINTN Size,Start;
	va_list arg_list;
	NoirConvertStandardFormatToEdk2Format(format,FormatBuffer,sizeof(FormatBuffer));
	va_start(arg_list,format);
	Start=AsciiStrnLenS(TempBuffer,sizeof(TempBuffer));
	Size=AsciiVSPrint(&TempBuffer[Start],sizeof(TempBuffer)-Start,FormatBuffer,arg_list);
	va_end(arg_list);
	if(!NoirEfiInRuntimeStage)AsciiPrint(TempBuffer,Size);
}

void __cdecl NoirDebugPrint(IN CONST CHAR8 *Format,...)
{
	CPUID_VERSION_INFO_ECX Ecx;
	CHAR8 TempBuffer[320];
	CHAR8 FormatBuffer[320];
	UINTN Size,Start;
	va_list ArgList;
	NoirConvertStandardFormatToEdk2Format(Format,FormatBuffer,sizeof(FormatBuffer));
	AsmCpuid(CPUID_VERSION_INFO,NULL,NULL,&Ecx.Uint32,NULL);
	AsciiSPrint(TempBuffer,sizeof(TempBuffer),"[NoirVisor - %a] ",Ecx.Bits.ParaVirtualized?"Guest":"Host");
	va_start(ArgList,Format);
	Start=AsciiStrnLenS(TempBuffer,sizeof(TempBuffer));
	Size=AsciiVSPrint(&TempBuffer[Start],sizeof(TempBuffer)-Start,FormatBuffer,ArgList);
	va_end(ArgList);
	if(!NoirEfiInRuntimeStage)AsciiPrint(TempBuffer,Size);
}

void* noir_alloc_nonpg_memory(IN UINTN Length)
{
	return AllocateRuntimeZeroPool(Length);
}

void* noir_alloc_contd_memory(IN UINTN Length)
{
	void* p=AllocateRuntimePages(EFI_SIZE_TO_PAGES(Length));
	if(p)ZeroMem(p,Length);
	return p;
}

void* noir_alloc_2mb_page()
{
	void* p=AllocateAlignedRuntimePages(0x200,0x200000);
	if(p)ZeroMem(p,0x200000);
	return p;
}

void noir_free_nonpg_memory(IN VOID* VirtualAddress)
{
	FreePool(VirtualAddress);
}

void noir_free_contd_memory(IN VOID* VirtualAddress,IN UINTN Length)
{
	FreePages(VirtualAddress,EFI_SIZE_TO_PAGES(Length));
}

void noir_free_2mb_page(IN VOID* VirtualAddress)
{
	FreeAlignedPages(VirtualAddress,0x200);
}

UINT64 noir_get_physical_address(IN VOID* VirtualAddress)
{
	// Mapping is identical.
	return (UINT64)VirtualAddress;
}

VOID* noir_find_virt_by_phys(IN UINT64 PhysicalAddress)
{
	// Mapping is identical.
	return (VOID*)PhysicalAddress;
}

void noir_copy_memory(IN VOID* Destination,IN VOID* Source,IN UINT32 Length)
{
	CopyMem(Destination,Source,Length);
}

void static NoirGenericCallRT(IN VOID* ProcedureArgument)
{
	PNV_MP_SERVICE_GENERIC_INFO GenericInfo=(PNV_MP_SERVICE_GENERIC_INFO)ProcedureArgument;
	UINTN ProcId=0;
	if(MpServices)MpServices->WhoAmI(MpServices,&ProcId);
	GenericInfo->Worker(GenericInfo->Context,(UINT32)ProcId);
}

void noir_generic_call(noir_broadcast_worker worker,void* context)
{
	// Initialize Structure for Generic Call.
	NV_MP_SERVICE_GENERIC_INFO GenericInfo;
	GenericInfo.Worker=worker;
	GenericInfo.Context=context;
	if(MpServices)
	{
#if defined(_GPC)
		// We are going to do Generic Calls asynchronously.
		EFI_EVENT GenericCallEvent;
		EFI_STATUS st=gBS->CreateEvent(0,0,NULL,NULL,&GenericCallEvent);
		if(st==EFI_SUCCESS)
		{
			UINTN SignaledIndex;
			// Initiate Asynchronous & Simultaneous Generic Call.
			st=MpServices->StartupAllAPs(MpServices,NoirGenericCallRT,FALSE,GenericCallEvent,0,&GenericInfo,NULL);
			// BSP is not invoked during the Generic Call. Do it here.
			NoirGenericCallRT((VOID*)&GenericInfo);
			// BSP has finished its job for Generic Call. Wait for all APs.
			st=gBS->WaitForEvent(1,&GenericCallEvent,&SingaledIndex);
			gBS->CloseEvent(GenericCallEvent);
		}
#else
		// Do it in BSP before other APs.
		NoirGenericCallRT((VOID*)&GenericInfo);
		// Initiate Synchronous & Serialized Generic Call.
		MpServices->StartupAllAPs(MpServices,NoirGenericCallRT,TRUE,NULL,0,&GenericInfo,NULL);
#endif
	}
	else
	{
		// Only one core is accessible.
		NoirGenericCallRT((VOID*)&GenericInfo);
	}
}

INTN EFIAPI NoirMemoryRangeComparator(IN CONST VOID *Buffer1,IN CONST VOID* Buffer2)
{
	PMEMORY_RANGE a=(PMEMORY_RANGE)Buffer1,b=(PMEMORY_RANGE)Buffer2;
	if(a->BaseAddress<b->BaseAddress)
		return -1;
	else if(a->BaseAddress>b->BaseAddress)
		return 1;
	return 0;
}

void noir_enum_physical_memory_ranges(IN NOIR_PHYSICAL_MEMORY_RANGE_CALLBACK CallbackRoutine,IN OUT VOID* Context)
{
	EFI_MEMORY_DESCRIPTOR *MemoryDescriptor=NULL;
	MEMORY_RANGE *MemoryRanges=NULL,DummyRange;
	UINTN MemoryDescriptorSize=0,MapKey,DescriptorSize;
	UINT32 DescriptorVersion;
	EFI_STATUS st=gBS->GetMemoryMap(&MemoryDescriptorSize,MemoryDescriptor,&MapKey,&DescriptorSize,&DescriptorVersion);
	if(st==EFI_BUFFER_TOO_SMALL)
	{
		MemoryDescriptorSize+=DescriptorSize<<1;
		MemoryDescriptor=AllocatePool(MemoryDescriptorSize);
		MemoryRanges=AllocatePool(MemoryDescriptorSize);
		if(MemoryDescriptor==NULL || MemoryRanges==NULL)
		{
			if(MemoryDescriptor)FreePool(MemoryDescriptor);
			if(MemoryRanges)FreePool(MemoryDescriptor);
			st=EFI_OUT_OF_RESOURCES;
		}
		else
		{
			EFI_MEMORY_DESCRIPTOR *CurrentEntry=MemoryDescriptor;
			st=gBS->GetMemoryMap(&MemoryDescriptorSize,MemoryDescriptor,&MapKey,&DescriptorSize,&DescriptorVersion);
			if(st!=EFI_SUCCESS)
				Print(L"Failed to get memory map! Status=0x%X\n",st);
			else
			{
				UINTN Ranges=0;
				do
				{
					/*
					CHAR8 *MemoryTypeName=CurrentEntry->Type<EfiMaxMemoryType?EfiMemoryTypeNames[CurrentEntry->Type]:"Unknown";
					Print(L"Memory Map Start: 0x%llX\t Pages: 0x%llX\t Type: %a\t Attributes: ",CurrentEntry->PhysicalStart,CurrentEntry->NumberOfPages,MemoryTypeName);
					if(CurrentEntry->Attribute==0)
						Print(L"None\n");
					else
					{
						for(UINT8 i=0;i<64;i++)
							if(_bittest64(&CurrentEntry->Attribute,i))
								Print(L"%a ",EfiMemoryAttributeNames[i]);
						Print(L"\n");
					}
					*/
					switch(CurrentEntry->Type)
					{
						case EfiLoaderCode:
						case EfiLoaderData:
						case EfiBootServicesCode:
						case EfiBootServicesData:
						case EfiRuntimeServicesCode:
						case EfiRuntimeServicesData:
						case EfiConventionalMemory:
						case EfiACPIReclaimMemory:
						{
							// These types of memory could be treated as General-Purpose Physical Memory.
							// Add to ranges.
							MemoryRanges[Ranges].BaseAddress=CurrentEntry->PhysicalStart;
							MemoryRanges[Ranges].RangeSize=CurrentEntry->NumberOfPages<<EFI_PAGE_SHIFT;
							Ranges++;
							break;
						}
					}
					CurrentEntry=(EFI_MEMORY_DESCRIPTOR*)((UINTN)CurrentEntry+DescriptorSize);
				}while((UINTN)CurrentEntry<(UINTN)MemoryDescriptor+MemoryDescriptorSize);
				// Sort the memory range.
				QuickSort(MemoryRanges,Ranges,sizeof(MEMORY_RANGE),NoirMemoryRangeComparator,&DummyRange);
				// Merge the ranges.
				UINT32 BaseIndex=0;
				for(UINT32 i=1;i<Ranges;i++)
				{
					PMEMORY_RANGE a=&MemoryRanges[BaseIndex],b=&MemoryRanges[i];
					if(a->BaseAddress+a->RangeSize>=b->BaseAddress && a->BaseAddress+a->RangeSize<=b->BaseAddress+b->RangeSize)
						a->RangeSize=b->BaseAddress+b->RangeSize-a->BaseAddress;
					else
						MemoryRanges[++BaseIndex]=*b;
				}
				for(UINT32 i=0;i<=BaseIndex;i++)
					CallbackRoutine(MemoryRanges[i].BaseAddress,MemoryRanges[i].RangeSize,Context);
			}
			FreePool(MemoryDescriptor);
			FreePool(MemoryRanges);
		}
	}
	Print(L"Query Memory Map Layout Status: 0x%X\n",st);
}

UINT32 noir_get_current_processor()
{
	if(!NoirEfiInRuntimeStage && MpServices)
	{
		UINTN Num;
		EFI_STATUS st=MpServices->WhoAmI(MpServices,&Num);
		if(st==EFI_SUCCESS)return (UINT32)Num;
	}
	return 0;
}

UINT32 noir_get_processor_count()
{
	if(MpServices)
	{
		UINTN Num1,Num2;
		EFI_STATUS st=MpServices->GetNumberOfProcessors(MpServices,&Num1,&Num2);
		if(st==EFI_SUCCESS)return (UINT32)Num1;
	}
	return 1;
}

void noir_qsort(IN OUT VOID* Base,IN UINT32 Number,IN UINT32 Width,IN BASE_SORT_COMPARE CompareFunction)
{
	UINT8 Buffer[1024];		// 1024 bytes should be enough.
	if(Width<=sizeof(Buffer))QuickSort(Base,Number,Width,CompareFunction,Buffer);
}

UINT32 noir_get_instruction_length(IN VOID* code,IN BOOLEAN LongMode)
{
	if(LongMode)
		return NoirGetInstructionLength64(code,0);
	return NoirGetInstructionLength32(code,0);
}

UINT32 noir_get_instruction_length_ex(IN VOID* Code,IN UINT8 Bits)
{
	switch(Bits)
	{
	case 16:
		return NoirGetInstructionLength16(Code,0);
	case 32:
		return NoirGetInstructionLength32(Code,0);
	case 64:
		return NoirGetInstructionLength64(Code,0);
	default:
		return 0;
	}
}

UINT32 noir_disasm_instruction(IN VOID* Code,OUT CHAR8 *Mnemonic,IN UINTN MnemonicLength,IN UINT8 Bits,IN UINT64 VirtualAddress)
{
	switch(Bits)
	{
	case 16:
		return NoirDisasmCode16(Mnemonic,MnemonicLength,Code,15,VirtualAddress);
	case 32:
		return NoirDisasmCode32(Mnemonic,MnemonicLength,Code,15,VirtualAddress);
	case 64:
		return NoirDisasmCode64(Mnemonic,MnemonicLength,Code,15,VirtualAddress);
	default:
		return 0;
	}
}

UINT64 noir_get_system_time()
{
	// Use HPET to get the time.
	// The return value has unit of 100ns.
	// Do not use Runtime Service by virtue of complex calendar calculation.
	if(hpet_period!=0xFFFFFFFF)
	{
		// Number of Ticks as indicated by HPET.
		UINT64 HpetTicks=nvc_hpet_read_counter();
		// Number of Femptoseconds as indicated by HPET.
		UINT64 HpetFs=MultU64x32(HpetTicks,hpet_period);
		// Divide by 100,000,000.
		return HpetFs/100000000;
	}
	return 0;
}