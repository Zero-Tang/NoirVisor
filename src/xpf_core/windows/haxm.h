/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the Intel HAXM Device Dispatcher on NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/windows/haxm.h
*/

#include <ntddk.h>
#include <windef.h>

// Intel HAXM Device Name
#define HAX_DEVICE_NAME			L"\\Device\\HAX"
#define HAX_LINK_NAME			L"\\DosDevices\\HAX"
// Intel HAXM VM/vCPU Device Name Formats
#define HAX_VM_DEVICE_FORMAT	L"\\Device\\hax_vm%02u"
#define HAX_VP_DEVICE_FORMAT	L"\\Device\\hax_vm%02u_vcpu%02u"
#define HAX_VM_LINK_FORMAT		L"\\DosDevices\\hax_vm%02u"
#define HAX_VP_LINK_FORMAT		L"\\DosDevices\\hax_vm%02u_vcpu%02u"

#define HAX_DEVEXT_TYPE_UP		0x1
#define HAX_DEVEXT_TYPE_VM		0x2
#define HAX_DEVEXT_TYPE_VCPU	0x3

PSTR HaxDeviceTypeString[4]=
{
	"HaxNone",
	"HaxUP",
	"HaxVM",
	"HAXVP"
};

#define MAX_HAX_VM_COUNT	8
#define MAX_HAX_VP_COUNT	16

typedef struct _HAX_VP_OBJECT
{
	PDEVICE_OBJECT DeviceObject;
	PMDL TunnelMdl;
	PMDL IoBuffMdl;
	PVOID KernelTunnel;
	PVOID KernelIoBuff;
	PVOID UserTunnel;
	PVOID UserIoBuff;
	UNICODE_STRING LinkName;
	WCHAR LinkNameBuffer[32];
}HAX_VP_OBJECT,*PHAX_VP_OBJECT;

typedef struct _HAX_VM_OBJECT
{
	// If Handle is -1, this object is free.
	CVM_HANDLE Handle;
	PDEVICE_OBJECT DeviceObject;
	// For HAXM, Each VM supports up to 16 vCPUs.
	HAX_VP_OBJECT VpList[MAX_HAX_VP_COUNT];
	UNICODE_STRING LinkName;
	WCHAR LinkNameBuffer[32];
}HAX_VM_OBJECT,*PHAX_VM_OBJECT;

typedef struct _HAX_DEVICE
{
	ULONG32 volatile Counter;
}HAX_DEVICE,*PHAX_DEVICE;

typedef struct _HAX_VM
{
	ULONG32 VmId;
	PHAX_VM_OBJECT Hax;
}HAX_VM,*PHAX_VM;

typedef struct _HAX_VP
{
	ULONG32 VmId;
	ULONG32 VpId;
	PHAX_VM_OBJECT HaxVm;
	PHAX_VP_OBJECT HaxVp;
}HAX_VP,*PHAX_VP;

typedef struct _HAX_DEVICE_EXTENSION
{
	LONG Type;
	union
	{
		HAX_DEVICE HaxDeviceExtension;
		HAX_VM HaxVmExtension;
		HAX_VP HaxVpExtension;
	};
}HAX_DEVICE_EXTENSION,*PHAX_DEVICE_EXTENSION;

typedef struct _HAX_QEMU_VERSION
{
	ULONG32 CurrentVersion;
	ULONG32 LeastVersion;
}HAX_QEMU_VERSION,*PHAX_QEMU_VERSION;

typedef struct _HAX_TUNNEL_INFO
{
	ULONG64 Va;
	ULONG64 IoVa;
	USHORT Size;
	USHORT Pad[3];
}HAX_TUNNEL_INFO,*PHAX_TUNNEL_INFO;

typedef struct _HAX_ALLOC_RAM_INFO
{
	ULONG32 Size;
	ULONG32 Pad;
	ULONG64 VA;
}HAX_ALLOC_RAM_INFO,*PHAX_ALLOC_RAM_INFO;

typedef struct _HAX_RAMBLOCK_INFO
{
	ULONG64 StartVa;
	ULONG64 Size;
	ULONG32 Reserved;
}HAX_RAMBLOCK_INFO,*PHAX_RAMBLOCK_INFO;

typedef struct _HAX_SET_RAM_INFO
{
	ULONG64 GpaStart;
	ULONG32 Size;
	union
	{
		struct
		{
			BYTE ReadOnly:1;
			BYTE Reserved:6;
			BYTE Invalid:1;
		};
		BYTE Value;
	}Flags;
	BYTE Pad[3];
	ULONG64 HvaStart;
}HAX_SET_RAM_INFO,*PHAX_SET_RAM_INFO;

typedef struct _HAX_SET_RAM_INFO2
{
	ULONG64 GpaStart;
	ULONG64 Size;
	ULONG64 HvaStart;
	struct
	{
		ULONG32 ReadOnly:1;
		ULONG32 Reserved1:5;
		ULONG32 Standalone:1;
		ULONG32 Invalid:1;
		ULONG32 Reserved2:24;
	}Flags;
	ULONG32 Reserved1;
	ULONG64 Reserved2;
}HAX_SET_RAM_INFO2,*PHAX_SET_RAM_INFO2;

#define HAX_DEVICE_TYPE		0x4000
#define HAX_CTL_CODE(i)		CTL_CODE(HAX_DEVICE_TYPE,i,METHOD_BUFFERED,FILE_ANY_ACCESS)

