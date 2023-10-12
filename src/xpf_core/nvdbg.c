/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the NoirVisor Debugger Engine.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/nvdbg.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <nv_intrin.h>
#include <stdarg.h>
#include <debug.h>

// Debug Port
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

noir_status noir_configure_qemu_debug_console(u16 port)
{
	nvdbg.medium_type=noir_debug_qemu_debugcon;
	nvdbg.debug_port.qemu_debugcon.port=port;
	return nvc_io_qemu_debugcon_init(port);
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
		case noir_debug_qemu_debugcon:
		{
			st=nvc_io_qemu_debugcon_read(buffer,length);
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
		case noir_debug_qemu_debugcon:
		{
			st=nvc_io_qemu_debugcon_write(buffer,length);
			break;
		}
	}
	return st;
}

void noir_dbgport_acquire_lock()
{
	while(noir_locked_cmpxchg(&nvdbg.port_lock,1,0))noir_pause();
}

void noir_dbgport_release_lock()
{
	noir_locked_xchg(&nvdbg.port_lock,0);
}

// Printing facilities
void cdecl nvd_vprintf(const char* src_file,const u32 ln,const char* format,va_list arg_list)
{
	char buffer[512];
	i32 prefix_len,content_len;
	if(src_file)
		prefix_len=nv_snprintf(buffer,sizeof(buffer),"[NoirVisor | (%s@%u)] ",src_file,ln);
	else
		prefix_len=nv_snprintf(buffer,sizeof(buffer),"[NoirVisor] ");
	content_len=nv_vsnprintf(&buffer[prefix_len],sizeof(buffer)-prefix_len,format,arg_list);
	noir_dbgport_acquire_lock();
	if(nvdbg.mode!=noir_debug_interactive)
		noir_dbgport_write(buffer,prefix_len+content_len);
	noir_dbgport_release_lock();
}

void cdecl nvd_printf(const char* format,...)
{
	va_list arg_list;
	char buffer[512];
	size_t len;
	va_start(arg_list,format);
	len=nv_vsnprintf(buffer,sizeof(buffer),format,arg_list);
	va_end(arg_list);
	noir_dbgport_acquire_lock();
	if(nvdbg.mode!=noir_debug_interactive)noir_dbgport_write(buffer,len);
	noir_dbgport_release_lock();
}

void cdecl nvd_panicf(const char* format,...)
{
	va_list arg_list;
	char buffer[512];
	size_t len;
	va_start(arg_list,format);
	len=nv_vsnprintf(buffer,sizeof(buffer),format,arg_list);
	va_end(arg_list);
	noir_dbgport_acquire_lock();
	if(nvdbg.mode==noir_debug_interactive)return;
	// For panicking output, write the console with red-colored characters.
	noir_dbgport_write("\x1b[31m",5);
	noir_dbgport_write(buffer,len);
	// Reset to white characters.
	noir_dbgport_write("\x1b[37m",5);
	noir_dbgport_release_lock();
}

// Dead
void nvd_deadloop()
{
	while(1)noir_pause();
}

// Exception Handlers
// Vector 0 #DE - Divide-by-Zero Error Fault
void noir_divide_error_fault_handler(noir_amd64_interrupt_frame_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("Divide-Error happened at rip=0x%p!\n",exception_frame->return_rip);
	nvd_deadloop();
}

// Vector 1 #DB - Debug Fault or Trap
void noir_debug_fault_trap_handler(noir_amd64_interrupt_frame_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("Debug Exception happened at rip=0x%p!\n",exception_frame->return_rip);
	nvd_deadloop();
}

// Vector 3 #BP - Breakpoint Trap
void noir_breakpoint_trap_handler(noir_amd64_interrupt_frame_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("Breakpoint happened at rip=0x%p!\n",exception_frame->return_rip);
	nvd_deadloop();
}

// Vector 4 #OF - Overflow Trap
void noir_overflow_trap_handler(noir_amd64_interrupt_frame_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("Overflow happened at rip=0x%p!\n",exception_frame->return_rip);
	nvd_deadloop();
}

