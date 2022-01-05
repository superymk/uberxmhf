# Support physical address more than 4G

## Scope
* x64 XMHF

## Behavior
Currently x64 XMHF does not construct page table for memory address >= 4GB.
As a result, HP cannot boot if Linux detects memory >= 4GB

## Debugging
`_vmx_setupEPT()` need to be updated to construct EPT.
`memp-x86_64vmx-data.c`, `bplt-x86_64vmx-smp.c`, and `_paging.h` contain
definitions of relevant fields.

`xmhf_sl_arch_x86_64_setup_runtime_paging()` is used to set up runtime paging.
Arrays for page tables defined in `rntm-x86_64-data.c`.

After fixing, HP can boot successfully and see the memory range > 4GB

## Fix

`63a196411..582a35cd5`
* Extend runtime paging to 8GB
* Add warning when exceed 8GB
* Extend EPT to 8GB

