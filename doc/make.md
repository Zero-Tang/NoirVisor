# Make Script
Since the update in June 2023, NoirVisor can be built using a python script. In comparison to the batch script, python script can parallelize the compilation of NoirVisor and thereby the performance in building NoirVisor can be significantly boosted.

## Preparation
The minimal version of python required for building is 3.9 since the script is using typing syntax to help reading the codes. \
Download [Python](https://www.python.org/downloads/windows/) from Python's official website.

## Make Commands
**Synopsis**
```
make <command> <parameters>
```

Here is a list of commands:
| Command | Description |
|---|---|
| `build` | Build the project. (Default) |
| `clean` | Clean the build directories. |
| `shell` | Enter the build shell. |

If no command is specified, the `build` command is used.

### Build Command
The `build` command plays the primary role of building the NoirVisor project. \
This command takes the following parameters:

- `/platform` Can be `win7` (default), `win11` and `uefi`. (`uefi` option is unsupported yet)
- `/optimize` Can be `checked` (default), `debug` (Optimization is disabled) or `free`, `release` (Optimization is enabled)
- `/target` Can be `hvm` (Build the NoirVisor, default) or `disasm` (Build the Disassembler)

### Clean Command
The `clean` command will clean all files generated, including compiled binaries, by the make system.
No argument is taken by this command.

### Shell Command
The `shell` command will enter the shell of NoirVisor make system. \
No argument is taken by this command.