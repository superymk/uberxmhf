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

TODO: maybe set a break point at MOV CR0
TODO: set a break point at `0x6a29bb` and see value of EDX.
TODO: write a kernel from scratch that simulates the operations

