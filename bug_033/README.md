# Entry state of virtual APs does not follow specification

## Scope
All configurations

## Behavior

After guest's BSP sends INIT-SIPI-SIPI to AP, AP starts, but some registers
have incorrect value compared to Intel volume 3 "Table 9-1".

## Debugging

### Changing states to follow INIT specification

In Intel volume 3 "Table 9-1. IA-32 and Intel 64 Processor States Following
Power-up, Reset, or INIT", the exact state after receiving INIT is defined.
Recall that booting AP is INIT-SIPI-SIPI.

The behavior of inter-processor interrupts are defined in
"10.6.1 Interrupt Command Register (ICR)".

Currently, break at VMENTRY for an AP and compare states between VMCS and
Table 9-1, INIT column. Can see that the following are incorrect (using
Debian x86 to test):
* (CS and EIP are ignored, because they are affected by SIPI)
* `vcpu->vmcs.guest_RFLAGS = 0x202`, should be `0x2`
* Effective CR0 is `0x20`, should be `0x60000010`
* EDX: looks like 0x80, should be `0x000n06xx`
* `vcpu->vmcs.guest_GDTR_limit = 0x0`, should be `0xfff`
* `vcpu->vmcs.guest_IDTR_limit = 0x3ff`, should be `0xfff`
* `vcpu->vmcs.guest_LDTR_limit = 0x0`, should be `0xfff`
* `vcpu->vmcs.guest_DR7 = 0x0`, should be `0x400`

What should be the environment for BSP? It should be the environment between
BIOS and GRUB. <https://en.wikipedia.org/wiki/BIOS#Boot_environment> has
something but without reference.

Another reference is <https://0xax.github.io/grub/>, and source code
<https://git.savannah.gnu.org/cgit/grub.git/tree/grub-core/boot/i386/pc/boot.S>

### Discovering states of GRUB entry

If add `-S` to QEMU, can see that right after entry CS:EIP is at
`0xf000:0x0000fff0`. This should be following column "Power up" in Table 9-1.

Then use `hb *0x7c00` to break at GRUB (first sector of MBR)
The jump out of this code is at `0x7d67`, where `*0x7c5a = 0x00018000`
```
    7d63:	f3 a5                	rep movsw %ds:(%si),%es:(%di)
    7d65:	1f                   	pop    %ds
    7d66:	61                   	popa   
    7d67:	ff 26 5a 7c          	jmp    *0x7c5a
```

And then the CPU jumps to CS:EIP = `0:0x8000`. Now can use GDB to dump machine
state.

Can see that CR0 = 0x10, `CR2 = CR3 = CR4 = {C..G}S = SS = 0`,
EAX = 1, EBX = 0x7000, ECX = 1, EDX = 0x80, EDI = EBP = 0, ESI = 0x7c05,
ESP = 0x1ffe, EFLAGS = 0x246, cannot see GDT and LDT in GDB.

In git `0f7229788`, we change:
* IDTR, LDTR, GDTR, DR7, RFLAGS (mostly when `!vcpu->isbsp`)

Change `__vmx_start_hvm()` interface to change EDX (git `0ec2e511e`):
* EDX

Change guest CR0 and CR0 shadow (git `9c0f9491a`):
* CR0

### Windows XP x86 Regression

At git `eeac1dff3`, Windows XP x86 SP3 will fail to boot the AP. AP will see
triple fault, but BSP can continue booting. Serial is the same as `bug_027`:
```
CPU(0x01): Unhandled intercept: 0x00000002
	CPU(0x01): EFLAGS=0x00010006
	SS:ESP =0x0000:0x00000000
	CS:EIP =0x1f00:0x000003a6
	IDTR base:limit=0xf78c4590:0x07ff
	GDTR base:limit=0xf78c4190:0x03ff
```

If revert commit `9c0f9491a`, the problem no longer exists

Looks like the value of CR3 is very strange. Does not look like content for
page table.
```
(gdb) p/x vcpu->vmcs.guest_CR3
$11 = 0x23c5000
(gdb) x/10gx vcpu->vmcs.guest_CR3
0x23c5000:	0xe57268540a4f0000	0x0000000000000001
0x23c5010:	0x2200000081fc6e70	0xe1003c0600000001
0x23c5020:	0x0000000000700006	0x81fc502881fc5028
0x23c5030:	0x81fc503081fc5030	0xf7a5d000f7a60000
0x23c5040:	0x0000000000000000	0x00000500f7a5fd24
(gdb) 
```

Need to see the expected value using GDB without running XMHF. Use hardware
breakpoint. Before Windows is booted:
```
t 2
hb *0x3a3
hb *0x1f3a3
hb *0x3a6
hb *0x1f3a6
c
```

The state is:
```
(gdb) p/x $cr3
$2 = 0x25c5000
(gdb) x/10gx $cr3
0x25c5000:	0x00000000025c4001	0x00000000025c3001
0x25c5010:	0x00000000025c2001	0x00000000025c1001
0x25c5020:	0x0000000000000000	0x0000000000000000
0x25c5030:	0x0000000000000000	0x0000000000000000
0x25c5040:	0x0000000000000000	0x0000000000000000
(gdb) p $cr4
$3 = [ PAE ]
(gdb) p/x $cr4
$4 = 0x20
(gdb) 
```

Is the problem still not updating PDPTEs?
```
(gdb) p/x vcpu->vmcs.guest_PDPTE0
$6 = 0x0
(gdb) p/x vcpu->vmcs.guest_PDPTE1
$7 = 0x0
(gdb) p/x vcpu->vmcs.guest_PDPTE2
$8 = 0x0
(gdb) p/x vcpu->vmcs.guest_PDPTE3
$9 = 0x0
(gdb) p/x vcpu->vmcs.guest_CR4
$10 = 0x2020
(gdb) p/x vcpu->vmcs.guest_CR0
$11 = 0x8000003b
(gdb) 
```

The answer is yes. However, to write the correct handler need to walk HPT. So
for now just HALT. See commit `ef05d91c8` for implementation. See commit
`ee1e4c976` for an insecure fix.

### Clear misunderstanding

I think I misunderstood this command in XMHF setup (GRUB menuentry).
```
	module --nounzip (hd0)+1	# should point to where grub is installed
```

I thought that `+1` it means loading the 2nd sector of hd0, but after reading
<https://www.gnu.org/software/grub/manual/grub/html_node/Block-list-syntax.html>
I think it means loading 1 sector starting from 1st sector.

In above I collected the state at `0:0x8000`, which is when code starts running
on the 2nd sector. However, I should use `0x7c00`.

Use `hb *0x7c00` and `b *0x7c00` to break, again use GDB to dump state.

Can see that CR0 = 0x10, `CR2 = CR3 = CR4 = {C..G}S = SS = 0`,
EAX = 0xaa55, EBX = ECX = ESI = EDI = EBP = 0, EDX = 0x80, ESP = 0x6f00,
EFLAGS = 0x202, cannot see GDT and LDT in GDB.

As a result, changed BSP's initial CR0 from 0x20 (from VMX fixed MSR) to 0x10
(1. according to Intel manual and SeaBIOS source code; 2. by experiment on
QEMU). Commit `a711db2d0`.

## Fix

`793137a11..a711db2d0`
* Make virtual APs' state follow Intel's specification
* Update CR0 intercept handler to halt when PAE changes to set

