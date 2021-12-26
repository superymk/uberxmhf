# HP x64 XMHF x64 Debian cannot boot XMP

## Scope
* x64 Debian 11, x64 XMHF, HP
* Good: QEMU

## Behavior

After selecting XMHF64 in GRUB, then Debian 11 Linux `5.10.0-10-amd64`, when
Linux is trying to boot other APs, see triple fault in AP.

After patching `565cef3cf` with `print1.diff`, see the following (removed some
entries related to CPU 0)
```
0x0010:0x99862b68 -> (ICR=0x00000300 write) INIT IPI detected and skipped, value=0x0000c500
0x0010:0x99862b68 -> (ICR=0x00000300 write) INIT IPI detected and skipped, value=0x00008500
0x0010:0x99862b68 -> (ICR=0x00000300 write) STARTUP IPI detected, value=0x0000069a
CPU(0x00): processSIPI, dest_lapic_id is 0x01
CPU(0x00): found AP to pass SIPI; id=0x01, vcpu=0x10249f98
CPU(0x00): Sent SIPI command to AP, should awaken it!
CPU(0x01): SIPI signal received, vector=0x9a
0x0010:0x99862b68 -> (ICR=0x00000300 write) STARTUP IPI detected, value=0x0000069a
xmhf_runtime_main[01]: starting partition...
CPU(0x00): processSIPI, dest_lapic_id is 0x01
CPU(0x01): VMCLEAR success.
CPU(0x00): found AP to pass SIPI; id=0x01, vcpu=0x10249f98
CPU(0x01): VMPTRLD success.
CPU(0x00): destination CPU #0x01 has already received SIPI, ignoring
CPU(0x01): VMWRITEs success.
Intercept: 0x01 0x0000000a 0x0000000000000000 0x0000000000001076 ;
Intercept: 0x01 0x0000000a 0x0000000000000000 0x00000000000010c3 ;
Intercept: 0x01 0x0000001f 0x0000000000000000 0x00000000000010f1 ;
Intercept: 0x01 0x0000000a 0x0000000000000000 0x0000000000001102 ;
Intercept: 0x01 0x0000000a 0x0000000000000000 0x000000000000111a ;
Intercept: 0x01 0x0000000a 0x0000000000000000 0x000000000000112a ;
Intercept: 0x01 0x0000000a 0x0000000000000000 0x0000000000001142 ;
Intercept: 0x01 0x0000001c 0x0000000000000000 0x0000000000001033 ;
Intercept: 0x01 0x00000020 0x0000000000000000 0x000000000009ae79 ;
Intercept: 0x01 0x00000002 0x0000000000000000 0x000000000009ae80 ;
0x01 set global_halt
CPU(0x01): Unhandled intercept: 0x00000002
	CPU(0x01): EFLAGS=0x00210002
	SS:ESP =0x0018:0x0009e020
	CS:EIP =0x0008:0x0009ae80
	IDTR base:limit=0x00000000:0x0000
	GDTR base:limit=0x00099030:0x001f
```

Linux will be able to boot successfully finally with only the BSP

## Debugging

After installing the same kernel in QEMU, can reproduce the intercepts (guest
RIPS are exactly the same).
```
Intercept: 0x01 0x0000000a 0x0000000000000000 0x0000000000001076 ;
Intercept: 0x01 0x0000000a 0x0000000000000000 0x00000000000010c3 ;
Intercept: 0x01 0x0000001f 0x0000000000000000 0x00000000000010f1 ;
Intercept: 0x01 0x0000000a 0x0000000000000000 0x0000000000001102 ;
Intercept: 0x01 0x0000000a 0x0000000000000000 0x000000000000111a ;
Intercept: 0x01 0x0000000a 0x0000000000000000 0x000000000000112a ;
Intercept: 0x01 0x0000000a 0x0000000000000000 0x0000000000001142 ;
Intercept: 0x01 0x0000001c 0x0000000000000000 0x0000000000001033 ;
Intercept: 0x01 0x00000020 0x0000000000000000 0x000000000009ae79 ;
Intercept: 0x01 0x0000000a 0x0000000000000000 0xffffffff81000109 ;
Intercept: 0x01 0x0000000a 0x0000000000000000 0xffffffff81000152 ;
...
```

By breaking QEMU at `0x000000000009ae79`, can dump the instructions
(add `0x9ae00` to addresses below):
```
  59:	a1 10 d0 09 00       	mov    0x9d010,%eax
  5e:	0f 22 e0             	mov    %eax,%cr4
  61:	b8 00 c0 09 00       	mov    $0x9c000,%eax
  66:	0f 22 d8             	mov    %eax,%cr3
  69:	a1 08 d0 09 00       	mov    0x9d008,%eax
  6e:	8b 15 0c d0 09 00    	mov    0x9d00c,%edx
  74:	b9 80 00 00 c0       	mov    $0xc0000080,%ecx
  79:	0f 30                	wrmsr  
  7b:	b8 01 00 01 80       	mov    $0x80010001,%eax
  80:	0f 22 c0             	mov    %eax,%cr0
  83:	ea e0 ae 09 00 10 00 	ljmp   $0x10,$0x9aee0
```

Can also dump instructions by editing the intercept handler. Can verify that
memory around `0x9ae79` are the same for QEMU and HP.

This is very similar to a bug found when working on `bug_012`. We need to print
the arguments to `wrmsr` at `0x9ae79`.

From QEMU's intercept list, there are no intercepts at `0x9ae80`, so the move
to CR0 does not cause `vmx_handle_intercept_cr0access_ug()` to be called.
So we should compare the machine state at the `wrmsr` instruction.

Results are in `intercept_1_qemu.txt` and `intercept_1_hp.txt`. Difference:

```
Intercept: 0x01 0x00000020 0x0000000000000000 0x000000000009ae79 ;
	rax = 0x00000901
		= [ NXE LME SCE ]
	rbx = 					0x01000800			0x01100800
	rcx = 0xc0000080
		= (MSR_EFER)
	rdx = 0x00000000
	cr0 = 					0x00000021			0x00000031
		=					[ NE PE ]			[ NE ET PE ]
	cr3 = 0x0009c000
	cr4 = 					0x000426b0			0x000026b0
		=					[ PAE ... ]			[ PAE ... ]
	Common					QEMU				HP
```

Maybe useful: <https://tldp.org/HOWTO/Linux-i386-Boot-Code-HOWTO/smpboot.html>

TODO: get complete assembly code of booting
TODO: analyze why 

