# Windows XP performs VMCALL

## Scope
* All x86 Linux OS
* x86 and x64 XMHF

## Behavior
See also: `bug_027`

For a very strange reason Windows XP performs VMCALL after boot. However, not
sure why these instructions are called.

## Debugging

Likely related to something called vAPIC / APICv, from Hyper-V? Resources:
* <https://listman.redhat.com/archives/vfio-users/2016-June/msg00055.html>
* <https://github.com/torvalds/linux/commit/b93463aa59d67b21b4921e30bd5c5dcc7c35ffbd>
* <https://lists.gnu.org/archive/html/qemu-devel/2012-02/msg00516.html>
* <https://lore.kernel.org/linux-hyperv/DM5PR21MB0137FCE28A16166207E08C7CD79E0@DM5PR21MB0137.namprd21.prod.outlook.com/>

Using `objdump -d` on all files under `C:\WINDOWS\system32`, can see that
only `wow32.dll` contains the VMCALL instruction. `dllcache/wow32.dll` is the
same file. But it does not look like what we are finding.

If search for VMCALL using `grep -r $'\x0f\x01\xc1'`, still does not look
right. Matches are:
```
DirectX/Dinput/ia3002_1.png
DirectX/Dinput/ms26.png
DirectX/Dinput/SV-262e1.png
DirectX/Dinput/sv2511.png
DirectX/Dinput/ms3b.png
dllcache/hwxkor.dll
dllcache/oembios.bin
dllcache/dxdiagn.dll
dllcache/expsrv.dll
dllcache/framd.ttf
dllcache/hanja.lex
dllcache/tourW.exe
dllcache/wow32.dll
oembios.bin
dxdiagn.dll
wow32.dll
expsrv.dll
```

### WinDbg

Set up WinDbg. See `windows.md`

Can break at VMCALL instruction using `bp 0x800cb189`, then use `g` to continue

Use `k` to print stack, but see "frame ip not in any known module. following
frames may be wrong" error. Stack is something like:
```
0x800cb189
KdExitDebugger+0x23
KdpReport+0x7a
KdpTrap+0x108
KiDispatchException+0x129
CommonDIspatchException+0x4d
...
```

Can also break at `0x806e6422` (caller to function containing VMCALL), and
stack is:
```
hal!KfLowerIrql+0x12
nt!KiFreezeTargetExecution+0xe7
nt!KiIpiServiceRoutine+0x9c
hal!HalpIpiHandler+0xb8
nt!KeEnableInterrupts+0xd
nt!UpdateSystemTime+0x16a
intelppm!AcpiC1Idle+0x12
nt!KiIdleLoop+0x10
```

The top most address points to `call 0x800cb0c0`

`0x806e6428 = KeGetCurrentIrql()`, first instruction is `call 0x800cb0c0`

Ref: <https://www.geek-share.com/detail/2674040068.html>

Looks like Windows is dealing with something related to IRQ. We are now sure
that caller is `KfLowerIrql()`. However, there are no symbols for `0x800cb0c0`,
so cannot continue debugging.

### VAPIC reading

Ref: <https://stackoverflow.com/questions/47950831/understanding-the-virtual-apic-page-for-x2apic>

Looks like vapic is called "APIC VIRTUALIZATION" in Intel's manual, see chapter
28.

For example, in
<https://lore.kernel.org/linux-hyperv/DM5PR21MB0137FCE28A16166207E08C7CD79E0@DM5PR21MB0137.namprd21.prod.outlook.com/>,
someone mentioned that Hyper-V will set CPUID to indicate vapic is available.

### Ideas not tries

* Re-install Windows, disable CPUID 0x40000000, disable network, see what
  happens

## Result

Not fixed because of low priority (Windows XP is EOL). Focus on Windows 10.

