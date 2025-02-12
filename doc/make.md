# Make Script
The python script is intended for parallelizing the build progress. It utilizes a simple pipelining algorithm with a dependency resolver.

## Preparation
The minimal version of python required for building is 3.9 since the script is using typing syntax to help reading the codes. \
Download [Python](https://www.python.org/downloads/windows/) from Python's official website.

You must execute the python script inside a Visual Studio prompt environment. This means you don't have to mount EWDK image if you have already installed Visual Studio.

## Synopsis
```
make [/target [target]] [/opt:yes|no]
```

### Arguments
`/target [target]` specifies the target binary to be built. \
Valid options are `windows` and `target`.

`/opt:yes|no` specifies whether optimizer is enabled.