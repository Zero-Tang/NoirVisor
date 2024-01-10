# Make Script
Since the update in January 2024, NoirVisor can be built using a python script. In comparison to the batch script, python script can parallelize the compilation of NoirVisor and thereby the performance in building NoirVisor can be significantly boosted.

## Preparation
The minimal version of python required for building is 3.9 since the script is using typing syntax to help reading the codes. \
Download [Python](https://www.python.org/downloads/windows/) from Python's official website.

## Make Commands
**Synopsis**
```
make <parameters>
```

Here is a list of parameters:

| Parameter | Description |
|---|---|
| `/j` | Enable parallel compilation. |
| `/platform` | Specify the target platform. |
| `/target` | Specify what the script will compile. |
| `/v` | Enable verbose output. Useful for debugging. |

### Parallel Compilation
Specifying `/j` option will enable parallel compilation. Please note that there is no option to control the maximum number of simultaneous tasks.

### Verbose Output
To debug the make script, you may specify `/v` option to see verbose output.

### Platform
There are three platform keywords: `win7x64`, `win11x64` and `uefix64`. Currently, `uefix64` is not supported. \
Default is `win7x64`.

### Target
The available keywords depend on the platform specified.

- If platform is `win7x64` or `win11x64`, available keywords are: `hypervisor`, `disassembler`, `snprintf`.
- If platform is `uefix64`, available keywords are: `hypervisor`, `loader`, `disassembler`, `snprintf`.

Target `hypervisor` builds the NoirVisor itself.
Target `disassembler` builds the `zydis` disassembler library, a required library for decoding instructions.
Target `snprintf` builds the `c99-snprintf` string format library, a required library for debug logging.
Target `loader` builds the booting program that runs NoirVisor as Type-I hypervisor. This is only available on baremetal platforms.

Default is `hypervisor`.