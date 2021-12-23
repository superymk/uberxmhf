# Triple fault in QEMU x64 Debian 11 after set CR0 PG

## Scope
* x64, Debian 11, QEMU
* Unknown about HP

## Behavior
After selecting XMHF64 in GRUB, then Debian 11 Linux `5.10.0-9-amd64`, see:

```
CPU(0x00): Unhandled intercept: 0x00000002
	CPU(0x00): EFLAGS=0x00010046
	SS:ESP =0x0018:0x01694238
	CS:EIP =0x0010:0x01000146
	IDTR base:limit=0x00000000:0x0000
	GDTR base:limit=0x0167ff60:0x002f
```

`System.map-5.10.0-9-amd64` shows that EIP is at `phys_startup_64`,
instructions around it are:

```
   0x1000129:	sub    $0x28,%esp
   0x100012c:	lea    0x67ca89(%rbp),%eax
   0x1000132:	mov    %edi,%ecx
   0x1000134:	mov    %esi,%edx
   0x1000136:	call   0x1000410
   0x100013b:	push   $0x10
   0x100013d:	push   %rax
   0x100013e:	mov    $0x80000001,%eax
   0x1000143:	mov    %rax,%cr0
   0x1000146:	lret   
   0x1000147:	add    %al,(%rax)
   0x1000149:	add    %al,(%rax)
```

Looks like this is the end of `startup_32` in
`arch/x86/boot/compressed/head_64.S`. I think the name is confusing, because
it looks like relocation of `startup_64`. But the content of `phys_startup_64`
and `startup_64` are different.

# Temporary notes

TODO: use the move to CR0 to set break point, maybe maliciously change `guest_RIP` to see whether `bug_008` is correctly fixed.

```
```

