# Windows 10 x64 does not boot

## Scope
* bad: x64 Windows 10, x64 QEMU
* good: x86 Windows 10, x64/x86 QEMU
* Unknown on HP

## Behavior

While booting x64 Windows 10, all CPUs halt with error:
```
[00]xmhf_parteventhub_arch_x86_64vmx_intercept_handler: invalid gpr value (15). halting!
[01]xmhf_parteventhub_arch_x86_64vmx_intercept_handler: invalid gpr value (15). halting!
```

## Debugging
The problem is that when porting `_vmx_decode_reg()` from 64-bits to 32-bits,
the code change is not complete. I only changed all 8 registers from 32-bits to
64-bits, but did not add 8 new registers (R8-R15).

`vcpu->vmcs.guest_RIP` is `0xfffff80523998302`.

We can also walk the page table using `bug_017`'s
"GDB snippet for performing page walk" and see the the instruction
```
(gdb) set $a = vcpu->vmcs.guest_RIP
(gdb) set $c = vcpu->vmcs.guest_CR3
(gdb) ...
(gdb) p $b
$9 = 56189698
(gdb) p/x $b
$10 = 0x3596302
(gdb) x/i $b
   0x3596302:	mov    %r15,%cr0
(gdb) 
```

So to fix this bug, just following Intel v3
"Table 26-3. Exit Qualification for Control-Register Accesses" and add these
registers.

After fix, can see login screen in Windows 10 x64.

## Fix

`a711db2d0..43b3d6552`
* Support `CR{0,4}` intercept with R8-R15

