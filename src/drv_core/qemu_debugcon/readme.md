# QEMU Debug Console Driver
NoirVisor may use QEMU's Debug Console to output messages in case the platform's debug printer (e.g.: `DbgPrint` from Windows Kernel) cannot work (e.g.: `GIF=0` for SVM).

## Run QEMU
To run QEMU with Debug Console, you must add a debug console device. \
For example, you may use `-chardev stdio,id=NoirVisor -device isa-debugcon,iobase=0x402,chardev=NoirVisor` to make the debug messages be printed onto host's `stdio`. \
Please note that the `iobase` field is up to you, if it doesn't conflict with other devices. If you use 0x402, the output messages you received, nevertheless, will be mixed with SeaBIOS's messages. You may use OVMF instead.

Considering that the most viable option to run NoirVisor in QEMU is via Linux KVM, the host of QEMU-KVM should be Linux. As such, it is recommended to make Debug Console be over socket so that receiver can be any machines connected to the network.
```
qemu-system-x86_64 -accel kvm -cpu host,svm=on,hypervisor=off -chardev socket,id=NoirVisor,port=0x402,host=0.0.0.0,server=on,telnet=on -device isa-debugcon,iobase=0x402,chardev=NoirVisor
```

Given the circumstances, a few points should be addressed:

- The `svm=on` should be `vmx=on` if the host has an Intel CPU.
- The `telnet=on` is intended for programs like `telnet` and `PuTTY` to display debug messages. Otherwise, it should be off so that data are not encoded with telnet protocol.
- If you do not specify `port=0x402` for ISA-DebugCon, QEMU will use 0xE9 as its default port number.

## Caveat
QEMU's ISA-DebugCon is a write-only device, meaning that the communication is uniplex (one-way). Thus, NoirVisor cannot enable internal interactive debugging session if QEMU's ISA-DebugCon is chosen as the debugger medium.

## Configure NoirVisor's Driver
You will have to configure the port number of QEMU's Debug Console so that innate driver in NoirVisor can choose a correct port number. \
If NoirVisor is loaded and the configured debug port is QEMU Debug Console, NoirVisor will send a message. Check the message to see if debugger connection is working.

### Windows
In registry `HKLM\SOFTWARE\Zero-Tang\NoirVisor`, write the following key values before loading NoirVisor:

| Value Name | Type | Data |
|---|---|---|
| DebugPort | REG_SZ | qemu_debugcon |
| QemuDebugConPortNumber | REG_DWORD | 0x402 |