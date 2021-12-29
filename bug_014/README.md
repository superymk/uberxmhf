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

### Compare full VMCS

Use `amt.py` to automatically save serial output to file (`/tmp/hpamt`).
Use grep to remove uninteresting data above intercepts.
```
grep -A100000 Intercept /tmp/amt > /tmp/q
grep -A100000 Intercept /tmp/hpamt > /tmp/h
```

Program in `print4.c`, output in `intercept_4_hp.txt` and
`intercept_4_qemu.txt`.

Difference in VMCS fields are:
* `vcpu->vmx_msrs`
	* Should be unrelated
* `vcpu->vmx_msr_efcr`
	* HP:   `0x000000000000ff07`
	* QEMU: `0x0000000000000005`
	* Should be unrelated
* `vcpu->vmcs.control_VMX_seccpu_based`
	* HP:   `0x000000aa`
	* QEMU: `0x000010aa`
	* HP does not enable `Enable INVPCID` because not supported. OK.
* `vcpu->vmcs.host_CR4`
	* HP  : `0x0000000000002668`
	* QEMU: `0x0000000000042020`
	* Looks like unrelated fields. Also this is **host** CR4, should be OK
* `vcpu->vmcs.info_vminstr_error`
	* HP  : `0x00000000`
	* QEMU: `0x0000000c`
	* Related to unsupported VMREAD / VMWRITE in QEMU, OK.
* `vcpu->vmcs.info_guest_linear_address`
	* HP  : `0x0000000000000000`
	* QEMU: `0x000000000009d014`
	* This field is undefined for intercepts not using this field. So OK.

### Running normal virtual machines in HP

Just to make sure that HP supports x64 SMP virtual machines.

Install QEMU and KVM in HP, can boot x64 Debian with 4 cores. VMX are also
enabled in the QEMU virtual machine. So HP hardware should be OK.

### Intercepting change in CR0.PG

A better NOP loop for GDB (just change RAX to exit the loop):
```c
asm volatile("xor %%rax, %%rax; 1: test %%rax, %%rax; jz 1b; nop; nop;" : : : "%rax", "cc");
```

We intercept change to CR0's PG (paging) bit. First, we need to make sure
intercepting this bit works correctly.

Patch commit `ff1efb9d3` with `cr0_5.diff`, and run it in x64 XMHF x86 Debian
QEMU, we see that everything works well.

If run it in x64 XMHF x64 Debian QEMU, see VM-ENTRY error in AP. The behavior
of HP and QEMU are the same. Ideally, adding this interception should only
cause performance downgrade. So the error means that the hypervisor should be
making some invalid configurations.

We have now reproduced some bug on QEMU. QEMU allows debugging, so it makes our
life easier. Also, there is a check list in Intel's manual for checking the
reason of VM-ENTRY error.

### Changes in CR0

The two CR0 intercepts in QEMU are:

```
[cr0-01] MOV TO, current=0x00000020, proposed=0x00000001
[cr0-01]       mask: 0x00000000e0000020
[cr0-01] requested : 0x0000000000000001
[cr0-01] old gstCR0: 0x0000000000000020
[cr0-01] old shadow: 0x0000000000000020
[cr0-01] old entctl: 0x00000000000011ff
[cr0-01] guest efer: 0x0000000000000000
[cr0-01] new gstCR0: 0x0000000000000021
[cr0-01] new shadow: 0x0000000000000001
[cr0-01] new entctl: 0x00000000000011ff
[cr0-01] MOV TO, current=0x00000021, proposed=0x80010001
[cr0-01]       mask: 0x00000000e0000020
[cr0-01] requested : 0x0000000080010001
[cr0-01] old gstCR0: 0x0000000000000021
[cr0-01] old shadow: 0x0000000000000001
[cr0-01] old entctl: 0x00000000000011ff
[cr0-01] guest efer: 0x0000000000000901
[cr0-01] new gstCR0: 0x0000000080010021
[cr0-01] new shadow: 0x0000000080010001
[cr0-01] new entctl: 0x00000000000013ff
VM-ENTRY error: reason=0x80000021, qualification=0x0000000000000000
...
```

