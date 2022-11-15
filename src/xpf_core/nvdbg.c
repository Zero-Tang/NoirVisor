/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the NoirVisor Debugger Engine.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/nvdbg.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <stdarg.h>
#include <debug.h>

void noir_query_serial_port_base(u16p port_base)
{
	if(nvdbg.medium_type==noir_debug_serial_port)*port_base=nvdbg.debug_port.serial.port_base;
}

noir_status noir_configure_serial_port_debugger(u8 port_number,u16 port_base,u32 baudrate)
{
	nvdbg.medium_type=noir_debug_serial_port;
	nvdbg.debug_port.serial.port_number=port_number;
	nvdbg.debug_port.serial.port_base=port_base;
	return nvc_io_serial_init(port_number,port_base,baudrate);
}

noir_status noir_dbgport_read(void* buffer,size_t length)
{
	noir_status st=noir_unsuccessful;
	switch(nvdbg.medium_type)
	{
		case noir_debug_serial_port:
		{
			st=nvc_io_serial_read(nvdbg.debug_port.serial.port_number,(u8p)buffer,length);
			break;
		}
	}
	return st;
}

noir_status noir_dbgport_write(void* buffer,size_t length)
{
	noir_status st=noir_unsuccessful;
	switch(nvdbg.medium_type)
	{
		case noir_debug_serial_port:
		{
			st=nvc_io_serial_write(nvdbg.debug_port.serial.port_number,(u8p)buffer,length);
			break;
		}
	}
	return st;
}

void cdecl nvd_printf(const char* format,...)
{
	va_list arg_list;
	char buffer[512];
	size_t len;
	va_start(arg_list,format);
	len=nv_vsnprintf(buffer,sizeof(buffer),format,arg_list);
	va_end(arg_list);
	if(nvdbg.mode!=noir_debug_interactive)noir_dbgport_write(buffer,len);
}

void cdecl nvd_panicf(const char* format,...)
{
	va_list arg_list;
	char buffer[512];
	size_t len;
	va_start(arg_list,format);
	len=nv_vsnprintf(buffer,sizeof(buffer),format,arg_list);
	va_end(arg_list);
	if(nvdbg.mode==noir_debug_interactive)return;
	// For panicking output, write the console with red-colored characters.
	noir_dbgport_write("\x1b[31m",5);
	noir_dbgport_write(buffer,len);
	// Reset to white characters.
	noir_dbgport_write("\x1b[37m",5);
}