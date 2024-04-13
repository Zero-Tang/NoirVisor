/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file is the driver of QEMU Debug Console.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /drv_core/qemu_debugcon.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nv_intrin.h>
#include <nvstatus.h>
#include "qemu_debugcon.h"

noir_status nvc_io_qemu_debugcon_read(u8p buffer,size_t length)
{
	for(size_t i=0;i<length;i++)
		buffer[i]=noir_inb(qemu_debugcon_port);
	return noir_success;
}

noir_status nvc_io_qemu_debugcon_write(u8p buffer,size_t length)
{
	for(size_t i=0;i<length;i++)
		noir_outb(qemu_debugcon_port,buffer[i]);
	return noir_success;
}

noir_status nvc_io_qemu_debugcon_init(u16 port_number)
{
	u8 port_ret=noir_inb(port_number);
	bool correct_port=port_ret==qemu_debugcon_readback;
	qemu_debugcon_port=port_number;
	if(correct_port)
		nvd_printf("QEMU ISA DebugCon is initialized successfully!\n");
	else
		nv_dprintf("Port 0x%04X is not QEMU Debug-Console! Return Value: 0x%02X\n",port_number,port_ret);
	return correct_port?noir_success:noir_unsuccessful;
}