// Vector 5 #BR - Bound-Range Fault
void noir_bound_range_fault_handler(noir_amd64_interrupt_frame_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("Bound-Range Exceed happened at rip=0x%p!\n",exception_frame->return_rip);
	nvd_deadloop();
}

// Vector 6 #UD - Invalid Opcode Fault
void noir_invalid_opcode_fault_handler(noir_amd64_interrupt_frame_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("Invalid Opcode happened at rip=0x%p!\n",exception_frame->return_rip);
	nvd_deadloop();
}

// Vector 7 #NM - Device-Not-Available Fault
void noir_device_not_available_fault_handler(noir_amd64_interrupt_frame_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("Device N/A happened at rip=0x%p!\n",exception_frame->return_rip);
	nvd_deadloop();
}

// Vector 8 #DF - Double-Fault Abort
void noir_double_fault_abort_handler(noir_amd64_interrupt_frame_with_error_code_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("Double-Fault happened at rip=0x%p!\n",exception_frame->return_rip);
	nvd_deadloop();
}

// Vector 10 #TS - Invalid TSS
void noir_invalid_tss_fault_handler(noir_amd64_interrupt_frame_with_error_code_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("Invalid TSS happened at rip=0x%p!\n",exception_frame->return_rip,exception_frame->error_code);
	nvd_deadloop();
}

// Vector 11 #NP - Segment-Not-Present Fault
void noir_segment_not_present_fault_handler(noir_amd64_interrupt_frame_with_error_code_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("Segment Absent happened at rip=0x%p! Error Code: 0x%X\n",exception_frame->return_rip,exception_frame->error_code);
	nvd_deadloop();
}

// Vector 12 #SS - Stack Fault
void noir_stack_fault_handler(noir_amd64_interrupt_frame_with_error_code_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("Stack Fault happened at rip=0x%p! Error Code: 0x%X\n",exception_frame->return_rip,exception_frame->error_code);
	nvd_deadloop();
}

// Vector 13 #GP - General-Protection Fault
void noir_general_protection_fault_handler(noir_amd64_interrupt_frame_with_error_code_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("General Protection happened at rip=0x%p! Error Code: 0x%X\n",exception_frame->return_rip,exception_frame->error_code);
	nvd_deadloop();
}

// Vector 14 #PF - Page Fault
void noir_page_fault_handler(noir_amd64_interrupt_frame_with_error_code_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("Page Fault happened at rip=0x%p! Faulting Address: 0x%p! Error Code: 0x%X!\n",exception_frame->return_rip,noir_readcr2(),exception_frame->error_code);
	nvd_deadloop();
}

// Vector 16 #MF - x87 Floating-Point Exception-Pending Fault
void noir_x87_floating_point_fault_handler(noir_amd64_interrupt_frame_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("x87 Floating-Point Pending-Exception happened at rip=0x%p!\n",exception_frame->return_rip);
	nvd_deadloop();
}

// Vector 17 #AM - Alignment-Check Fault
void noir_alignment_check_fault_handler(noir_amd64_interrupt_frame_with_error_code_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("Alignment Check happened at rip=0x%p!\n",exception_frame->return_rip);
	nvd_deadloop();
}

// Vector 18 #MC - Machine-Check Abort
void noir_machine_check_abort_handler(noir_amd64_interrupt_frame_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("Machine Check happened at rip=0x%p!\n",exception_frame->return_rip);
	nvd_deadloop();
}

// Vector 19 #XF - SIMD Floating-Point Fault
void noir_simd_floating_point_fault_handler(noir_amd64_interrupt_frame_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("SIMD Floating Point Check happened at rip=0x%p\n",exception_frame->return_rip);
	nvd_deadloop();
}

// Vector 21 #CP - Control-Protection Fault
void noir_control_protection_fault_handler(noir_amd64_interrupt_frame_with_error_code_p exception_frame,noir_gpr_state_p gpr_state)
{
	nvd_printf("Control Protection happened at rip=0x%p! Error Code: %u\n",exception_frame->return_rip,exception_frame->error_code);
	nvd_deadloop();
}