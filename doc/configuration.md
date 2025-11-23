# Configuring NoirVisor
You may specify configurations to run NoirVisor.

## Summary of Configuration Values
| Value Name | Type | Description |
|---|---|---|
| CpuidPresence | Boolean | After subverting the system, `cpuid.eax=1.ecx[bit31]` will be set and reveals NoirVisor's presence. This value is default to true. |
| DebugPort | String | Specifies what debug medium will be used by NoirVisor. Supported values are `qemu_debugcon` and `serial`. |
| EnableIommu | Boolean | Specifies if IOMMU-based DMA protection. This feature is unstable. This value is default to false. |
| EncryptedVirtualization | Boolean | Specifies if hardware-accelerated encrypted virtualization is enabled. This feature is unsupported. This value is default to false. |
| HideFromIntelPT | Boolean | Specifies if NoirVisor will hide VMX-related events in Intel PT packets. This value is default to false. |
| NestedVirtualization | Boolean | Specifies if Nested Virtualization is enabled. This feature is unsupported. This value is default to false. |
| QemuDebugConPortNumber | Integer | Specifies the I/O port number of the ISA Debug Console. Commonly used values are 0xE9 and 0x402. |
| SegregatedVirtualization | Boolean | Specifies if NoirVisor Segregated Virtualization (NSV) is enabled for CVM. This feature is planned. This value is default to false. |
| SerialBaudRate | Integer | Specifies the baud rate of serial port. Commonly used value is 115200. |
| SerialPortBase | Integer | Specifies the I/O port base of the serial port. Commonly used values are 0x3F8 and 0x2F8. |
| SerialPortNumber | Integer | Specifies the serial port number. Commonly used values are 1 and 2. |
| StealthInlineHook | Boolean | Enables stealthy inline hook. This feature is deprecated. This value is default to false. |
| StealthMsrHook | Boolean | Enables stealthy MSR hook. This feature is deprecated. This value is default to false. |
| SubvertOnDriverLoad | Boolean | Specifies whether NoirVisor should immediately subvert the system upon driver-load. Not used in UEFI. |

## Windows Driver
Configurations of NoirVisor are specified in registry key `HKLM\SOFTWARE\Zero-Tang\NoirVisor`. Add specific values in this key in order to configure NoirVisor runtime behavior.

- Windows Registry does not support boolean types. Use `REG_DWORD` type instead. Set `1` to represent `true` and `0` to represent `false`.
- Integer types are considered as `REG_DWORD`.
- String types are considered as `REG_SZ`.

## UEFI Runtime Driver
Configurations of NoirVisor should be compiled into special binary format file `NoirVisorConfig.bin` and placed into the root directory. You may execute the `makeueficonfig.py` script to compile your config in json format into the special binary format.