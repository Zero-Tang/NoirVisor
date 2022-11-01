# Documentation
This directory will store some documentation files important to the development of NoirVisor. \
It is recommended to read these files to join the development of NoirVisor.

## Cache List
The [cache_list.md](/doc/cache_list.md) file describes a data structure called `Cache List`. \
This is a fundamentally acyclic doubly-linked list with additional mechanism that recycles unused nodes like cache does in the processor.

## Overview of Project NoirVisor
The [hypervisor.md](/doc/hypervisor.md) file describes the overview of Project NoirVisor.

## Stealthy Hooks
The [stealthy_hooks.md](/doc/stealthy_hooks.md) file describes the general algorithm of stealthy hooks implementations.

## I/O Virtualization
The [io_hook.md](/doc/io_virt.md) file describes the general architecture of how NoirVisor takes exclusive ownership to peripheral hardware.

## Secure Virtualization
The [nsv.md](/doc/nsv.md) file describes the architecture of how NoirVisor Secure Virtualization works.