# NoirVisor Debugger
This document describes the debugging facility integrated with NoirVisor.

## System Debugger
Opearting Systems come with a debugging facility. However, they come with limitations:

- Certain system components are disaled when debugging mode is enabled.
- Most things, including debug-printing, are unavailable in host context due to the highest TPL context.
- Type-I hypervisor do not come with a debugging facility.

Therefore, NoirVisor must have a special primitive for debugger.

## Remote Connections
Currently, NoirVisor supports debugging over serial connections.

## Text-Mode Debugging
For this debugging mode, the connection is considered a console. The remote host will be receiving texts only. So programs like PuTTY should do the job well in terms of a serial console.

## Interactive Debugging
The specification is unavailable yet.