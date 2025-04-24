# Make Script
The python script is intended for parallelizing the build progress. It utilizes a simple pipelining algorithm with a dependency resolver.

## Preparation
The minimal version of python required for building is 3.9 since the script is using typing syntax to help reading the codes. \
Download [Python](https://www.python.org/downloads/windows/) from Python's official website.

The minimal version of Rust compiler is 1.85.0 since NoirVisor uses Rust 2024 edition. \
Download [Rust](https://www.rust-lang.org/tools/install) from Rust-lang's official website. Note that you must install the `nightly` toolchain.

You must execute the python script inside a Visual Studio prompt environment. This means you don't have to mount EWDK image if you have already installed Visual Studio.

### NoirVisor Core
NoirVisor Core is written in Rust. See [documentation for NoirVisor in Rust](./rust.md).

Install [Rust](https://www.rust-lang.org/tools/install). \
The toolchain is up to you. It is recommended to use the `stable` toolchain. But feel free to use `nightly` toolchain.

Currently, NoirVisor Core in Rust can subvert the system with AMD-V in UEFI and Windows.

### Windows Driver
To build a kernel-mode driver on Windows, you should either install Visual Studio or mount Enterprise WDK. \
Presets for Free/Release build are available. Please note that the compiled binary under Free build does not come along with a digital signature. You might have to sign it yourself.

You must install `x86_64-pc-windows-msvc` target host for Rust. This should be installed by default, but if you didn't, you may install it by:
```
rustup target add x86_64-pc-windows-msvc
```

### EFI Application and Runtime Driver
To build a UEFI runtime driver, you should either install Visual Studio or mount Enterprise WDK. \
Due to different EFI firmware implementation, most modern computer firmware does not support booting an EFI Runtime Driver directly. Therefore, it is necessary to build a separate EFI Application. In this way, modern computer firmware will boot, and the application can load runtime driver into memory. \
NoirVisor also use EDK II Libraries. However, they should be pre-compiled. Visit [EDK-II-Library](https://github.com/Zero-Tang/EDK-II-Library) on GitHub in order to build them.

You must install `x86_64-unknown-uefi` target host for Rust. This is not installed by default, so you may install it by:
```
rustup target add x86_64-unknown-uefi
```

## VSCode Setup
You may setup VSCode with VS Tools Command Prompt so that you do not have to use a separate window to run the make script.

### Visual Studio 2022
If you have installed Visual Studio 2022, add the following entry in `"terminal.integrated.profiles.windows"` of `settings.json` file:
```json
"VC2022 Native x64 Prompt": {
	"path": "${env:windir}\\System32\\cmd.exe",
	"args": ["/k", "${env:ProgramFiles}\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat"]
},
```
Then you may launch `cmd` with VC2022-related environment variables.

### Enterprise WDK
If you have mounted EWDK to, for example, `V:`, add the following entry in `"terminal.integrated.profiles.windows"` of `settings.json` file:
```json
"EWDK11 Native x64 Prompt": {
	"path": "V:\\LaunchBuildEnv.cmd",
	"args": ["amd64","amd64"]
}
```

## Synopsis
```
make [/target [target]] [/opt:yes|no]
```

### Arguments
`/target [target]` specifies the target binary to be built. \
Valid options are `windows` and `uefi`. Default is `windows`.

`/opt:yes|no` specifies whether optimizer is enabled. Default is `no`.