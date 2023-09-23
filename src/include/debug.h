/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the NoirVisor Debugger Engine.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/debug.h
*/

#include <nvdef.h>

typedef enum _noir_debug_media_type
{
	noir_debug_unknown_medium,	// Unknown medium means debugger is deactivated.
	noir_debug_serial_port,
	noir_debug_qemu_debugcon
}noir_debug_media_type,*noir_debug_media_type_p;

typedef enum _noir_debug_mode
{
	noir_debug_text_output,
	noir_debug_interactive
}noir_debug_mode,*noir_debug_mode_p;

typedef struct _noir_debugger
{
	noir_debug_media_type medium_type;
	noir_debug_mode mode;
	union
	{
		struct
		{
			u8 port_number;
			u8 pad;
			u16 port_base;
		}serial;
		struct
		{
			u16 port;
		}qemu_debugcon;
	}debug_port;
	u32v port_lock;
}noir_debugger,*noir_debugger_p;

// Serial Driver
noir_status nvc_io_serial_init(u8 port_number,u16 port_base,u32 baudrate);
noir_status nvc_io_serial_read(u8 port_number,u8p buffer,size_t length);
noir_status nvc_io_serial_write(u8 port_number,u8p buffer,size_t length);
// QEMU Debug Console Driver
noir_status nvc_io_qemu_debugcon_init(u16 port_number);
noir_status nvc_io_qemu_debugcon_read(u8p buffer,size_t length);
noir_status nvc_io_qemu_debugcon_write(u8p buffer,size_t length);

#if defined(_nvdbg)
// String Facility
i32 cdecl nv_snprintf(char* buffer,size_t limit,const char* format,...);
i32 cdecl nv_vsnprintf(char* buffer,size_t limit,const char* format,va_list arg_list);

noir_hvdata noir_debugger nvdbg={0};
#else
extern noir_debugger nvdbg;
#endif