# QEMU Debian 11 x64 cannot boot

## Scope
* x86-64 XMHF, x86-64 Debian 11, QEMU
* HP machine not tried yet

## Behavior
When booting x64 Debian 11 with x64 XMHF, after some e820 intercepts, see
VM-ENTRY error
```
VM-ENTRY error: reason=0x80000021, qualification=0x0000000000000000
Guest State follows:
guest_CS_selector=0x0010
guest_DS_selector=0x0018
guest_ES_selector=0x0018
guest_FS_selector=0x0018
guest_GS_selector=0x0018
guest_SS_selector=0x0018
guest_TR_selector=0x0020
guest_LDTR_selector=0x0000
guest_CS_access_rights=0x0000c09b
guest_DS_access_rights=0x0000c093
guest_ES_access_rights=0x0000c093
guest_FS_access_rights=0x0000c093
guest_GS_access_rights=0x0000c093
guest_SS_access_rights=0x0000c093
guest_TR_access_rights=0x0000808b
guest_LDTR_access_rights=0x0001c000
guest_CS_base/limit=0x00000000/0xffff
guest_DS_base/limit=0x00000000/0xffff
guest_ES_base/limit=0x00000000/0xffff
guest_FS_base/limit=0x00000000/0xffff
guest_GS_base/limit=0x00000000/0xffff
guest_SS_base/limit=0x00000000/0xffff
guest_GDTR_base/limit=0x0167ff60/0x002f
guest_IDTR_base/limit=0x00000000/0x0000
guest_TR_base/limit=0x00000000/0x0fff
guest_LDTR_base/limit=0x00000000/0xffff
guest_CR0=0x80000001, guest_CR4=0x00002020, guest_CR3=0x0339c000
guest_RSP=0x01694238
guest_RIP=0x01000146
guest_RFLAGS=0x00000046
```

## Debugging

Remember thatn `bug_007` is also VM-ENTRY error.

By setting a print in `xmhf_parteventhub_arch_x86_64vmx_intercept_handler()`,
looks like the last intercept is `VMX_VMEXIT_CRX_ACCESS` (same as `bug_007`).

Add a print in `vmx_handle_intercept_cr0access_ug()`, see `bug_007`'s `git.diff`

Serial prints
```
[cr0-00] MOV TO, current=0x00000021, proposed=0x80000001, shadow=0x00000020, mask=0x60000020, post_cr0=0x80000001, post_shadow=0x80000001, post_mask=0x60000020
```

Same as `bug_007`, the problem should be caused by trying to unset bit 5 of CR0

The problem is in 

Reference: Intel volume 3
* C. VMX BASIC EXIT REASONS (0x1c = Control-register accesses, 0x21 -> 25.3.1)
* 25.3.1 Checks on the Guest State Area (list of checks, refers to 22.8)
* 22.8 RESTRICTIONS ON VMX OPERATION (refers to A.7)
* A.7 VMX-FIXED BITS IN CR0 (definition of fixed bits in CR0)

### After fix

After fix, for `bug_008` the same guest RIP results in a triple fault.
For `bug_007`, the program runs and then triple fault at another guest RIP.
So I guess the triple fault after `bug_008`'s fix is another problem.

## Fix

`9b1777042..801fe353f`
* For CR0 fields that is forced to be 1, enforce this requirement

