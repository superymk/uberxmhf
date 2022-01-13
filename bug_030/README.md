# XMHF see unexpected MOV CR4 when booting Windows XP

## Scope
* x86 / x64 XMHF, x86 Windows XP SP3, QEMU

## Behavior
When booting Windows XP, see an CR4 intercept at guest RIP = `0x6a29be`.
However, should not have intercept.

Serial output is:
```
CPU(0x00): CS:EIP=0x0008:0x006a29be MOV CR4, xx
MOV TO CR4 (flush TLB?), current=0x00002020, proposed=0x00002030
```

## Debugging
Instructions near by are:
```
   0x6a29aa:	mov    %cr0,%ecx
   0x6a29ad:	and    $0x7fffffff,%ecx
   0x6a29b3:	mov    %ecx,%cr0
   0x6a29b6:	jmp    0x6a29b8
   0x6a29b8:	mov    %cr4,%edx
   0x6a29bb:	or     $0x10,%edx
   0x6a29be:	mov    %edx,%cr4
   0x6a29c1:	mov    $0x806a29d1,%edx
   0x6a29c6:	or     $0x80000000,%ecx
   0x6a29cc:	mov    %ecx,%cr0
   0x6a29cf:	jmp    *%edx
   0x6a29d1:	mov    %eax,%cr3
   0x6a29d4:	popf   
   0x6a29d5:	ret    $0x8
```

At the intercept, `edx = 0x2030`, guest CR4 = `0x2020`, CR4 mask = `0x2000`,
CR4 shadow = `0x0`. So ideally `0x6a29b8` should read `0x20` into EDX, then at
`0x6a29be` EDX should be `0x30`, instead of `0x2030`.

### Relationship with number of CPUs

After fixing `bug_032`, if set only one CPU in QEMU, cannot reproduce this bug.
At this point, can also set a break point at `0x6a29bb` and see EDX = 0x20
(expected value)

If set 2 CPUs in QEMU, break point at `0x6a29bb` will cause KVM's 
`!env->exception_has_payload` assertion error. Will receive CR4 intercept.

If mask all bits in CR0 (git `f3530051b`), no longer see the problem. If only
mask CR0.PG, also cannot see the problem. In these two cases, will see CR0
intercepts at `0x6a29b3` and `0x6a29cc`.

### Setting break point using processSIPI()

Can set an infinite loop at `processSIPI()`, and then `hb *0x6a29bb` to break
at `0x6a29bb`. This way KVM assertion error will not occur. At `0x6a29bb`, can
see that EDX = 0x2020, ECX = 0x1003b. (git `6bd5798ea`)

CPU 1 happen to be in hypervisor mode. Looks like the CR4 related VMCS fields
are the same as CPU 0 (guest CR4 = `0x2020`, CR4 mask = `0x2000`, CR4 shadow =
`0x0`). So likely not a bug in XMHF?

If mask all bits in CR4 (git `9713068b0`), CPU 0 will still have incorrect
value at `0x6a29be`. CPU 1 will have incorrect value at the next intercept.
Serial:
```
CPU(0x01): CS:EIP=0x0008:0x006a29be MOV CR4, xx
MOV TO CR4 (flush TLB?), current=0x00002020, proposed=0x00000030
                         mask   =0xffffffff, shadow  =0x00000020
CPU(0x00): CS:EIP=0x0008:0x006a29be MOV CR4, xx
MOV TO CR4 (flush TLB?), current=0x00002020, proposed=0x00002030
                         mask   =0xffffffff, shadow  =0x00000020
CPU(0x01): CS:EIP=0x0008:0x806a292c MOV CR4, xx
CPU(0x00): CS:EIP=0x0008:0x806a292c MOV CR4, xx
MOV TO CR4 (flush TLB?), current=0x00002030, proposed=0x000020b0
MOV TO CR4 (flush TLB?), current=0x00002030, proposed=0x000020b0
                         mask   =0xffffffff, shadow  =0x00000030
                         mask   =0xffffffff, shadow  =0x00002030
[cr0-01] MOV TO 0x80649d3b, current=0x8001003b, proposed=0xc001003b
[cr0-00] MOV TO 0x80649d3b, current=0x8001003b, proposed=0xc001003b
```

At this point, I think I am going to give up. It is likely not a problem in
XMHF. May be KVM / Intel.

### Update due to `bug_033`

See `bug_033` "Windows XP x86 Regression". It is related to handling CR0.

However, strange errors happen. For example, git `c1b86c231` will cause
"KVM internal error. Suberror: 3" when loading GRUB the second time. However,
`f3530051b` does not have this problem.

Now we bisect. The two commits above are in the `xmhf64-dev` branch. 
`c1b86c231` is the same as `ef05d91c8` and mask all CR0.
`f3530051b` is closest to `793137a11`. We first try to add mask all CR0 to
`793137a11` and test it.

The test result is fail. We may need to move to earlier history.

* `8b67cfbf3`: fail
* `164fea8e9`: fail

This is too early. 

After reviewing code, this is actually caused by not modifying CR0 intercept
handler correctly. Fix is `05ec956e0`. However, after fix, QEMU has strange
behavior (while booting Windows, QEMU exits without error message).

Again, give up.

### Untested ideas

* Write a kernel from scratch that simulates the operations
	* Will be difficult to reproduce sequences of CR0 and CR4 change in all CPUs

## Result

Not sure what's happening, skip for now

