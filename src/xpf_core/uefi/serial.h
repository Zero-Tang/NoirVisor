/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file defines the serial communicator for NoirVisor on UEFI.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/serial.h
*/

#include <Uefi.h>
#include <Protocol/SerialIo.h>

#define IO_PORT_OFFSET_DATA			0
#define IO_PORT_OFFSET_DIVISOR_LO	0
#define IO_PORT_OFFSET_INT_EN		1
#define IO_PORT_OFFSET_DIVISOR_HI	1
#define IO_PORT_OFFSET_INT_ID		2
#define IO_PORT_OFFSET_FIFO_CTRL	2
#define IO_PORT_OFFSET_LINE_CTRL	3
#define IO_PORT_OFFSET_MODEM_CTRL	4
#define IO_PORT_OFFSET_LINE_ST		5
#define IO_PORT_OFFSET_MODEM_ST		6
#define IO_PORT_OFFSET_SCRATCH		7

UINT16 IO_PORT_COM[8]={0x3F8,0x2F8,0x3E8,0x2E8,0x5F8,0x4F8,0x5E8,0x4E8};

#define IO_PORT_BAUD_DIVISOR_LO_115200	0x01
#define IO_PORT_BAUD_DIVISOR_HI_115200	0x00
#define IO_PORT_BAUD_DIVISOR_LO_57600	0x02
#define IO_PORT_BAUD_DIVISOR_HI_57600	0x00
#define IO_PORT_BAUD_DIVISOR_LO_38400	0x03
#define IO_PORT_BAUD_DIVISOR_HI_38400	0x00
#define IO_PORT_BAUD_DIVISOR_LO_19200	0x06
#define IO_PORT_BAUD_DIVISOR_HI_19200	0x00
#define IO_PORT_BAUD_DIVISOR_LO_9600	0x0C
#define IO_PORT_BAUD_DIVISOR_HI_9600	0x00
#define IO_PORT_BAUD_DIVISOR_LO_4800	0x18
#define IO_PORT_BAUD_DIVISOR_HI_4800	0x00
#define IO_PORT_BAUD_DIVISOR_LO_2400	0x30
#define IO_PORT_BAUD_DIVISOR_HI_2400	0x00
#define IO_PORT_BAUD_DIVISOR_LO_600		0xC0
#define IO_PORT_BAUD_DIVISOR_HI_600		0x00
#define IO_PORT_BAUD_DIVISOR_LO_300		0x80
#define IO_PORT_BAUD_DIVISOR_HI_300		0x01
#define IO_PORT_BAUD_DIVISOR_LO_50		0x00
#define IO_PORT_BAUD_DIVISOR_HI_50		0x09

typedef union _IO_PORT_INTERRUPT_ENABLE
{
	struct
	{
		UINT8 ReceivedDataAvailable:1;
		UINT8 TransmitterHoldingEmpty:1;
		UINT8 ReceiverLineStatus:1;
		UINT8 ModemStatus:1;
		UINT8 SleepMode:1;
		UINT8 LowPowerMode:1;
		UINT8 Reserved:2;
	};
	UINT8 Value;
}IO_PORT_INTERRUPT_ENABLE;

#define IO_PORT_FIFO_INT_THRESHOLD_1BYTE	0
#define IO_PORT_FIFO_INT_THRESHOLD_4BYTE	1
#define IO_PORT_FIFO_INT_THRESHOLD_8BYTE	2
#define IO_PORT_FIFO_INT_THRESHOLD_14BYTE	3

typedef union _IO_PORT_FIFO_CONTROL
{
	struct
	{
		UINT8 EnableFifo:1;
		UINT8 ClearReceive:1;
		UINT8 ClearTransmit:1;
		UINT8 DMA:1;
		UINT8 Reserved:1;
		UINT8 Enable64ByteFifo:1;
		UINT8 InterruptThreshold:2;
	};
	UINT8 Value;
}IO_PORT_FIFO_CONTROL;

#define IO_PORT_DATA_BITS_5		0
#define IO_PORT_DATA_BITS_6		1
#define IO_PORT_DATA_BITS_7		2
#define IO_PORT_DATA_BITS_8		3

#define IO_PORT_STOP_BITS_1		0
#define IO_PORT_STOP_BITS_1_5	1
#define IO_PORT_STOP_BITS_2		1

#define IO_PORT_PARITY_NONE		0
#define IO_PORT_PARITY_ODD		1
#define IO_PORT_PARITY_EVEN		3
#define IO_PORT_PARITY_HIGH		5
#define IO_PORT_PARITY_LOW		7

typedef union _IO_PORT_LINE_CONTROL
{
	struct
	{
		UINT8 DataBits:2;
		UINT8 StopBits:1;
		UINT8 Parity:3;
		UINT8 BreakEnable:1;
		UINT8 DLAB:1;
	};
	UINT8 Value;
}IO_PORT_LINE_CONTROL;

typedef union _IO_PORT_MODEM_CONTROL
{
	struct
	{
		UINT8 ForceDTR:1;
		UINT8 ForceRTS:1;
		UINT8 AuxOutput1:1;
		UINT8 AuxOutput2:1;
		UINT8 LoopBackMode:1;
		UINT8 AutoFlowControl:1;
		UINT8 Reserved:2;
	};
	UINT8 Value;
}IO_PORT_MODEM_CONTROL;

typedef union _IO_PORT_LINE_STATUS
{
	struct
	{
		UINT8 DataReady:1;
		UINT8 OverrunError:1;
		UINT8 ParityError:1;
		UINT8 FramingError:1;
		UINT8 BreakInterrupt:1;
		UINT8 EmptyTransmitterHolding:1;
		UINT8 EmptyDataHolding:1;
		UINT8 ReceivedFifoError:1;
	};
	UINT8 Value;
}IO_PORT_LINE_STATUS;

typedef union _IO_PORT_MODEM_STATUS
{
	struct
	{
		UINT8 DeltaCTS:1;
		UINT8 DeltaDSR:1;
		UINT8 TrailingERI:1;
		UINT8 DeltaDCD:1;
		UINT8 CTS:1;
		UINT8 DSR:1;
		UINT8 RI:1;
		UINT8 DCD:1;
	};
	UINT8 Value;
}IO_PORT_MODEM_STATUS;