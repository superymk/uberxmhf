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

TODO: set up windbg
TODO: is the problem caused by drivers?
