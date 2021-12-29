# Optimize `vmx_handle_intercept_cr0access_ug()`

## Scope
* x64 XMHF

## Behavior
Currently `vmx_handle_intercept_cr0access_ug()` updates bit 9 of
`vcpu->vmcs.control_VM_entry_controls` always. However, it only needs to be
updated when CR0.PG changes.

However, when trying to change the code, see VM-ENTRY error.
```
VM-ENTRY error: reason=0x80000021, qualification=0x0000000000000000
Guest State follows:
guest_CS_selector=0x0000
guest_DS_selector=0x0000
guest_ES_selector=0x0000
guest_FS_selector=0x0000
guest_GS_selector=0x0020
guest_SS_selector=0x0000
guest_TR_selector=0xffff
guest_LDTR_selector=0xffff
guest_CS_access_rights=0x0001c000
guest_DS_access_rights=0x0001c000
guest_ES_access_rights=0x0001c000
guest_FS_access_rights=0x0001c000
guest_GS_access_rights=0x0000808b
guest_SS_access_rights=0x0001c000
guest_TR_access_rights=0x00000000
guest_LDTR_access_rights=0x00000000
guest_CS_base/limit=0x00000000/0xffff
guest_DS_base/limit=0x00000000/0xffff
guest_ES_base/limit=0x00000000/0xffff
guest_FS_base/limit=0xffffffff82c04000/0xffff
guest_GS_base/limit=0x00000000/0x0fff
guest_SS_base/limit=0x00000000/0xffff
guest_GDTR_base/limit=0xffffffff82e6a000/0xc000
guest_IDTR_base/limit=0x00000400/0xa09b
guest_TR_base/limit=0xffffffff82c0f000/0x0fff
guest_LDTR_base/limit=0x00000000/0x007f
guest_CR0=0x0260a000, guest_CR4=0x00000000, guest_CR3=0x000626b0
guest_RSP=0xffffffff81062b72
guest_RIP=0x00010046
guest_RFLAGS=0x00000100
```

## Debugging
Working on commit `da32a4e60` / `0.c`. 
* First `1.diff` / `1.c` has the problem.
* Then found that `0.c` also has the problem.

Maybe need to `make clean`?
* After rebuild, `0.c` is good
* Then building `1.c` (without rebuild) is good

Then continue cleanup / development
* `2.c` is good
* `3.c` is good (final version)

So in conclusion, the problem is fixed by rebuilding.

## Fix

`da32a4e60..050a8138e`
* Only update "IA-32e mode guest" in VMCS when CR0.PG changes

