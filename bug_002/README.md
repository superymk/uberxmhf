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

### Try Debian kernel that is not signed for secure boot
Tried on 5.10.0-8-686, does not work

### Try to debug Debian kernel
```sh
apt-get install linux-image-5.10.0-9-686-dbg
apt-get source linux-image-5.10.0-9-686-dbg
```

Copy files out to where gdb runs

Call stack is 
```
(gdb) bt
#0  0xc1bbb032 in native_halt () at arch/x86/include/asm/irqflags.h:66
#1  halt () at arch/x86/include/asm/irqflags.h:112
#2  early_fixup_exception (regs=<optimized out>, trapnr=<optimized out>) at arch/x86/mm/extable.c:247
#3  0xc1ba3174 in early_idt_handler_common () at /build/linux-Zn7N0z/linux-5.10.84/arch/x86/kernel/head_32.S:411
#4  0xffffffff in ?? ()
#5  0xc1c9a8ac in highstart_pfn ()
#6  0x00000000 in ?? ()
(gdb) 
```


### temporary notes below

```
Try to set a break point in early_fixup_exception
```

