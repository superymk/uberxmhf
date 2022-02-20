# ACPI problem in x64 DRT

## Scope
* HP, x64 XMHF, DRT
* Good: x86 XMHF
* Good: no DRT

## Behavior
See `### ACPI problem` in `bug_037`. Looks like some kind of triple fault
happens in `xmhf_baseplatform_arch_x86_64_acpi_getRSDP()`.

## Debugging

We use git `837bcbda2`'s `put_hex()` to print in assembly mode.
Git `df0ea08d5`, serial `20220219202043`. Can see that all segments are good.

Note that in the comparision mentioned in `bug_037`, the value of `rsdp` is
different. This means the stack address is different. In x64 without DRT this
is `0x1d9f7f84 <x_init_stack+3972>`. In x64 with DRT this is `0x0000ff7c`
(strange value). Did we mess up the stack?
* Later: looks like `bug_037`'s README is incorrect.

<del>In `sl-x86.lds.S` and `sl-x86_64.lds.S`, can see that the SL stack is
intended to be `0x0000ff7c`. We need to check x86 version's behavior</del>

<del>Git `0a8378392`, x64 drt serial `20220219204921`, x64 nodrt serial
`20220219205053`.</del>

I found that segment registers are not touched in `sl-x86_64.lds.S` at all.
Maybe we should change them. Commited `87a2a70a9`.

Things are becoming strange. For example, in git `6a2662cc1`, before the main
loop in `_acpi_computetablechecksum()` everything looks fine. Serial ends with:
```
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 1998 0x0000ff7c 0x00000014
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 1999 0x0000ff7c 0x00000014
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:69 0x0000ff7c
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:69 0x0000ff7d
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:69 0x0000ff7e
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:69 0x0000ff7f
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:69 0x0000ff80
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:69 0x0000ff81
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:69 0x0000ff82
```

In git `82ff960f1`, the situation is similar. Looks like
`xmhf_baseplatform_arch_flat_copy()` is accessing the physical memory
correctly.

Git `2842b4265`, looks like even adding wbinvd does not work.

Git `c03c4cff4`, serial below. Now looks like the problem happens before
`_acpi_computetablechecksum()`'s man loop. Also, the problem seems
deterministic (tried 3 times, the serial output ends with the same thing):
```
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 32 0x0000ff7c 0x00000014
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 33 0x0000ff7c 0x00000014
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 34 0x0000ff7c 0x00000014
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 35 0x0000ff7c 0x0
```

Git `bc82aa13f`, only changed a constant from previous commit, but the result
changes. The result is still deterministic:
```
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 47 0x0000ff7c 0x00000014
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 48 0x0000ff7c 0x00000014
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 49 0x0000ff7c 0x00000014
FILE:LINE a
```

I think it is better to set up IDT and see whether there is an exception.

### TXT reference

There should be a manual called "Intel MLE Developer's Guide
(Dec. 2009 revision)", but I have trouble finding the official version.
<https://kib.kiev.ua/x86docs/Intel/TXT/315168-006.pdf> has an unoffical
version.

Found that `acm_hdr_t` should be packed. Committed `81563bccd`.

## Fix

`da97a50e1..` (81563bccd)
* Reset segment descriptors in x64 sl entry
* Add attribute packed to `_txt_acmod.h`

# tmp notes

TODO: should `xmhf_sl_arch_sanitize_post_launch()` be called before `xmhf_baseplatform_initialize()`?
TODO: set up IDT
TODO: review `set_mtrrs_for_acmod()`
TODO: read Intel MLE Developer's Guide
TODO: what if in `xmhf_baseplatform_arch_x86_64_acpi_getRSDP()` we jump straight to the address (0xfc600) containing RSDP?

