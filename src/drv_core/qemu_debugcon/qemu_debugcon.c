/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

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

noir_status nvc_io_qemu_debugcon_init(u16 port_number)
{
	qemu_debugcon_port=port_number;
	return noir_success;
}

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