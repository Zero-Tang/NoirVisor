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
This method is supposed to support serial connections. However, due to unknown reasons, serial connection is not available on Windows in VMware machines. Serial connection is lost once OS is loaded. Direct I/O will be refused by the serial device. Although you can't use serial connection in VMware, you may instead use it in Hyper-V.

### Serial Connection for Hyper-V VMs
There are two generations of VM in Hyper-V.
- For Gen 1 VMs, you may directly modify serial port settings in the settings page. 
- For Gen 2 VMs, you should use PowerShell to set the COM Ports.
	```PowerShell
	Set-VMComPort -VM <VMName> -Number <Index> -Path <Pipe Name>
	```
	- The `VMName` is the name of your VM.
	- The `Index` is the number of your COM Port. It should be either 1 or 2.
	- The `Pipe Name` is the name of the named pipe.

### QEMU ISA Debug Console
This method utilizes QEMU's ISA Debug Console so that NoirVisor can run inside QEMU with Linux KVM host. This method can be used in any processor context. However, only QEMU with Linux KVM is available since this is the only option to enable nested virtualization.

QEMU ISA Debug Console can only be written. Any reads from it will return 0xE9. So this connection can only be used in text-mode debugging. Interactive debugging is infeasible.

## Text-Mode Debugging
For this debugging mode, the connection is considered a console. The remote host will be receiving texts only. So programs like PuTTY should do the job well in terms of a serial console.

NoirVisor uses buffers with a size of 512 bytes. Therefore, any single print should not exceed this limit. Overflown parts will be discarded.

## Interactive Debugging
The specification is unavailable yet. In future, the `nvdebug` tool might become the interactive debugger.

## Tool: nvdebug
This is a simple tool that parses the UEFI environment and the symbol. Currently, `nvdebug` can parse QEMU guest through GDB connection. \
If you specified `-s` when you launch QEMU, the port number is 1234.

### Obtain the Symbol Name of the Faulting Instruction Pointer
When NoirVisor panicked due to an exception intercepted by IDT, you may use this feature to obtain the symbol name of the faulting rip. \
In `tools/nvdebug` directory, execute:
```
cargo run qemu://[kvmhost]:[port] [fault-rip]
```

### Obtain the Stack Trace
When NoirVisor panicked in runtime, you may use this feature to obtain a stack trace. \
In `tools/nvdebug` directory, execute:
```
cargo run qemu:://[kvmhost]:[port]
```
You should be able to see a stack trace.

## WinDbg EXDI
Use this option when you are debugging NoirVisor as a Windows Driver running inside a target which can be debugged via WinDbg eXDI (eXtensible Debugging Interface). Note that this option doesn't work great with NoirVisor as UEFI Runtime Driver since WinDbg currently can't enumerate images in UEFI. \
Install [WinDbg from Windows Store](https://apps.microsoft.com/detail/9pgjgd53tn86). Choose `File` -> `Start Debugging` -> `Attach to Kernel` -> `EXDI`. Then select the options that fit your scenario. \
Once NoirVisor is loaded, you may break the target and type `.reload /i NoirVisor.sys` in order to load the symbol of NoirVisor. \
Note that WinDbg EXDI won't be able to receive debug messages. Use the text-mode debugging techniques (e.g.: ISA-DebugCon from QEMU) instead.

### Debug NoirVisor in QEMU/KVM
To debug NoirVisor in QEMU/KVM, you have to enable GDB in QEMU:

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