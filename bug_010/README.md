# Triple fault in QEMU x64 Debian 11 due to GS Base

## Scope
* x64, Debian 11, QEMU
* Unknown about HP

## Behavior
After selecting XMHF64 in GRUB, then Debian 11 Linux `5.10.0-9-amd64`, after
the Linux kernel is loaded, see (need to disable KASLR):

```
CPU(0x00): CS:EIP=0x0008:0x0009e059 MOV CR4, xx
MOV TO CR4 (flush TLB?), current=0x00002020, proposed=0x00000020
CPU(0x00): CS:EIP=0x0010:0x01000057 MOV CR4, xx
MOV TO CR4 (flush TLB?), current=0x00002020, proposed=0x000000a0
CPU(0x00): Unhandled intercept: 0x00000002
	CPU(0x00): EFLAGS=0x00010046
	SS:ESP =0x0000:0x82603f48
	CS:EIP =0x0010:0x82c3142b
	IDTR base:limit=0x82604000:0x01ff
	GDTR base:limit=0x82c0f000:0x007f
```

(Pointers here are inaccurate. See below)

## Debugging

### Obtain correct guest RIP

After halt, enter GDB

```
(gdb) symbol-file xmhf/src/xmhf-core/xmhf-runtime/runtime.exe
Reading symbols from xmhf/src/xmhf-core/xmhf-runtime/runtime.exe...
(gdb) p/x vcpu->vmcs.guest_RIP
$4 = 0xffffffff82c3142b
(gdb) 
```

See that RIP is actually more than 32-bits wide, so need to change the
"Unhandled intercept" state dump.

The actual message is
```
CPU(0x00): Unhandled intercept in long mode: 0x00000002
	CPU(0x00): EFLAGS=0x00010046
	SS:RSP =0x0000:0xffffffff82603f48
	CS:RIP =0x0010:0xffffffff82c3142b
	IDTR base:limit=0xffffffff82604000:0x01ff
	GDTR base:limit=0xffffffff82c0f000:0x007f
```

### Determine Guest Page Table

Try to walk the page table for current guest RIP:
```
(gdb) symbol-file xmhf/src/xmhf-core/xmhf-runtime/runtime.exe
Reading symbols from xmhf/src/xmhf-core/xmhf-runtime/runtime.exe...
(gdb) p/x vcpu->vmcs.guest_CR3
$16 = 0x2c92000
(gdb) p/x vcpu->vmcs.guest_RIP
$17 = 0xffffffff82c3142b
(gdb) x/gx vcpu->vmcs.guest_CR3 | (((vcpu->vmcs.guest_RIP >> 39) & 0x1ff) << 3)
0x2c92ff8:	0x000000000260e067
(gdb) x/gx 0x000000000260e000 | (((vcpu->vmcs.guest_RIP >> 30) & 0x1ff) << 3)
0x260eff0:	0x000000000260f063
(gdb) x/gx 0x000000000260f000 | (((vcpu->vmcs.guest_RIP >> 21) & 0x1ff) << 3)
0x260f0b0:	0x0000000002c001e3
(gdb) p/x 0xffffffff82c3142b & ~(0x200000 - 1)
$18 = 0xffffffff82c00000
(gdb) 
```

So looks like the 2M page `0xffffffff82c00000` maps to `0x0000000002c00000`

Now see the size of this mapping. PML4E's (each entry governs 512G):
```
(gdb) x/512gx vcpu->vmcs.guest_CR3
0x2c92000:	0x0000000002c94063	0x0000000002c94063
0x2c92010:	0x0000000000000000	0x0000000000000000
...
0x2c92fe0:	0x0000000000000000	0x0000000000000000
0x2c92ff0:	0x0000000000000000	0x000000000260e067
(gdb) 
```

PDPTE's (each entry governs 1G):
```
(gdb) x/512gx 0x000000000260e000
0x260e000:	0x0000000000000000	0x0000000000000000
...
0x260efe0:	0x0000000000000000	0x0000000000000000
0x260eff0:	0x000000000260f063	0x0000000002610067
(gdb) 
```