#define HAX_IOCTL_VERSION			HAX_CTL_CODE(0x900)
#define HAX_IOCTL_CREATE_VM			HAX_CTL_CODE(0x901)
#define HAX_IOCTL_CAPABILITY		HAX_CTL_CODE(0x910)
#define HAX_IOCTL_SET_MEMLIMIT		HAX_CTL_CODE(0x911)

#define HAX_VM_IOCTL_VCPU_CREATE	HAX_CTL_CODE(0x902)
#define HAX_VM_IOCTL_ALLOC_RAM		HAX_CTL_CODE(0x903)
#define HAX_VM_IOCTL_SET_RAM		HAX_CTL_CODE(0x904)
#define HAX_VM_IOCTL_VCPU_DESTROY	HAX_CTL_CODE(0x905)
#define HAX_VM_IOCTL_ADD_RAMBLOCK	HAX_CTL_CODE(0x913)
#define HAX_VM_IOCTL_SET_RAM2		HAX_CTL_CODE(0x914)
#define HAX_VM_IOCTL_PROTECT_RAM	HAX_CTL_CODE(0x915)

#define HAX_VCPU_IOCTL_RUN			HAX_CTL_CODE(0x906)
#define HAX_VCPU_IOCTL_SET_MSRS		HAX_CTL_CODE(0x907)
#define HAX_VCPU_IOCTL_GET_MSRS		HAX_CTL_CODE(0x908)
#define HAX_VCPU_IOCTL_SET_FPU		HAX_CTL_CODE(0x909)
#define HAX_VCPU_IOCTL_GET_FPU		HAX_CTL_CODE(0x90A)
#define HAX_VCPU_IOCTL_SETUP_TUNNEL	HAX_CTL_CODE(0x90B)
#define HAX_VCPU_IOCTL_INTERRUPT	HAX_CTL_CODE(0x90C)
#define HAX_VCPU_IOCTL_SET_REGS		HAX_CTL_CODE(0x90D)
#define HAX_VCPU_IOCTL_GET_REGS		HAX_CTL_CODE(0x90E)
#define HAX_VCPU_IOCTL_KICKOFF		HAX_CTL_CODE(0x90F)

// Intel HAX API Version 2.0 for vCPU
#define HAX_VM_IOCTL_NOTIFY_QEMU_VERSION	HAX_CTL_CODE(0x910)
#define HAX_IOCTL_VCPU_DEBUG				HAX_CTL_CODE(0x916)
#define HAX_VCPU_IOCTL_SET_CPUID			HAX_CTL_CODE(0x917)
#define HAX_VCPU_IOCTL_GET_CPUID			HAX_CTL_CODE(0x918)

PVOID NoirGetInputBuffer(IN PIRP Irp);
PVOID NoirGetOutputBuffer(IN PIRP Irp);

NOIR_STATUS nvc_hax_get_version_info(OUT PVOID Buffer,IN ULONG32 Size,OUT PULONG32 ReturnSize OPTIONAL);
NOIR_STATUS nvc_hax_get_capability(OUT PVOID Buffer,IN ULONG32 Size,OUT PULONG32 ReturnSize OPTIONAL);
NOIR_STATUS nvc_hax_set_memlimit(IN PVOID Buffer,IN ULONG32 Size,OUT PULONG32 ReturnSize OPTIONAL);
NOIR_STATUS nvc_hax_set_mapping(IN PVOID VirtualMachine,IN PHAX_SET_RAM_INFO RamInfo);
NOIR_STATUS nvc_hax_set_tunnel(IN PVOID VirtualProcessor,IN PVOID Tunnel,IN PVOID IoBuff);
NOIR_STATUS nvc_hax_set_vcpu_registers(IN PVOID VirtualMachine,IN PVOID Buffer,IN ULONG32 Size,OUT PULONG32 ReturnSize OPTIONAL);
NOIR_STATUS nvc_hax_get_vcpu_registers(IN PVOID VirtualProcessor,OUT PVOID Buffer,IN ULONG32 Size,OUT PULONG32 ReturnSize OPTIONAL);
NOIR_STATUS nvc_hax_set_fpu_state(IN PVOID VirtualMachine,IN PVOID Buffer,IN ULONG32 Size,OUT PULONG32 ReturnSize OPTIONAL);
NOIR_STATUS nvc_hax_get_fpu_state(IN PVOID VirtualProcessor,OUT PVOID Buffer,IN ULONG32 Size,OUT PULONG32 ReturnSize OPTIONAL);
NOIR_STATUS nvc_hax_set_msrs(IN PVOID VirtualMachine,IN PVOID Buffer,IN ULONG32 Size,OUT PULONG32 ReturnSize OPTIONAL);
NOIR_STATUS nvc_hax_get_msrs(IN PVOID VirtualProcessor,OUT PVOID Buffer,IN ULONG32 Size,OUT PULONG32 ReturnSize OPTIONAL);
NOIR_STATUS nvc_hax_run_vcpu(IN PVOID VirtualProcessor);

NOIR_STATUS nvc_register_memblock(IN PVOID VirtualMachine,IN ULONG64 Start,IN ULONG64 Size,IN PVOID Host);

extern PDRIVER_OBJECT NoirDriverObject;
extern PDEVICE_OBJECT HaxDeviceObject;

// Intel HAXM supports up to eight VMs
HAX_VM_OBJECT HaxVmList[MAX_HAX_VM_COUNT];
EX_PUSH_LOCK HaxVmDeviceListLock={0};