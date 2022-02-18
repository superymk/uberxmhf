# ACPI problem in x64 DRT

## Scope
* HP, x64 XMHF, DRT
* Good: x86 XMHF
* Good: no DRT

## Behavior
See `### ACPI problem` in `bug_037`. Looks like some kind of triple fault
happens in `xmhf_baseplatform_arch_x86_64_acpi_getRSDP()`.

## Debugging

TODO: use 837bcbda2's put_hex()
