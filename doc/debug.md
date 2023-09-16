# NoirVisor Debugger
This document describes the debugging facility integrated with NoirVisor.

## System Debugger
Opearting Systems come with a debugging facility. However, they come with limitations:

- Certain system components are disaled when debugging mode is enabled.
- Most things, including debug-printing, are unavailable in host context due to the highest TPL context.
- Type-I hypervisor do not come with a debugging facility. EXDI is very powerful. However, very few models support EXDI.

Therefore, NoirVisor must have a special primitive for debugger.

## Remote Connections
Currently, NoirVisor supports debugging over serial connections or QEMU's ISA Debug Console.

### Serial Connection
This method is supposed to support serial connections. However, due to unknown reasons, serial connection is not available on Windows. Serial connection is lost once OS is loaded. Direct I/O will be refused by the serial device.

### QEMU ISA Debug Console
This method utilizes QEMU's ISA Debug Console so that NoirVisor can run inside QEMU with Linux KVM host. This method can be used in any processor context. However, only QEMU with Linux KVM is available since this is the only option to enable nested virtualization.

## Text-Mode Debugging
For this debugging mode, the connection is considered a console. The remote host will be receiving texts only. So programs like PuTTY should do the job well in terms of a serial console.

## Interactive Debugging
The specification is unavailable yet.