/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the Serial-Port driver core for NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /drv_core/serial/serial.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <nv_intrin.h>
#include "serial.h"

noir_status nvc_io_serial_init(u8 port_number,u16 port_base,u32 baudrate)
{
	noir_status st=noir_invalid_parameter;
	if(port_number>=8)
		nv_dprintf("[Serial I/O Init] Port number is too big!\n");
	else
	{
		noir_io_serial_interrupt_enable int_en;
		noir_io_serial_baud_rate baud_rate;
		noir_io_serial_fifo_control fifo_ctrl;
		noir_io_serial_line_control line_ctrl;
		noir_io_serial_modem_control modem_ctrl;
		if(port_base)noir_serial_io_ports[port_number]=port_base&0xFFF8;
		nv_dprintf("Initializing COM%u (Base=0x%04X)...\n",port_number+1,noir_serial_io_ports[port_number]);
		// NoirVisor disables interrupts from serial I/O.
		int_en.value=noir_inb(noir_serial_io_ports[port_number]+noir_serial_port_io_offset_ier);
		nv_dprintf("Previous interrupt setting of COM%u: 0x%02X\n",port_number+1,int_en.value);
		noir_outb(noir_serial_io_ports[port_number]+noir_serial_port_io_offset_ier,0);
		// Initialize Serial Port Attributes.
		line_ctrl.value=0;
		line_ctrl.dlab=true;	// We need to toggle Baud-Rate.
		noir_outb(noir_serial_io_ports[port_number]+noir_serial_port_io_offset_lcr,line_ctrl.value);
		// Set Serial Port Baud Rate
		if(baudrate>115200)
		{
			baud_rate.baud_divisor=(u16)(30000000/baudrate);
			baud_rate.clk_sel=1;
		}
		else
		{
			baud_rate.baud_divisor=(u16)(115200/baudrate);
			baud_rate.clk_sel=0;
		}
		nv_dprintf("BaudRate: Lo=%02X, Hi=%02X\n",baud_rate.lo,baud_rate.hi);
		noir_outb(noir_serial_io_ports[port_number]+noir_serial_port_io_offset_baudrate_low,baud_rate.lo);
		noir_outb(noir_serial_io_ports[port_number]+noir_serial_port_io_offset_baudrate_high,baud_rate.hi);
		// Disable DLAB
		line_ctrl.data=noir_serial_port_io_data_bits(8);
		line_ctrl.stop=noir_serial_port_io_stop_bits(1);
		line_ctrl.parity=noir_serial_port_io_parity_none;
		line_ctrl.break_ctrl=false;
		line_ctrl.dlab=false;
		nv_dprintf("Line Control: %02X\n",line_ctrl.value);
		noir_outb(noir_serial_io_ports[port_number]+noir_serial_port_io_offset_lcr,line_ctrl.value);
		// Enable FIFO. Clear Recv/Xmit. Use 14-byte threshold.
		fifo_ctrl.exrf=true;
		fifo_ctrl.clear_recv=true;
		fifo_ctrl.clear_xmit=true;
		fifo_ctrl.dma=false;
		fifo_ctrl.reserved=0;
		fifo_ctrl.trigger_level=noir_serial_port_io_trigger_level_14_bytes;
		nv_dprintf("FIFO Control: %02X\n",fifo_ctrl.value);
		noir_outb(noir_serial_io_ports[port_number]+noir_serial_port_io_offset_fifo,fifo_ctrl.value);
		// Enter loop-back mode to test serial hardware.
		modem_ctrl.dtr=false;
		modem_ctrl.rts=true;
		modem_ctrl.out1=true;
		modem_ctrl.out2=true;
		modem_ctrl.loopback=true;
		modem_ctrl.autoflow=false;
		modem_ctrl.reserved=0;
		nv_dprintf("Modem Control (Before LoopBack): %02X\n",modem_ctrl.value);
		noir_outb(noir_serial_io_ports[port_number]+noir_serial_port_io_offset_mcr,modem_ctrl.value);
		// Send a dummy byte to check if the serial hardware works properly.
		noir_outb(noir_serial_io_ports[port_number]+noir_serial_port_io_offset_comm,0xa5);
		if(noir_inb(noir_serial_io_ports[port_number]+noir_serial_port_io_offset_comm)==0xa5)
		{
			nv_dprintf("COM%u is initialized successfully!\n",port_number+1);
			st=noir_success;
		}
		else
		{
			nv_dprintf("COM%u hardware is faulty!\n",port_number+1);
			st=noir_unsuccessful;
		}
		// Leave loop-back mode.
		modem_ctrl.dtr=true;
		modem_ctrl.loopback=false;
		nv_dprintf("Modem Control (After LoopBack): %02X\n",modem_ctrl.value);
		noir_outb(noir_serial_io_ports[port_number]+noir_serial_port_io_offset_mcr,modem_ctrl.value);
	}
	return st;
}

noir_status nvc_io_serial_read(u8 port_number,u8p buffer,size_t length)
{
	noir_status st=noir_success;
	for(size_t i=0;i<length;i++)
	{
		noir_io_serial_line_status line_st;
		while(1)
		{
			line_st.value=noir_inb(noir_serial_io_ports[port_number]+noir_serial_port_io_offset_lsr);
			if(line_st.data_ready)
				break;
			else if(line_st.overrun_error || line_st.parity_error || line_st.frame_error)
			{
				st=noir_hardware_error;
				break;
			}
		}
		if(st!=noir_success)break;
		buffer[i]=noir_inb(noir_serial_io_ports[port_number]+noir_serial_port_io_offset_comm);
	}
	return st;
}

noir_status nvc_io_serial_write(u8 port_number,u8p buffer,size_t length)
{
	noir_status st=noir_success;
	for(size_t i=0;i<length;i++)
	{
		noir_io_serial_line_status line_st;
		while(1)
		{
			line_st.value=noir_inb(noir_serial_io_ports[port_number]+noir_serial_port_io_offset_lsr);
			if(line_st.transmit_empty)
				break;
			else if(line_st.transmit_error || line_st.fifo_error)
			{
				st=noir_hardware_error;
				break;
			}
		}
		if(st!=noir_success)break;
		noir_outb(noir_serial_io_ports[port_number]+noir_serial_port_io_offset_comm,buffer[i]);
	}
	return st;
}