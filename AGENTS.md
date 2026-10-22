# Project Agents Configuration

This file provides context, tech stack rules, and run commands for AI coding assistants. \
Do not modify this structure without updating the project guidelines.

## Project & Tech Stack Context

- **Project Name:** NoirVisor (A Bluepill-like Type-1 Hypervisor that boots before OS)
- **Primary Stack:**
	* Rust 2024 (Most of the codes in NoirVisor)
	* Assembly (Special instructions and context switches)
	* Python (Scripts that used for building and testing NoirVisor)
	* JSON (Configurations used by Python scripts for building and running NoirVisor)

## Coding Standards
- In general, codes must pass tests from `python make.py check` command. No warnings and errors are allowed.
- If TOML manifest is changed, agent must highlight the change and notify for user's attention.
- Read [the Rust guidelines](./doc/rust.md) for further details.

## Critical Terminal Commands
At the repository root, the following commands are available:
- **Clippy and Audit:** `python make.py check`
- **Build for UEFI:** `python make.py /target uefi`
- **Build for Windows:** `python make.py /target windows`
- **Run In-Code Tests:** `python make.py test`
- **Reformat Codes:** `cargo fmt` (Must be done after verified the changes)

All of the commands above must be invoked while evaluating changes to the Rust codes. \
If the environment is in Linux, you might need to invoke `python3` instead of `python`.

### Running Bochs
At the `tests` directory, thw following commands are available to run Bochs emulator:
- **Run Bochs and Emulate Intel VT-x:** `python run_bochs.py` (Only available in Windows)
- **Run Bochs and Emulate AMD-V:** `python run_bochs.py --cpu-model ryzen` (Only available in Windows, unstable to use)

### Running QEMU
At the `tests` directory, the following commands are available to run QEMU:
- **The Base Command:** `python run_qemu.py` (Available in all hosts)

You may append more arguments to run the QEMU:
- **Use KVM:** Append `-accel kvm` argument. In Windows, you may utilize WSL to run KVM: `wsl -- python3 run_qemu.py -accel kvm`.
- **Emulate IOMMU:** Append `-iommu intel` to emulate Intel VT-d. Append `-iommu amd` to emulate AMD-Vi.
- **Debug Console:** Append `-dcon-dev socket,port=22222,host=0.0.0.0,server=on,telnet=on` to let QEMU wait on socket connection. Agent must connect to `127.0.0.1:22222` through `telnet` in order to receive debug output. (e.g.: `plink 127.0.0.1 -P 22222`)
- **VGA Character Console:** Append `-nographic` to force VGA console to output data to the `stdio`. Agent must also specify the Debug Console to go through chardevs other than `stdio` to prevent collision.
- **GDB Debugger:** Append `-gdb` to enable a GDB stub. In GDB, agent may connect to the target via `tar rem 127.0.0.1:1234` command.
