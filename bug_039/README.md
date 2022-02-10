# HP cannot boot Windows 10 x64 (WRMSR)

## Scope
* HP, x64 Windows 10, x64 XMHF
* good: HP, x86 Windows 10, x64 XMHF
* good: QEMU, Windows 10
* good: QEMU, Fedora
* bad, probably related: ThinkPad, Fedora 35

## Behavior

When booting x64 Windows 10, x64 XMHF on HP, see exception in hypervisor. Use
`read_stack.py` to annotate:
```
[00]: unhandled exception 14 (0xe), halting!
[00]: error code: 0x0000000000000000
[00]: state dump follows...
[00] CS:RIP 0x0010:0x0000000010204a30 with RFLAGS=0x0000000000010083
...
[00]-----stack dump-----
[00]  Stack(0x000000001d985ea8) -> 0x0000000010204a30 CS	<_vmx_handle_intercept_wrmsr+784>
[00]  Stack(0x000000001d985eb0) -> 0x0000000000000010 RFLAGS	???
[00]  Stack(0x000000001d985eb8) -> 0x0000000000010083 RSP	???
[00]  Stack(0x000000001d985ec0) -> 0x000000001d985ed8 SS	<g_cpustacks+65240>
...
[00]  Stack(0x000000001d985f28) -> 0x000000001d985f68 RBP #0	<g_cpustacks+65384>
[00]  Stack(0x000000001d985f30) -> 0x0000000010205859 RIP #1	<xmhf_parteventhub_arch_x86_64vmx_intercept_handler+1278>
...
[00]  Stack(0x000000001d985f68) -> 0x0000000000000000 RBP #1	???
[00]  Stack(0x000000001d985f70) -> 0x0000000010203f2e RIP #2	<xmhf_parteventhub_arch_x86_64vmx_entry+40>
...
```

## Debugging

### Fedora

After installing x64 Fedora 35 on HP, can reproduce the same issue.
Git around `ae6d89527`, serial `20220208231648`. We think Windows 10 and Fedora
35 should have similar issue. Currently this issue is reproduced on:
* HP, x64 Windows 10, x64 XMHF
* HP, x64 Fedora 35, x64 XMHF
* ThinkPad, x64 Fedora 35, x64 XMHF

# tmp notes

