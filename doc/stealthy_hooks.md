# Stealthy Hooks
The stealthy hooks are deprecated features of NoirVisor. Further updates regarding stealthy hooks will most likely be driven by community instead of Zero Tang himself.

## Stealthy System Call Hooks
The stealthy system call hooks are implemented by overwriting the `LSTAR` MSR with interceptions upon any reads to this MSR. \
When a software tries to read the `LSTAR` MSR, it will be intercepted by NoirVisor. \
The KVA-shadow in Windows is a mitigation to the [Meltdown Attack](https://meltdownattack.com/). This mitigation has an innate side-effect that interferes with system-call hooking. You may read [this blog](https://tangptr.com/?p=149) in order to see how to mitigate such side-effect.

## Stealthy Inline Hooks
Inline hooks are implemented by writing instructions that jump to the proxy functions. In order to make them stealthy, memory virtualization is involved.

### Hide Inline Hooks via Intel VT-x/EPT
The Extended Page Table (EPT) mechanism supports splitting the reading and executing accesses completely. Hypervisor can grant either no-execution or execute-only permissions to guest pages. \
With such feature, NoirVisor may grant execute-only permission to hooked pages and no-execution permission to original pages. NoirVisor implements such stealthy inline hook in a lazy-scheduling approach and do not use hypervisor-assisted control-flow redirection. Therefore, theoretically, there can be no performance cost if no one tries to detect the hook. \
If hooked pages are mapped, EPT violations could occur when reading or writing the the hooked pages. On EPT violation, NoirVisor would map to original pages and grant no-execution permissions. \
If original pages are mapped, EPT violations could occur when executing the hooked pages. On EPT violation, NoirVisor would map to hooked pages and grant execute-only permissions. \
There is a special situation: an instruction in the same page to the data being accessed. In this case, the EPT violations might fall into a dead loop. The solution is to temporarily grant all permissions and enable the Monitor Trap Flag (MTF) feature. The MTF feature is a single-stepping feature that triggers VM-Exit when the next guest instruction completes. In the VM-Exit induced by MTF, revoke read/write permissions and map to hooked pages.

### Hide Inline Hooks via AMD-V/NPT
The Nested Page Table (NPT) mechanism does not support splitting the reading and executing accesses. Therefore, hiding Inline Hooks can be much more arduous than Intel VT-x/EPT. \
NoirVisor provides two sets of NPT paging structures: the primary paging structure grants all accesses to most guest physical pages but revoke execution permissions for hooked pages; the secondary pagging structure grants all accesses to hooked pages but revoke execution permissions for other pages. \
As you see, there is no guarantee that hooking via NPT can hide instructions from reading data on the same pages. Albeit you may emulate the MTF via setting the `RFlags.TF` bit and intercepting the `#DB` exceptions, after all, to what extent should you start and stop single-stepping? Plus, such single-stepping is vulnerable to interrupts. \
It is extremely recommended to emulate the hooked function (i.e.: do not detour back to the hooked function) for the best performance. By doing so, #NPF VM-Exits for hiding hooks can be significantly reduced. \
Regardless, it is significantly unrecommended to implement stealthy inline hooks via AMD-V/NPT.

### Compatibility with Control-Flow Enforcement
For 64-bit codes, older versions of NoirVisor use the following instruction sequence to perform control-flow redirection:

```Assembly
push rax
mov rax,proxy
xchg rax,[rsp]
ret
```

This approach offers significant performance boost by virtue of no VM-Exits would occur just in order redirect the control-flow. \
The problem is that, if the Control-Flow Enforcement (CET) is enabled, the `ret` instruction could trigger `#CP(1)` exception due to the returning address being different from what was stored in the shadow stack. \
In order to be compatible with CET, the `ret` instruction must be circumvented. We will emulate the behavior of `ret` instruction by adding the `rsp` and jumping by pointer. The drawback is the even longer shellcode.

```Assembly
push rax                    ; 50
mov rax,proxy               ; 48 B8 XX XX XX XX XX XX XX XX
xchg rax,qword ptr [rsp]    ; 48 87 04 24
add rsp,8                   ; 48 83 C4 F8
jmp qword ptr[rsp-8]        ; FF 64 24 F8
```

This shellcode for jumping has 23 bytes in total, 7 bytes longer than the shellcode which does not consider the CET.

## Install Your Own Stealthy Inline Hooks
You may add install your own stealthy Inline Hooks for your own specific purposes. \
Keep in mind that NoirVisor's approach to install hooks requires overwriting a sequence of bytes. Overlapped hooks can cause undefined behavior.

### Windows
The `/src/xpf_core/windows/hooks.c` file is subject to be revised if you wish to add your own hooks. Locate the `NoirBuildHookedPages` function. This is the function you should revise. <br>
Generally, installing a stealthy inline hook requires the following procedure:

1. Locate the image. You can skip this step if you can locate the function without locating the image.
2. Locate the function address.
3. Invoke the `NoirConstructHook` function to construct hook pages. Note that NoirVisor uses a sequence of instructions to perform control-flow redirection, so the hook code can span to two pages. In addition, the constant `HookPageLimit` does not imply the maximum amount of hooks can be installed. Therefore, it is possible you can only install less number of hooks than the limit of hooked pages, yet it is also possible to install more number of hooks than the limit of hooked pages if the hooks are gathered in the same pages.

Note:

1. NoirVisor provides some functions to help locating the kernel-mode modules and exported procedures. Use them wisely to reduce your effort.
2. Unexported procedures, or private procedures, must be located by your own effort.

When you uninstall a hook, only one additional step is required: release the detour function pointer you received from the `NoirConstructHook` function. Do this in `NoirTeardownHookPages` function.