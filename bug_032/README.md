# XMHF cannot boot computers with 1 CPU

## Scope
* All configurations, QEMU

## Behavior
When setting `-smp 1` in QEMU, XMHF cannot boot correctly

## Debugging
The following line in `smpg-x86.c` is incorrect.
```
if(vcpu->isbsp && (g_midtable_numentries > 1)){
```

When nproc = 1, this line will cause BSP to execute AP code (wait for SIPI), so
CPU hangs. Should remove `(g_midtable_numentries > 1)`.

Also, in `xmhf_smpguest_arch_x86vmx_initialize()` (called by BSP), should not
unmap LAPIC if only one CPU.

## Fix

`8b67cfbf3..793137a11`
* Support booting XMHF with only one CPU

