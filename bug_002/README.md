# QEMU booting Debian 11 hangs due to INVPCID

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

### Exception in Debian kernel

Break at `early_idt_handler_common`, then print the stack

```
(gdb) x/30x $esp
0xc1a6fe4c:	0x00000006	0x00000000	0xc105973e	0x00000060
0xc1a6fe5c:	0x00210102	0x00000000	0x00000000	0x00000000
0xc1a6fe6c:	0x00000000	0x00000000	0xffffffff	0x00000100
0xc1a6fe7c:	0xc1a6fe84	0xc1058a98	0xc1a6fec8	0xc1bba894
0xc1a6fe8c:	0x00000000	0xc1c86c00	0x00000000	0x00000002
0xc1a6fe9c:	0x00000400	0x00000000	0x00000100	0x00000100
0xc1a6feac:	0x00000000	0x00000300	0x00100000	0x00000001
0xc1a6febc:	0xc1804a60	0xc1a6fef0
```

Looks like the first numbers are (error code, ???, EIP, CS). Not sure why there
is a 0 in between. Now set a break point at EIP
`0xc105973e <native_flush_tlb_global+62>`.

### Cause of exception

Problematic instruction and arguments:
```
(gdb) x/i $eip
=> 0xc105973e <native_flush_tlb_global+62>:	invpcid -0x1c(%ebp),%eax
(gdb) p $ebp
$4 = (void *) 0xc1a6fe7c
(gdb) x/2gx ($ebp - 0x1c)
0xc1a6fe60:	0x0000000000000000	0x0000000000000000
(gdb) p $eax
$5 = 2
(gdb) 
```

### Fix the problem
In Intel v3 "Table 23-7. Definitions of Secondary Processor-Based VM-Execution
Controls", should set `Enable INVPCID`

### Attachment: GDB script for setting break points
```
# Start with gdb/x86_vm_entry.gdb
b _vmx_handle_intercept_xsetbv
c
p/x vcpu->vmcs.guest_RIP
# Should be 0x1025bce (xsetbv), next instruction is 0x1025bd1
hb *0xc1025bd1
c
d
source gdb/linux-sym.gdb
b early_fixup_exception
b native_flush_tlb_global
b early_idt_handler_common
c
```

## Fix
`eca643c3c..3b8955c74`
* Set VMCS control field to enable the INVPCID instruction

