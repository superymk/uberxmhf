# Should support booting Linux with PAE

## Scope
* All x86 Linux OS
* x86 and x64 XMHF

## Behavior
XMHF does not support running guest OS with PAE (I don't remember how it
crashes). But looks like in `bug_027` Windows XP can boot with PAE, so we
should at least make it Linux with x86 PAE boot.

## Debugging
Installed Debian with PAE (`5.10.0-10-686-pae`) on QEMU, can boot successfully.
But cannot run TrustVisor (problem with "to scode").

Use gdb to debug
```
source gdb/runtime.gdb
hb hpt_emhf_get_guest_hpt_type
c
```

Can see that `hpt_emhf_get_guest_hpt_type()` correctly returns `HPT_TYPE_PAE`

```
(gdb) up
#2  0x0000000010207743 in _vmx_handle_intercept_eptviolation (vcpu=0x1024b820 <g_vcpubuffers>, r=0x1d987f78 <g_cpustacks+65400>) at arch/x86_64/vmx/peh-x86_64vmx-main.c:434
434			xmhf_app_handleintercept_hwpgtblviolation(vcpu, r, gpa, gva,
(gdb) p/x gpa
$39 = 0x3740df8
(gdb) p/x gva
$40 = 0xb7f5d006
(gdb) 
```

See also: Intel volume 3 "Table 26-7. Exit Qualification for EPT Violations"

Looks like the problem happens when trying to resolve paging for `gva`.

Using "GDB snippet for performing page walk in PAE paging" in `bug_027`,
the correct paging resolution is:
```
$a  = 0xb7f5d006
$p1 = 0x1039f010 = CR3 + offset
$p2 = 0x103a0df8 = *p1 + offset
$p3 = 0x103a1ae8 = *p2 + offset
$p4 = 0x8db3000  = *p3 + offset
$b  = 0x8db3006  =  p4 + offset
```

Note the lowest 12 bits for `$39` is the same as `$p2`.

My guess is that `$p2` is incorrectly calculated to `0x3740df8`, and then
trying to read this address results in failure. However, not sure how this
address is calculated.

Next step: figure out what are `whitelist.gcr3`, `whitelist.pal_gcr3`,
`whitelist.reg_gpt_root_pa`,
`whitelist_new.hptw_pal_checked_guest_ctx.super.root_pa`.

### Look into paging

`gcr3`: set in `scode_register`
`pal_gcr3`: same as `hptw_pal_checked_guest_ctx.super.root_pa`
`reg_gpt_root_pa`: set in `scode_register`, CR3 for regular code
`hptw_pal_checked_guest_ctx.super.root_pa`: new CR3

Looks like the failed page resolution is using the old guest CR3 (for normal
code), but should use new guest CR3 (for PAL).

Is it a problem with caching? In Intel volume 3:
* P144:
  > With PAE paging, the PDPTEs are stored in internal, non-architectural
    registers. The operation of these registers is described in Section 4.4.1
    and differs from that described here.
* Section 4.4.1:
  > Certain VMX transitions load the PDPTE registers. See Section 4.11.1.
* Section 4.11.1:
  > VMX transitions that result in PAE paging load the PDPTE registers (see
    Section 4.4.1) as follows:
    VM entries load the PDPTE registers either from the physical address being
    loaded into CR3 or from the virtual-machine control structure (VMCS); see
    Section 25.3.2.4.
* Section 25.3.2.4:
  > If the control ("enable EPT") is 1, the PDPTEs are loaded from
    corresponding fields in the guest-state area (see Section 23.4.2). The
    values loaded are treated as guest-physical addresses in VMX non-root
    operation.

So we need to modify VMCS manually to update PDPTEs. We can use gdb to verify
that `vcpu->vmcs.guest_PDPTE0` etc are not updated correctly.

### Update to code

Add `VCPU_gpdpte_set()` to allow hypapp to update PDPTEs. We do not
automatically update PDPTEs in `VCPU_gcr3_set()` because hypapp may modify
values in CR3 afterwards.

After TrustVisor's two calls to `VCPU_gcr3_set()`, add calls to
`VCPU_gpdpte_set()`.

After fix, can run x86 PALs in x86 PAE kernel, both x64 and x86 XMHF, in QEMU.

In HP, x64 XMHF runs well, but x86 XMHF fails due to EPT violation. When guest
memory > 4GB, x86 XMHF cannot support PAE. This is because XMHF does not enable
PAE, so it cannot access PA > 4GB.

## Fix

`41e8844f4..8b67cfbf3`
* For PAE paging, update `guest_PDPTE{0..3}` after changing `guest_CR3`