For reference, there are 3 intercepts on BSP that changes CR0.PG:
```
[cr0-00] MOV TO, current=0x00000021, proposed=0x80000001
[cr0-00]       mask: 0x00000000e0000020
[cr0-00] requested : 0x0000000080000001
[cr0-00] old gstCR0: 0x0000000000000021
[cr0-00] old shadow: 0x0000000000000020
[cr0-00] old entctl: 0x00000000000011ff
[cr0-00] guest efer: 0x0000000000000100
[cr0-00] new gstCR0: 0x0000000080000021
[cr0-00] new shadow: 0x0000000080000001
[cr0-00] new entctl: 0x00000000000013ff
[cr0-00] MOV TO, current=0x80000021, proposed=0x00000001
[cr0-00]       mask: 0x00000000e0000020
[cr0-00] requested : 0x0000000000000001
[cr0-00] old gstCR0: 0x0000000080000021
[cr0-00] old shadow: 0x0000000080000001
[cr0-00] old entctl: 0x00000000000013ff
[cr0-00] guest efer: 0x0000000000000500
[cr0-00] new gstCR0: 0x0000000000000021
[cr0-00] new shadow: 0x0000000000000001
[cr0-00] new entctl: 0x00000000000011ff
[cr0-00] MOV TO, current=0x00000021, proposed=0x80000001
[cr0-00]       mask: 0x00000000e0000020
[cr0-00] requested : 0x0000000080000001
[cr0-00] old gstCR0: 0x0000000000000021
[cr0-00] old shadow: 0x0000000000000001
[cr0-00] old entctl: 0x00000000000011ff
[cr0-00] guest efer: 0x0000000000000100
[cr0-00] new gstCR0: 0x0000000080000021
[cr0-00] new shadow: 0x0000000080000001
[cr0-00] new entctl: 0x00000000000013ff
```

There are 3 transitions from no paging to long mode. One in AP (failed), and
two in BSP (good). We can compare them:
```
Same fields:
		  mask = 0xe0000020
	old gstCR0 = 0x00000021
	old entctl = 0x000011ff
	new entctl = 0x000013ff

stat	requested 	old shadow	guest efer	new gstCR0	new shadow
bad 	0x80010001	0x00000001	0x00000901	0x80010021	0x80010001
good	0x80000001	0x00000020	0x00000100	0x80000021	0x80000001
good	0x80000001	0x00000001	0x00000100	0x80000021	0x80000001
```

We guess that if we change CR0 and EFER from bad value to good value, VM-ENTRY
should now succeed. Also, if we do not change anything, VM-ENTRY should
still succeed.

Changed bits are:
	* `control_VM_entry_controls`.(bit 9)
	* CR0.WP
	* CR0.PG

Different bits between good and bad are:
	* CR0.WP (`bit 12, 0x10000`, Intel v3 page 75)
	* EFER.SCE (`bit 0, 0x1`, Intel v4 page 70)
	* EFER.NXE (`bit 11, 0x800`, Intel v4 page 70)

### Experiment with set CR0 intercept

Experiment cases:
* No change: bad
* Prevent setting CR0.PG: good
* Prevent setting CR0.WP: bad
* Prevent setting CR0.WP, remove EFER.SCE and EFER.NXE: bad

Currently the state printed in serial output looks like the second good above.
This is strange.

Following Intel volume 3 "25.3.1 Checks on the Guest State Area", the
"Access-rights fields" for "TR"'s "Bits 3:0 (Type)" says:
> If the guest will be IA-32e mode, the Type must be 11 (64-bit busy TSS).

However, after halt and connecting GDB, looks like type is 3:
```
(gdb) p/x vcpu->vmcs.guest_TR_access_rights & 0xf
$14 = 0x3
(gdb) 
```

### Checking TR access rights

Looks like a likely cause. In all CR0 intercepts on BSP,
`vcpu->vmcs.guest_TR_access_rights = 0x0000808b`. But on AP,
`vcpu->vmcs.guest_TR_access_rights = 0x00000083`.

In Intel's manual, there are no useful information if search for
"TR access rights". Should search for "access rights".

The access rights structure is defined in
Intel v3 "23.4.1 Guest Register State": see Table 23-2

According to Intel v3 "25.3.1.2 Checks on Guest Segment Registers" and
"25.3.2.2 Loading Guest Segment Registers and Descriptor-Table Registers",
TR must be usable.

Also note this sentence and footnote in "23.4.1 Guest Register State":
> Bit 16 indicates an unusable segment. Attempts to use such a segment fault
  except in 64-bit mode. In general, a segment register is unusable if it has
  been loaded with a null selector. [2]
> [2] There are a few exceptions to this statement. ... In contrast, the TR
  register is usable after processor reset despite having a null selector;
  see Table 10-1 in the Intel(R) 64 and IA-32 Architectures Software
  Developer's Manual, Volume 3A.

Not sure why Table 10-1 is related to this. But we now know that TR is defined
to be usable.

### Experimenting with TR access rights

