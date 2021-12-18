# QEMU booting Debian 11 hangs (IN PROGRESS)

## Scope
* x86 and x86-64

## Behavior
In QEMU, boot XMHF64, then Debian 11 Linux 5.10.0-9-686, serial port prints
```
MOV TO CR4 (flush TLB?), current=0x00002000, proposed=0x00000000
_vmx_handle_intercept_xsetbv: xcr_value=7
_vmx_handle_intercept_xsetbv: host cr4 value=00042020
```
Then the system hangs

## Debugging
* GDB shows that CPU 0 halts at `0x00000000c1bb8f42` (after a `hlt` instruction)
* Ubuntu 12.04 in QEMU can proceed further, likely need to debug Debian kernel
* Ubuntu 12.04 in HP can proceed further (GUI initialized)

### Investigate state after `_vmx_handle_intercept_xsetbv()`
`_vmx_handle_intercept_xsetbv()` is the last known location in hypervisor.
At this intercept the virtual machine's PC is at `gva = 0xc1025b7e`,
`spa = gpa = 0x1025b7e`.

```
    0x1025b7e:	xsetbv 
    0x1025b81:	mov    0xc1b883a4,%eax
    0x1025b86:	test   $0x8,%al
```

Can set a hardware breakpoint at `*0xc1025b81`. Normal breakpoint does not work.

# temporary notes below

```
TODO: Install Debian debug symbols
TODO: Try Debian kernel that is not signed for secure boot
```

