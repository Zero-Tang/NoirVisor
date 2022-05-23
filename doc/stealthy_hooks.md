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

### Weak Point of NoirVisor's Stealthy Inline Hooks
For 64-bit codes, NoirVisor uses the following instruction sequence to perform control-flow redirection:

```Assembly
push rax
mov rax,proxy
xchg rax,[rsp]
ret
```

This approach offers significant performance by virtue of using minimal amount of instructions for redirecting the control-flow. \
The problem is that, if the Control-Flow Enforcement (CET) is enabled, the `ret` instruction could trigger `#CP(1)` exception due to the returning address being different from what was stored in the shadow stack. \
Future implementation of NoirVisor might intercept `#CP` exceptions in order to be compatible with `CET`, or use the `int 3` hypervisor-assisted control-flow redirection approach when CET is detected. Performance cost should be considered.