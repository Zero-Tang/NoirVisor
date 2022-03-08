/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file defines the serial communicator for NoirVisor on UEFI.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/serial.c
*/

#include <Uefi.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <intrin.h>
#include "serial.h"

// Warning: Port-I/O for Serial communication is not guaranteed to be working in UEFI!
// Typically, platforms like VMware do provide such functionality.
// For other platforms, consult the vendor about UART/RS232/RS485 hardware ports.
// All serial communications in this library are done in poll-mode, instead of interrupt-mode.

void NoirInitializeSerialPort(IN UINTN ComPort,IN UINT16 PortBase)
{
	if(ComPort<8)
	{
		IO_PORT_LINE_CONTROL LineCtrl;
		IO_PORT_FIFO_CONTROL FifoCtrl;
		IO_PORT_MODEM_CONTROL ModemCtrl;
		if(PortBase)IO_PORT_COM[ComPort]=PortBase&0xFFF8;
		__outbyte(IO_PORT_COM[ComPort]+IO_PORT_OFFSET_INT_EN,0);		// Disable all interrupts.
		// Initialize Serial-Port Attributes. Enable DLAB.
		LineCtrl.DataBits=IO_PORT_DATA_BITS_8;
		LineCtrl.StopBits=IO_PORT_STOP_BITS_1;
		LineCtrl.Parity=IO_PORT_PARITY_NONE;
		LineCtrl.BreakEnable=FALSE;
		LineCtrl.DLAB=TRUE;
		__outbyte(IO_PORT_COM[ComPort]+IO_PORT_OFFSET_LINE_CTRL,LineCtrl.Value);
		// Set Baud-Rate
		__outbyte(IO_PORT_COM[ComPort]+IO_PORT_OFFSET_DIVISOR_LO,IO_PORT_BAUD_DIVISOR_LO_115200);
		__outbyte(IO_PORT_COM[ComPort]+IO_PORT_OFFSET_DIVISOR_HI,IO_PORT_BAUD_DIVISOR_HI_115200);
		// Disable DLAB
		LineCtrl.DLAB=FALSE;
		__outbyte(IO_PORT_COM[ComPort]+IO_PORT_OFFSET_LINE_CTRL,LineCtrl.Value);
		// Enable FIFO. Clear Recv/Trmt. Set 14-byte interrupt threshold.
		FifoCtrl.EnableFifo=TRUE;
		FifoCtrl.ClearReceive=TRUE;
		FifoCtrl.ClearTransmit=TRUE;
		FifoCtrl.DMA=FALSE;
		FifoCtrl.Reserved=0;
		FifoCtrl.Enable64ByteFifo=FALSE;
		FifoCtrl.InterruptThreshold=IO_PORT_FIFO_INT_THRESHOLD_14BYTE;
		__outbyte(IO_PORT_COM[ComPort]+IO_PORT_OFFSET_FIFO_CTRL,FifoCtrl.Value);
		// Enter Loop-Back Mode to test the serial hardware.
		ModemCtrl.ForceDTR=FALSE;
		ModemCtrl.ForceRTS=TRUE;
		ModemCtrl.AuxOutput1=TRUE;
		ModemCtrl.AuxOutput2=TRUE;
		ModemCtrl.LoopBackMode=TRUE;
		ModemCtrl.AutoFlowControl=FALSE;
		ModemCtrl.Reserved=0;
		__outbyte(IO_PORT_COM[ComPort]+IO_PORT_OFFSET_MODEM_CTRL,ModemCtrl.Value);
		// Send a dummy byte to check if the serial hardware works properly.
		__outbyte(IO_PORT_COM[ComPort]+IO_PORT_OFFSET_DATA,0xA5);
		if(__inbyte(IO_PORT_COM[ComPort]+IO_PORT_OFFSET_DATA)==0xA5)
			Print(L"COM Port %u is successfully initialized!\n",ComPort);
		else
			Print(L"COM Port %u is faulty!\n",ComPort);
		// Leave Loop-Back Mode.
		ModemCtrl.ForceDTR=TRUE;
		ModemCtrl.LoopBackMode=FALSE;
		__outbyte(IO_PORT_COM[ComPort]+IO_PORT_OFFSET_MODEM_CTRL,ModemCtrl.Value);
	}
}

void NoirSerialRead(IN UINTN ComPort,OUT UINT8 *Buffer,IN UINTN Length)
{
	for(UINTN i=0;i<Length;i++)
	{
		IO_PORT_LINE_STATUS LineStatus;
		do
		{
			LineStatus.Value=__inbyte(IO_PORT_COM[ComPort]+IO_PORT_OFFSET_LINE_ST);
		}while(!LineStatus.DataReady);
		Buffer[i]=__inbyte(IO_PORT_COM[ComPort]+IO_PORT_OFFSET_DATA);
	}
}

void NoirSerialWrite(IN UINTN ComPort,IN UINT8 *Buffer,IN UINTN Length)
{
	for(UINTN i=0;i<Length;i++)
	{
		IO_PORT_LINE_STATUS LineStatus;
		do
		{
			LineStatus.Value=__inbyte(IO_PORT_COM[ComPort]+IO_PORT_OFFSET_LINE_ST);
		}while(!LineStatus.EmptyTransmitterHolding);
		__outbyte(IO_PORT_COM[ComPort]+IO_PORT_OFFSET_DATA,Buffer[i]);
	}
}

BOOLEAN NoirSerialPoll(IN UINTN ComPort)
{
	IO_PORT_LINE_STATUS LineStatus;
	LineStatus.Value=__inbyte(IO_PORT_COM[ComPort]+IO_PORT_OFFSET_LINE_ST);
	return LineStatus.DataReady;
}