/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2023, Zero Tang. All rights reserved.

  This file is the Serial-Port driver core for NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /drv_core/serial/serial.h
*/

#include <nvdef.h>

#define noir_serial_port_io_offset_comm					0	// DLAB negedge
#define noir_serial_port_io_offset_ier					1	// DLAB negedge
#define noir_serial_port_io_offset_baudrate_low			0	// DLAB posedge
#define noir_serial_port_io_offset_baudrate_high		1	// DLAB posedge
#define noir_serial_port_io_offset_iid					2	// Read-Only
#define noir_serial_port_io_offset_fifo					2	// Write-Only
#define noir_serial_port_io_offset_lcr					3
#define noir_serial_port_io_offset_mcr					4
#define noir_serial_port_io_offset_lsr					5
#define noir_serial_port_io_offset_msr					6
#define noir_serial_port_io_offset_scratch				7

typedef union _noir_io_serial_interrupt_enable
{
	struct
	{
		u8 recv_avail:1;
		u8 xmit_empty:1;
		u8 line_status:1;
		u8 modem_status:1;
		u8 sleep_mode:1;
		u8 low_power:1;
		u8 reserved:2;
	};
	u8 value;
}noir_io_serial_interrupt_enable,*noir_io_serial_interrupt_enable_p;

// For following baud-rates, clock selector should be zero.
#define noir_serial_port_io_baud_divisor_115200	1
#define noir_serial_port_io_baud_divisor_57600	2
#define noir_serial_port_io_baud_divisor_38400	3
#define noir_serial_port_io_baud_divisor_19200	6
#define noir_serial_port_io_baud_divisor_9600	12
#define noir_serial_port_io_baud_divisor_7200	16
#define noir_serial_port_io_baud_divisor_4800	24
#define noir_serial_port_io_baud_divisor_3600	32
#define noir_serial_port_io_baud_divisor_2400	48
#define noir_serial_port_io_baud_divisor_1800	64
#define noir_serial_port_io_baud_divisor_1200	96
#define noir_serial_port_io_baud_divisor_600	192
#define noir_serial_port_io_baud_divisor_300	384
#define noir_serial_port_io_baud_divisor_150	768
#define noir_serial_port_io_baud_divisor_75		1536
#define noir_serial_port_io_baud_divisor_50		2304

// For following baud-rates, clock selector should be one.
#define noir_serial_port_io_baud_divisor_3000k	1
#define noir_serial_port_io_baud_divisor_1500k	2
#define noir_serial_port_io_baud_divisor_750k	4
#define noir_serial_port_io_baud_divisor_500k	6
#define noir_serial_port_io_baud_divisor_375k	8
#define noir_serial_port_io_baud_divisor_300k	10
#define noir_serial_port_io_baud_divisor_250k	12
#define noir_serial_port_io_baud_divisor_150k	20
#define noir_serial_port_io_baud_divisor_125k	24

typedef union _noir_io_serial_baud_rate
{
	struct
	{
		u16 baud_divisor:15;
		u16 clk_sel:1;
	};
	struct
	{
		u8 lo;
		u8 hi;
	};
	u16 value;
}noir_io_serial_baud_rate,*noir_io_serial_baud_rate_p;

#define noir_serial_port_io_trigger_level_1_byte	0
#define noir_serial_port_io_trigger_level_4_bytes	1
#define noir_serial_port_io_trigger_level_8_bytes	2
#define noir_serial_port_io_trigger_level_14_bytes	3

typedef union _noir_io_serial_fifo_control
{
	struct
	{
		u8 exrf:1;
		u8 clear_recv:1;
		u8 clear_xmit:1;
		u8 dma:1;
		u8 reserved:2;
		u8 trigger_level:2;
	};
	u8 value;
}noir_io_serial_fifo_control,*noir_io_serial_fifo_control_p;

typedef union _noir_io_serial_interrupt_id
{
	struct
	{
		u8 int_pending:1;
		u8 int_id:3;
		u8 reserved:2;
		u8 fifo_enable:2;
	};
	u8 value;
}noir_io_serial_interrupt_id,*noir_io_serial_interrupt_id_p;

#define noir_serial_port_io_data_bits(x)	(x-5)

#define noir_serial_port_io_stop_bits(x)	(x-1)

#define noir_serial_port_io_parity_none		0
#define noir_serial_port_io_parity_odd		1
#define noir_serial_port_io_parity_even		3
#define noir_serial_port_io_parity_mark		5
#define noir_serial_port_io_parity_space	7

typedef union _noir_io_serial_line_control
{
	struct
	{
		u8 data:2;
		u8 stop:1;
		u8 parity:3;
		u8 break_ctrl:1;
		u8 dlab:1;
	};
	u8 value;
}noir_io_serial_line_control,*noir_io_serial_line_control_p;

typedef union _noir_io_serial_modem_control
{
	struct
	{
		u8 dtr:1;
		u8 rts:1;
		u8 out1:1;
		u8 out2:1;
		u8 loopback:1;
		u8 autoflow:1;
		u8 reserved:2;
	};
	u8 value;
}noir_io_serial_modem_control,*noir_io_serial_modem_control_p;

typedef union _noir_io_serial_line_status
{
	struct
	{
		u8 data_ready:1;
		u8 overrun_error:1;
		u8 parity_error:1;
		u8 frame_error:1;
		u8 break_interrupt:1;
		u8 transmit_empty:1;
		u8 transmit_error:1;
		u8 fifo_error:1;
	};
	u8 value;
}noir_io_serial_line_status,*noir_io_serial_line_status_p;

typedef union _noir_io_serial_modem_status
{
	struct
	{
		u8 delta_cts:1;
		u8 delta_dsr:1;
		u8 trailing_ri:1;
		u8 delta_dcd:1;
		u8 cts:1;
		u8 dsr:1;
		u8 ri:1;
		u8 dcd:1;
	};
	u8 value;
}noir_io_serial_modem_status,*noir_io_serial_modem_status_p;

u16 noir_serial_io_ports[8]=
{
	0x3F8,
	0x2F8,
	0x3E8,
	0x2E8,
	0x5F8,
	0x4F8,
	0x5E8,
	0x4E8
};