(Remove CR0.PG intercept)

In QEMU, if print `vcpu->vmcs.guest_TR_access_rights` for each interrupt, see
```
Intercept: 0x01 0x0000000a 0x0000000000000000 0x9900:0x0000000000001076 ;
[int-01] TR_acc_rts: 0x0000000000000083
Intercept: 0x01 0x0000000a 0x0000000000000000 0x9900:0x00000000000010c3 ;
[int-01] TR_acc_rts: 0x0000000000000083
Intercept: 0x01 0x0000001f 0x0000000000000000 0x9900:0x00000000000010f1 ;
[int-01] TR_acc_rts: 0x0000000000000083
Intercept: 0x01 0x0000000a 0x0000000000000000 0x9900:0x0000000000001102 ;
[int-01] TR_acc_rts: 0x0000000000000083
Intercept: 0x01 0x0000000a 0x0000000000000000 0x9900:0x000000000000111a ;
[int-01] TR_acc_rts: 0x0000000000000083
Intercept: 0x01 0x0000000a 0x0000000000000000 0x9900:0x000000000000112a ;
[int-01] TR_acc_rts: 0x0000000000000083
Intercept: 0x01 0x0000000a 0x0000000000000000 0x9900:0x0000000000001142 ;
[int-01] TR_acc_rts: 0x0000000000000083
Intercept: 0x01 0x0000001c 0x0000000000000000 0x9900:0x0000000000001033 ;
[int-01] TR_acc_rts: 0x0000000000000083
[cr0-01] MOV TO, current=0x00000020, proposed=0x00000001
[cr0-01] TR_acc_rts: 0x0000000000000083
[cr0-01]       mask: 0x0000000060000020
[cr0-01] requested : 0x0000000000000001
[cr0-01] old gstCR0: 0x0000000000000020
[cr0-01] old shadow: 0x0000000000000020
[cr0-01] old entctl: 0x00000000000011ff
[cr0-01] guest efer: 0x0000000000000000
[cr0-01] new gstCR0: 0x0000000000000021
[cr0-01] new shadow: 0x0000000000000001
[cr0-01] new entctl: 0x00000000000011ff
Intercept: 0x01 0x00000020 0x0000000000000000 0x0008:0x000000000009ae79 ;
[int-01] TR_acc_rts: 0x0000000000000083
Intercept: 0x01 0x0000000a 0x0000000000000000 0x0010:0xffffffff81000109 ;
[int-01] TR_acc_rts: 0x000000000000008b
Intercept: 0x01 0x0000000a 0x0000000000000000 0x0010:0xffffffff81000152 ;
[int-01] TR_acc_rts: 0x000000000000008b
```

So the CPU should be automatically changing TR access rights after MOV CR0.

In `part-x86_64vmx.c`, can see where TR access rights is initialized:
`vcpu->vmcs.guest_TR_access_rights = 0x83;`

If change this to `vcpu->vmcs.guest_TR_access_rights = 0x8b;`, and add CR0.PG
intercept back, QEMU and HP can all boot correctly.

If only change initial `guest_TR_access_rights`, and remove CR0.PG intercept,
still can work correctly.

Currently, the git repo is commit `ff1efb9d3` patched with
`tr_access_right6.diff`. Will clean up debug code and commit.

### Untried ideas for debugging

Related to TR access rights

* Intel v2 "LAR-Load Access Rights Byte" mentioned "access right"
	* But not related to TR access rights in VMX
* Some places mention that "segment selector is not null"
	* Most segment selectors are unusable when null, but not TR

General ideas

* Try to change VMCS to let the first fault in the triple fault cause VMEXIT
	* Will be interesting to see which exception will occur. But fixed before
	  trying.
* Try to capture entire guest state and simulate (costs a lot of time)
	* Too costly
* Consider fixing the problem in `bug_012`
	TODO
* Is it possible that QEMU has a bug?
	* QEMU and HP's behavior are different. Not sure which one is correct, or
	  maybe both are correct.

### `bug_012` `load IA32_EFER` problem

Tried to see the `load IA32_EFER` problem found in `bug_012` is fixed.
The idea is to revert `66265c823` and then revert `9b2327510`, but need to fix
git conflicts. After reverting and fixing a compile error, the git repo is
same as patching commit `da32a4e60` with `bug_012_rebase.diff`.

Unfortunately, still see `WARNING: WRMSR EFER EDX != 0` (Linux trying to WRMSR
`IA32_EFER` with edx != 0). Not going to fix for now.

## Fix

`565cef3cf..da32a4e60`
* Change TR access right initialization

