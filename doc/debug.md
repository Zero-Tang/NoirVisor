# NoirVisor Debugger
This document describes the debugging facility integrated with NoirVisor.

## System Debugger
Opearting Systems come with a debugging facility. However, they come with limitations:

- Certain system components (e.g.: PatchGuard in Windows) are disabled when debugging mode is enabled.
- Most things, including debug-printing, are unavailable in host context due to the highest TPL context.
- Type-I hypervisor scenario do not come with a debugging facility. EXDI is very powerful. However, very few hardware support EXDI.

Therefore, NoirVisor must have a special primitive for debugger.

## Remote Connections
Currently, NoirVisor supports debugging over serial connections or QEMU's ISA Debug Console.

### Serial Connection
This method is supposed to support serial connections. However, due to power management reasons, serial connection is unavailable on Windows. They will be set to D3 state, so connection is not viable. Hyper-V's and QEMU's implementation of serial emulation does not contain power management, so you can use serial connection on Hyper-V and QEMU.

### Serial Connection for Hyper-V VMs
There are two generations of VM in Hyper-V.
- For Gen 1 VMs, you may directly modify serial port settings in the settings page. 
- For Gen 2 VMs, you should use PowerShell to set the COM Ports.
	```PowerShell
	Set-VMComPort -VMName <VMName> -Number <Index> -Path <Pipe Name>
	```
	- The `VMName` is the name of your VM.
	- The `Index` is the number of your COM Port. It should be either 1 or 2.
	- The `Pipe Name` is the name of the named pipe. For named pipes on local machine, it starts with `\\.\pipe\`.

### QEMU ISA Debug Console
This method utilizes QEMU's ISA Debug Console so that NoirVisor can run inside QEMU with Linux KVM host. This method can be used in any processor context. However, only QEMU with Linux KVM is available since this is the only option to enable nested virtualization.

QEMU ISA Debug Console can only be written. Any reads from it will return 0xE9. So this connection can only be used in text-mode debugging. Interactive debugging is infeasible.

To enable QEMU ISA Debug Console, you need to add an `isa-debugcon` device and connect it to a `chardev`. For example:
```
qemu-system-x86_64 \
 -chardev socket,port=23456,host=0.0.0.0,server=on,telnet=on,id=debugger \
 -device isa-debugcon,iobase=0x402,chardev=debugger \
 ...
```
The `socket` option allows the remote debug console. If you don't need it, just use `stdio` option instead.

## Text-Mode Debugging
For this debugging mode, the connection is considered a console. The remote host will be receiving texts only. So programs like PuTTY should do the job well in terms of a serial console.

NoirVisor uses buffers with a size of 512 bytes. Therefore, any single print should not exceed this limit. Overflown parts will be discarded.

## Interactive Debugging
The specification is unavailable yet. You may use GDB or WinDbg EXDI.

## WinDbg EXDI
Use this option when you are debugging NoirVisor running inside a target which can be debugged via WinDbg eXDI (eXtensible Debugging Interface). Note that this option doesn't work great with NoirVisor as UEFI Runtime Driver since WinDbg currently can't enumerate images in UEFI. \
Install [WinDbg from Windows Store](https://apps.microsoft.com/detail/9pgjgd53tn86). Choose `File` -> `Start Debugging` -> `Attach to Kernel` -> `EXDI`. Then select the options that fit your scenario. \
Once NoirVisor is loaded, you may break the target and type `.reload /i NoirVisor.sys` in order to load the symbol of NoirVisor. \
Note that WinDbg EXDI won't be able to receive debug messages. Use the text-mode debugging techniques (e.g.: ISA-DebugCon from QEMU) instead.

### Debug NoirVisor in QEMU/KVM
To debug NoirVisor in QEMU/KVM, you have to enable GDB in QEMU. There are three ways to enable GDB in QEMU:

- Append `-s` argument before you launch QEMU. QEMU will listen on TCP port 1234.
- Append `-gdb tcp:[ip]:[port]` argument before you launch QEMU. QEMU will listen on `[ip]:[port]`.
- Use QEMU Monitor to dynamically enable GDB stub server in QEMU. If you use the SPICE protocol to operate QEMU VM from remote, append `-monitor` argument to use QEMU Monitor from remote.

After Windows is booted, you may attach WinDbg to QEMU. In WinDbg, select `QEMU` as target type, `X64` as target architecture, `Windows` as target OS, `0xFFFE - NT` as image scanning heuristic size and finally type the IP address of your QEMU host. \
Note that, in QEMU, the only supported MSR is `EFER`. Other MSRs (e.g.: `LSTAR`) are not accessible.

### Debug NoirVisor in VMware
To debug NoirVisor in VMware, you have to enable GDB in `.vmx` configuration file:

1. Disable VM encryption. Otherwise, your `.vmx` configuration file is encrypted. VM is typically not encrypted by default, but you can always go to `Edit virtual machine settings`, choose `Options` tab, pick `Access Control` item, and check if it is encrypted.
2. Add the following items in the `.vmx` file:
	```
	debugStub.listen.guest64 = "TRUE"
	debugStub.listen.guest64.remote = "TRUE"
	debugStub.hideBreakpoints = "TRUE"
	debugStub.port.guest64 = "8864"
	```
	If port `8864` is occupied for other reasons, just use other ports.
3. Power on the VM. Due to firewall policies, Windows might ask you if you will allow VMware to listen on ports. Just allow it.

After Windows is booted, you may attach WinDbg to VMware. In WinDbg, select `VMWare` as target type, `X64` as target architecture, `Windows` as target OS, `0xFFFE - NT` as image scanning heuristic size and finally type the IP address of your VMware host. \
It seems that VMware does not support accessing MSRs from GDB at all. Reading any MSRs will just return zero.

### Debug NoirVisor as UEFI Runtime Driver
To debug NoirVisor as UEFI Runtime Driver, you need to manually load NoirVisor's symbol since WinDbg cannot enumerate UEFI modules. \
NoirVisor displays its base address in the console, so you may simply use the [`.reload` command](https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/-reload--reload-module-) to manually load the symbol.

```
.reload /i NoirVisor.efi=<NoirVisor's base address>
```