PDE's (each entry governs 2M):
```
(gdb) x/512gx 0x000000000260f000
0x260f000:	0x00000000000001e2	0x00000000002001e2
0x260f010:	0x00000000004001e2	0x00000000006001e2
0x260f020:	0x00000000008001e2	0x0000000000a001e2
0x260f030:	0x0000000000c001e2	0x0000000000e001e2
0x260f040:	0x00000000010001e3	0x00000000012001e3
0x260f050:	0x00000000014001e3	0x00000000016001e3
0x260f060:	0x00000000018001e3	0x0000000001a001e3
0x260f070:	0x0000000001c001e3	0x0000000001e001e3
0x260f080:	0x00000000020001e3	0x00000000022001e3
0x260f090:	0x00000000024001e3	0x00000000026001e3
0x260f0a0:	0x00000000028001e3	0x0000000002a001e3
0x260f0b0:	0x0000000002c001e3	0x0000000002e001e3
0x260f0c0:	0x00000000030001e3	0x00000000032001e3
0x260f0d0:	0x00000000034001e2	0x00000000036001e2
0x260f0e0:	0x00000000038001e2	0x0000000003a001e2
...
0x260ffe0:	0x000000003f8001e2	0x000000003fa001e2
0x260fff0:	0x000000003fc001e2	0x000000003fe001e2
(gdb) x/512gx 0x0000000002610000
0x2610000:	0x0000000000000000	0x0000000000000000
...
0x2610fb0:	0x0000000000000000	0x0000000000000000
0x2610fc0:	0x0000000000000000	0x0000000000000000
0x2610fd0:	0x0000000002611067	0x0000000002612067
0x2610fe0:	0x0000000000000000	0x0000000000000000
0x2610ff0:	0x0000000000000000	0x0000000000000000
(gdb) 
```

In the PDE entries, only `1e3` are present pages, `1e2` are not present.
So the first valid PDE should be 0x00000000010001e3, the last valid PDE
should be 0x00000000032001e3. The physical address range is
`0x1000000 - 0x3200000`, The virtual address range is
`0xffffffff81000000 - 0xffffffff83200000`. This is 34 MiB.

Now, can use `x/20i 0x2c31400` to dump guest address space. When in hypervisor,
can use
`symbol-file -o -0xffffffff80000000 usr/lib/debug/boot/vmlinux-5.10.0-9-amd64`
to view symbols.

Looks like the problem is when executing `this_cpu_write` in
`cr4_init_shadow()`. The assembly is `mov    %rax,%gs:0x7d3fa165(%rip)`:
```
   0x2c31424 <x86_64_start_kernel>:	push   %rbp
   0x2c31425 <x86_64_start_kernel+1>:	mov    %rdi,%rbp
   0x2c31428 <x86_64_start_kernel+4>:	mov    %cr4,%rax
   0x2c3142b <x86_64_start_kernel+7>:	mov    %rax,%gs:0x7d3fa165(%rip)        # 0x8002b598 <cpu_tlbstate+24>
   0x2c31433 <x86_64_start_kernel+15>:	call   0x2c3119e <reset_early_page_tables>
```

In Intel's manual volume 2 this should be `MOV moffs64*,RAX`. This instruction
is only valid in long mode.

### After jumping to address above 4GB

Can use CR4 intercept to break in XMHF, then set a break point and jump to
Linux code (should be within `secondary_startup_64_no_verify`). GDB script:
```
# Start from gdb/x64_rt_pre.gdb
b vmx_handle_intercept_cr4access_ug
c
c
p/x vcpu->vmcs.guest_RIP
# 0x1000057
# (gdb) x/3i 0x1000057
#    0x1000057:	mov    %rcx,%cr4
#    0x100005a:	add    0x1612faf(%rip),%rax        # 0x2613010
#    0x1000061:	push   %rsi
# (gdb)
b *0x100005a
c
display /3i $rip
```

Then it writes to the MSR "Map of BASE Address of GS" with value
`0xffffffff82c04000` at `0xffffffff8100009b`. This MSR should be able to be
observed in VMCS.

Then `secondary_startup_64` calls `x86_64_start_kernel`, and causes the error.

### Debug GS base

After the CPU halts in XMHF due to "Unhandled intercept", printing
`vcpu->vmcs.guest_GS_base` gives 0.
```
(gdb) p vcpu->vmcs.guest_GS_base
$17 = 0
(gdb) 
```

So should not forward the WRMSR call of "Map of BASE Address of GS" for the
guest OS.

## Fix

`9223b099e..de16a3daf`
* Print full 64-bit address when reporting "Unhandled intercept"
* Use `guest_GS_base` for RDMSR, instead of forwarding

