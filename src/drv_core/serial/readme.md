# Serial Port Driver
NoirVisor may need to access serial ports in order to debug.

# Serial Driver Specification
Non-ISA-standard serial ports are out of scope.

## Port Addresses
The port addresses may vary. Typically, the address may be:

| COM Port | Port Base |
|---|---|
| COM1 | 0x3F8 |
| COM2 | 0x2F8 |
| COM3 | 0x3E8 |
| COM4 | 0x2E8 |
| COM5 | 0x5F8 |
| COM6 | 0x4F8 |
| COM7 | 0x5E8 |
| COM8 | 0x4E8 |

The COM1 and COM2 may be reliable to hard code somehow. But the rest aren't really reliable.

The most reliable way to locate the port addresses are searching devices in ACPI namespace `ACPI\PNP0501`.

## I/O Port
Once you have the port base, you may operate the serial port.

| I/O Port Offset | DLAB Value | Access | Register |
|---|---|---|---|
| +0 | 0 | R	| Byte to receive |
| +0 | 0 | W	| Byte to transmit |
| +0 | 1 | R/W	| The least-significant byte of the divisor for baud rate |
| +1 | 0 | R/W	| Interrupt-Enable Register |
| +1 | 1 | R/W	| The most-significant byte of the divisor for baud rate |
| +2 | X | R	| Interrupt Identification Register |
| +2 | X | W	| FIFO Control Registers |
| +3 | X | R/W	| Line Control Register |
| +4 | X | R/W	| Modem Control Register |
| +5 | X | R/W	| Line Status Register |
| +6 | X | R/W	| Modem Status Register |
| +7 | X | R/W	| Scratch Register |

For example, the following assembly code is receiving a byte ready at serial port COM1:

```Assembly
in al,0x3F8
```

Once the `in` instruction completes, the `al` register contains the byte received from the serial port.

## Interrupt Enable Register
The Interrupt Enable Register configures whether the serial hardware will throw IRQs on certain events.

| Bits | Description |
|---|---|
| 7:6	| Reserved. |
| 5		| Interrupt when the serial port is enterring low-power mode. |
| 4		| Interrupt when the serial port is enterring sleep mode. |
| 3		| Interrupt when the modem status of the serial port has changed. |
| 2		| Interrupt when the line status of the serial port has changed. |
| 1		| Interrupt when the serial port is ready to transmit a new byte. |
| 0		| Interrupt when the serial port has a byte to receive. |

NoirVisor's implementation will disable all interrupts for the sake of atomic operations.

## Baud Rate
The serial port controller has an internal clock. UART has two types of clock: 1.8432MHz and 24 MHz baud clocks. \
The baud rate must be set before using it. Typically, most systems should support a baud rate at 115200 hertz. In order to set baud rate, the DLAB bit (See Line Control Register) must be set.

The divisor registers for baud rate are defined as the followings:
<table>
	<thead>
		<th>I/O Port Offset</th>
		<th>Bit Field</th>
		<th>Definition</th>
	</thead>
	<tbody>
		<tr>
			<td>+0</td>
			<td>7:0</td>
			<td>Bits 0-7 of the baud divisor</td>
		</tr>
		<tr>
			<td rowspan=2>+1</td>
			<td>7</td>
			<td>Use 24MHz UART Clock</td>
		</tr>
		<tr>
			<td>6:0</td>
			<td>Bits 8-14 of the baud divisor</td>
		</tr>
	</tbody>
</table>

NoirVisor's implementation will use 115200 baudrate by default.

## Line Control Register
The Line Control Register is defined as following:

| Bits | Description |
|---|---|
| 7		| Divisor Latch Access Bit (DLAB). You should set it in order to set the baud divisor register. |
| 6		| Break Control Bit. |
| 5:3	| Parity. |
| 2		| Stop Bit. |
| 1:0	| Data Bits. |

### Data Bits
The number of bits per character. Usually, this is set to 8-bit, but you can set to 7-bit if you only communicate with ASCII text. The fewer bits per character, the faster communication.

| Value | Bits per Character |
|---|---|
| 0 | 5 |
| 1 | 6 |
| 2 | 7 |
| 3 | 8 |

### Stop Bits
Stop bits are used by the serial controller to verify whether each character are correctly sent. 

| Value | Number of Stop Bits |
|---|---|
| 0 | 1 |
| 1 | 1.5 if each character has 5 bits, 2 elsewise |

### Parity

| Value | Definition |
|---|---|
| 0 or 2 or 4 or 6 | None |
| 1 | Odd |
| 3 | Even |
| 5 | Mark |
| 7 | Space |

## Line Status Register
The Line Status Register is defined as following:

| Bits | Description |
|---|---|
| 7	| There is an error on FIFO transmission. |
| 6	| Transmission failed. Nothing was sent. |
| 5	| There is no pending bytes in transmitter. |
| 4	| There is a break in the date input. |
| 3	| A stop bit is missing. |
| 2	| The parity is incorrect. |
| 1	| Data lost in receiving. |
| 0	| Data is ready to receive. |

## Modem Control Register
The Modem Control Register is defined as following:

| Bits | Description |
|---|---|
| 7:5	| Reserved. |
| 4		| Loopback Mode. Typically used for testing hardware functionality. |
| 3		| Controls hardware control pin `OUT2`. This is used for enabling IRQ. |
| 2		| Controls hardware control pin `OUT1`. This is unused in ISA PC. |
| 1		| Controls the Request-to-Send `RTS` pin. |
| 0		| Controls the Data-to-Receive `DTR` pin. |

## Modem Status Register
The Modem Status Register is defined as following:

| Bits | Description |
|---|---|
| 7 	| Inverted DCD Signal |
| 6 	| Inverted RI Signal |
| 5 	| Inverted DSR Signal |
| 4 	| Inverted CTS Signal |
| 3 	| Indicates that DCD input has changed state since the last time it ware read |
| 2 	| Indicates that RI input to the chip has changed from a low to a high state |
| 1 	| Indicates that DSR input has changed state since the last time it was read |
| 0 	| Indicates that CTS input has changed state since the last time it was read |

## Scratch Register
The scracth register can store a byte for whatever purpose.