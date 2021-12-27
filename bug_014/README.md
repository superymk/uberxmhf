# HP x64 XMHF x64 Debian cannot boot SMP

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

In real mode, `CS = 0x9900`, `EIP` is above `0x1000`. In protected mode,
`CS = 0x8`. So real mode code address is around
`(0x9900 << 4) + 0x1000 = 0x9a000`. Can dump memory using QEMU + GDB.
The dump result is in `memory2.txt`. Write `memory2.s` using the result, and
then generate object dump:
```
as --32 memory2.s -o /tmp/a.out
objdump -d /tmp/a.out > memory2.s.32
objdump -m i8086 -d /tmp/a.out > memory2.s.16
```

`memory2.s.txt` contains annotated assembly code

Can see that `0x9a000` is `trampoline_start` in
`arch/x86/realmode/rm/trampoline_64.S`.
* `0x9a050` - `0x9a177` is `verify_cpu`
* `0x9ae30` is `startup_32`

Arguments are passed through `struct trampoline_header`, defined in
`arch/x86/include/asm/realmode.h`. Arguments are set up in `setup_real_mode()`.
Address for page table is `0x9c000`. Address for arguments are
`0x9d000 - 0x9d018`.

Check page table in QEMU (need to disable KASLR)
```
(gdb) x/gx 0x9c000
0x9c000:	0x0000000003201067
(gdb) x/gx 0x0000000003201000
0x3201000:	0x0000000003202067
(gdb) x/gx 0x0000000003202000
0x3202000:	0x0000000003203067
(gdb) x/gx 0x0000000003203000 + 8 * 0x9a
0x32034d0:	0x000000000009a061
(gdb) 
```

Dump VM state using `print3.c`, result are `intercept_3_hp.txt` and
`intercept_3_qemu.txt`. However did not find anything useful.

### CPU family

On HP laptop, `lscpu` shows family and mode = 6 and 37. In Intel's manual this
is `06_25H`, which is "Intel(R) microarchitecture code name Westmere".
In QEMU, if use `-cpu Westmere,vmx=yes`, still successful.

### Guess about what is happening

Recall in `bug_012`, I found that in another configuration memory mysteriously
change and cause problems. Maybe some incorrect settings in the hypervisor
cause this change. For `bug_012`, memory address at `0x9d00c` becomes
corrupted. For this bug, maybe memory for GDT etc becomes corrupted?

However, after future print debugging, content in GDT are exactly the same,
compared between QEMU and HP.

### Triple fault check list for x86-64 mode
Intel volume 3's "Figure 2-2. System-Level Registers and Data Structures in
IA-32e Mode and 4-Level Paging" may be helpful.

* Paging
	* State
		* `CR0.PG` change from 0 to 1 (`bit 31 (0x80000000)`)
		* `CR4.PAE = 1` (`bit 5 (0x20)`)
		* `IA32_EFER.LME = 1` (`0xC0000080, bit 8 (0x100)`)
	* Walk page table
		* `CR3 & 0xfff == 0`
		* Split RIP into bits
		* `*(CR3 & ~0xfff | bit_47_39)` is a valid entry
		* `*(_ & ~0xfff | bit_38_30)` is a valid entry
		* `*(_ & ~0xfff | bit_29_21)` is a valid entry
		* `*(_ & ~0xfff | bit_20_12)` is a valid entry
		* Large page: `PS, bit 7 (0x80)`
* GDT points to valid memory
* CS points to the correct GDT entry
* (TODO: Incomplete)

### Temporary notes

TODO: write script to record HP AMT to file
TODO: dump VMCS in serial output and compare
TODO: try to change VMCS to let the first fault in the triple fault cause VMEXIT
TODO: try to intercept the write to CR0, and see whether VMENTRY fail occurs
TODO: try to capture entire guest state and simulate (costs a lot of time)
TODO: write triple fault check list

