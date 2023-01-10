/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This header defines operations with High-Pricision Event Timer (HPET).

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/hpet.h
*/

#include <Uefi.h>
#include <IndustryStandard/HighPrecisionEventTimerTable.h>

/*
  The HPET specification can be found in Intel's website.

  http://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/software-developers-hpet-spec-1-0a.pdf

  The HPET specification is a support document of ACPI.
  See https://uefi.org/acpi for further details.
*/

#define HpetGeneralCapabilitiesId					(0x000)
#define HpetGeneralCounterClockPeriod				(0x004)
#define HpetGeneralConfiguration					(0x010)
#define HpetGeneralInterruptStatus					(0x020)
#define HpetMainCounterValue						(0x0F0)
#define HpetTimerConfigurationAndCapability(N)		(0x100+(N<<5)+0x00)
#define HpetTimerConfigurationAndCapabilityHigh(N)	(0x100+(N<<5)+0x04)
#define HpetTimerComparatorValue(N)					(0x100+(N<<5)+0x08)
#define HpetTimerComparatorValueHigh(N)				(0x100+(N<<5)+0x0C)
#define HpetTimerFsbInterruptRoute(N)				(0x100+(N<<5)+0x10)
#define HpetTimerFsbInterruptRouteHigh(N)			(0x100+(N<<5)+0x14)
#define HpetTimerFsbInterruptValue(N)				(0x100+(N<<5)+0x10)
#define HpetTimerFsbInterruptAddress(N)				(0x100+(N<<5)+0x14)

typedef union _HPET_GENERAL_CAPABILITY_ID
{
	struct
	{
		UINT64 RevisionId:8;
		UINT64 NumberOfTimers:5;
		UINT64 CounterSize:1;
		UINT64 Reserved:1;
		UINT64 LegacyReplacementRoute:1;
		UINT64 VendorId:16;
		UINT64 CounterClockPeriod:32;
	};
	UINT64 Value;
}HPET_GENERAL_CAPABILITY_ID;

typedef union _HPET_GENERAL_CONFIGURATION
{
	struct
	{
		UINT64 Enable:1;
		UINT64 LegacyReplacementRoute:1;
		UINT64 Reserved0:6;
		UINT64 ReservedNonOS:8;
		UINT64 Reserved1:48;
	};
	UINT64 Value;
}HPET_GENERAL_CONFIGURATION;

// It is recommended to use bit-test instructions to check stuff, though.
typedef union _HPET_GENERAL_INTERRUPT_STATUS
{
	struct
	{
		UINT64 Timer0InterruptStatus:1;
		UINT64 Timer1InterruptStatus:1;
		UINT64 Timer2InterruptStatus:1;
		UINT64 ReservedForOtherTimers:29;
		UINT64 Reserved:32;
	};
	UINT64 Value;
}HPET_GENERAL_INTERRUPT_STATUS;

typedef union _HPET_TIMER_CONFIGURATION_CAPABILITIES
{
	struct
	{
		UINT64 Reserved0:1;
		UINT64 InterruptType:1;
		UINT64 InterruptEnable:1;
		UINT64 TimerType:1;
		UINT64 PeriodInterrupt:1;
		UINT64 TimerSize:1;
		UINT64 TimerValueSet:1;
		UINT64 Reserved1:1;
		UINT64 Timer32BitMode:1;
		UINT64 InterruptRoute:5;
		UINT64 FsbRouteEnable:1;
		UINT64 Reserved2:16;
		UINT64 RoutingCapability:32;
	};
	UINT64 Value;
}HPET_TIMER_CONFIGURATION_CAPABILITIES;

extern EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdOut;

UINT64 NoirHpetHardware=0;
UINT32 NoirHpetIncrementingPeriod=0xFFFFFFFF;