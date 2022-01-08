# Support running x64 PALs

## Scope
* x64 XMHF, x64 Debian

## Behavior
Currently TrustVisor by design only supports x86 calling convention. Need to
support x64.

## Debugging

Change `trustvisor.h` to hold 64-bit pointers (breaks binary compatibility)
* Need to care about alignment. In x86, the largest alignment is 4. But in x64,
  the largest alignment is 8. So need to align manually.

### GDB snippet for performing page walk

```gdb
set $a = vcpu->vmcs.guest_GDTR_base
set $c = vcpu->vmcs.guest_CR3
set $p0 = ($c           ) & ~0xfff | ((($a >> 39) & 0x1ff) << 3)
set $p1 = (*(long*)($p0)) & ~0xfff | ((($a >> 30) & 0x1ff) << 3)
set $p2 = (*(long*)($p1)) & ~0xfff | ((($a >> 21) & 0x1ff) << 3)
set $p3 = (*(long*)($p2)) & ~0xfff | ((($a >> 12) & 0x1ff) << 3)
set $p4 = (*(long*)($p3)) & ~0xfff
set $b = $p4 | ($a & 0xfff)
```

To test whether the application is running in compatibility mode, need to
read GDT entry for CS. Need to walk page table. Using the script above, GDB
example:
```
(gdb) set $a = vcpu->vmcs.guest_GDTR_base
...
(gdb) p/x $b
$46 = 0x800000000fe0b000
(gdb) p/x vcpu->vmcs.guest_GDTR_limit 
$47 = 0x7f
(gdb) x/16gx 0xfe0b000
0xfe0b000:	0x0000000000000000	0x00cf9b000000ffff
0xfe0b010:	0x00af9b000000ffff	0x00cf93000000ffff
0xfe0b020:	0x00cffb000000ffff	0x00cff3000000ffff
0xfe0b030:	0x00affb000000ffff	0x0000000000000000
0xfe0b040:	0x00008b0030004087	0x00000000fffffe00
0xfe0b050:	0x0000000000000000	0x0000000000000000
0xfe0b060:	0x0000000000000000	0x0000000000000000
0xfe0b070:	0x0000000000000000	0x0040f50000000000
(gdb) p/x vcpu->vmcs.guest_CS_selector 
$48 = 0x33
(gdb) x/2gx 0xfe0b000 + 0x30
0xfe0b030:	0x00affb000000ffff	0x0000000000000000
(gdb) 
```

Can see that the guest is running IA-32e mode.

Actually, should use `vcpu->vmcs.guest_CS_access_rights` bit 13 to test this.

Terminology: reference <https://en.wikipedia.org/wiki/X86-64#Operating_modes>
* Long mode (`VCPU_glm()`)
	* 64-bit mode (`VCPU_g64()`)
	* Compatibility mode
* Legacy mode
	* Protected mode
	* ...

Then update `scode.c` to follow x64 calling convention (first 6 arguments are
RDI, RSI, RDX, RCX, R8, R9). Also need to change size of return address.

## Fix

`592fbd12c..2317b0cda`
* Update `pal_demo` for 64-bits
* Change `trustvisor.h` to be able to hold 64-bit pointers
* Update TrustVisor's APIs to accept 64-bit pointers
* Detect whether guest application running in 64-bit mode
* Implement `scode_marshall64()` to marshall arguments for 64-bit mode
* Update `pal_demo` to test argument passing

