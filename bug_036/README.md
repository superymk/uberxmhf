# HP cannot boot Windows 10

## Scope
* HP, x64 Windows 10, x86 XMHF
* Cannot testing x86 Windows 10 easily due to bootloader issues
* good: QEMU

## Behavior
See also `bug_027` "Behavior" section, especially `20220109205416`

When booting x64 Windows 10 with x64 QEMU on HP, see different behaviors

Before `bug_027`
* Exception #GP on CPU 4, exception #MC on CPU 0 (`20220109205416`, 1 time)
* No output on serial port after E820 intercepts (3 times)

Before `bug_036`
* No output on serial port after E820 intercepts (1 time)

## Debugging

One suspect is something related to TPM. Windows 10's use of TPM caused
`bug_031`. We may want to hide the TPM from the guest OS.

In git `f0293308f`, we print all intercepts. Can see that CPU 0 stucks after
some int 15h BIOS calls. Serial in `20220116215157`. However, for VMCALL, we
need to read the stack to see actual RIP.

Found a bug about using all bits of RSP in real mode. Should only use 16 bits.
Fixed in `7b6db45be`. In Intel volume 3 "19.1 REAL-ADDRESS MODE":
> A single, 16-bit-wide stack is provided for handling procedure calls and
  invocations of interrupt and exception handlers. This stack is contained in
  the stack segment identified with the SS register. The SP (stack pointer)
  register contains an offset into the stack segment.

Print CS:IP for caller to int 15h. Git `6cd8df3e8`, serial `20220117111803`.
Maybe we should print AX-DX.

Git is `eba84f4bf`, serial `20220117112953`. Not sure what `e80c` means.

### Use of BDA

Currently `_vmx_int15_initializehook()` uses BDA. However, I think the better
way is to use EBDA, similar to GRUB. In GRUB, if use `drivemap -s (hd0) (hd1)`
trice, can see that first 2 entries of E820 map changes from:
```
0x0000000000000000, size=0x000000000009fc00 (1)
0x000000000009fc00, size=0x0000000000000400 (2)
```
to:
```
0x0000000000000000, size=0x000000000009f7b0 (1)
0x000000000009f7b0, size=0x0000000000000850 (2)
```

Likely also need to change BDA `0x40:0x13` and `int $0x12`. Also looks like
GRUB changes `int $0x13`. However, likely low priority bug for now.

### Capture more BIOS calls

In git `8d3e47171`, captured INT 0x10 - 0x1e. QEMU serial in `20220117141508`,
HP serial in `20220117141531`. `INT=0x1c` are timer interrupts (can ignore).
Looks like after int 0x1a EAX=0xbb00 (`TCG_StatusCheck`), Windows also issues
`TCG_CompactHashLogExtendEvent` after seeing TPM is present.

Now we try to modify `TCG_StatusCheck` to return `TCG_PC_TPM_NOT_PRESENT`.
Git `891c35c82`, serial `20220117145712`. By comparing with `20220117141531`,
can see that there are more progress in HP, and then stucks. QEMU also stucks
after the fix, so the idea fix should be working. But now need to clean up
code.

Maybe the problem is caused by BDA address `0x4b4` (`40:b4`) is used. Moving it
to `0x4cc` (`40:cc`) seems to solve the problem in QEMU, but not HP. Git
`20a5f366d`.

Next steps:
1. Formalize the code to hide TCG
2. Try more debugging techniques

### Do not use BDA

<https://wiki.osdev.org/Memory_Map_(x86)> "External Links" has some references
of BDA structure. But some of them look conflicting. I think it is better to
follow GRUB's implementation and modify E820.

However, even if I move int1a handler to (remove int15 handler)
(git `edd016c5a`), still have this problem on HP. So not going to fix BDA for
now.

### Disable TPM

Is it possible that Windows uses BIOS to detect TPM, but afterwards it uses
another way to detect (ACPI? See
<https://qemu.readthedocs.io/en/latest/specs/tpm.html#acpi-interface>)? And
then using TPM causes the error of getting stuck.

We try to disable TPM in BIOS. Disable TXT, then set embedded security device
(TPM) as hidden.

Looks like the situation is the same. Linux now cannot detect TPM if started on
bare metal. The strange thing is that disabling TPM and reverting git to
`8d3e47171` will still cause HP to stuck early: `20220117160050`. Also git
`3b6be492b`, serial `20220117162026`.

Remember that `8d3e47171` (serial `20220117141531`) to `891c35c82` (serial
`20220117145712`) is just 1 commit that changes int 0x1a EAX=0xbb**. We can
confirm that change in XMHF code changes the situation, not change in BIOS.

|.|`8d3e47171`|`891c35c82`|
|-|-|-|
|Disable TPM in BIOS|bad|good|
|Enable TPM in BIOS|bad|good|

Where:
* bad: only 2 - 3 `0x1a` shows in serial output, screen still shows
  "Setting partition type to 0x7"
* good: a lot of `0x1a` shows in serial output, screen no longer shows
  "Setting partition type to 0x7"

The question now is, what is the behavior of this BIOS call when TPM is hidden?

We can use `a6b6fe20c` to test. HP result (disable TPM in BIOS):
```
p[0] = 0x00008600
p[1] = 0xbeefdead
p[2] = 0xbeefdead
p[3] = 0xbeefdead
p[4] = 0xbeefdead
p[5] = 0xbeefdead
p[6] = 0x00000000
p[7] = 0x00401f0f
```

HP result (enable TPM in BIOS):
```
p[0] = 0x00000000
p[1] = 0x41504354
p[2] = 0x00000102
p[3] = 0x00000000
p[4] = 0xbb3b1010
p[5] = 0xbb3b1298
p[6] = 0x00000000
p[7] = 0x00401f0f
```

QEMU result:
```
p[0] = 0x00000023
p[1] = 0xbeefdead
p[2] = 0xbeefdead
p[3] = 0xbeefdead
p[4] = 0xbeefdead
p[5] = 0xbeefdead
p[6] = 0x00000000
p[7] = 0x00401f0f
```

Not sure what the return value of 0x8600 means. The only clue in
"TCG PC Specific Implementation Specification" (see `bug_031`) is:
> Note that this cannot be guaranteed if (AH) = 86h because the call could be
  made on a non-TCG BIOS.

In an older version of this specification, meaning of AH is defined. Link
<https://trustedcomputinggroup.org/wp-content/uploads/PC-Client-Implementation-for-BIOS.pdf>:
> On return: (EAX)=Return code. If (AH) = 86h, the function is not supported by
  the system.
  All other register contents including upper words of 32-bit registers are
  preserved. Note that this cannot be guaranteed if (AH) = 86h because the call
  could be made on a pre-TCG BIOS.

It is strange to see that returning 0x8600 will still cause the problem, but
returning 0x23 will not. We should reverse engineer the caller of int 0x1a
EAX=0xbb00.

From previous log files, the first call to 0xbb00 is at `CS:IP=0x07c0:0x00db`.
If TPM is enabled, `0xbb07` will be called at `CS:IP=0x07c0:0x010d`. The disasm
for this is already acquired in `bug_031/real1.s.16`, see `7cdb`, `7d0b`. These
are in the first sector of NTFS.

The second call to 0xbb00 is at `CS:IP=0x07c0:0x1000`.

For the first 0xbb00 call, can see from `bug_031/real1.s.16` that the logic is:
```c
label_7cd6:
	EAX = 0xbb00
	int1a
	/* cx == 0x102 means TPM version 1.2x */
	if (eax && ebx == 0x41504354 && cx == 0x102) {
		EAX = %ss, 0xbb07
		ECX = %ss, 0x1152
		EDX = %ss, 0x0009
		EBX = EBX
		...
		ES = CS
		int1a
	}
label_7d0d:
	(other code)
```

We need to reverse engineer the second 0xbb00 call.

### Emulating TPM in QEMU

Since QEMU is easier to debug, we want to make sure Windows 10 can boot in QEMU
with TPM.

Ref: <https://tpm2-software.github.io/2020/10/19/TPM2-Device-Emulation-With-QEMU.html>

Install:
```sh
sudo dnf install swtpm swtpm-tools
```

Start TPM (can remove `--tpm2`):
```
mkdir /tmp/emulated_tpm
swtpm socket --tpmstate dir=/tmp/emulated_tpm --ctrl type=unixio,path=/tmp/emulated_tpm/swtpm-sock --log level=20 --tpm2
```

In QEMU command line:
```
-chardev socket,id=chrtpm,path=/tmp/emulated_tpm/swtpm-sock \
-tpmdev emulator,id=tpm0,chardev=chrtpm -device tpm-tis,tpmdev=tpm0
```

Debian shows:
```
$ sudo dmesg | grep tpm
[    3.626069] tpm_tis 00:06: 1.2 TPM (device-id 0x1, rev-id 1)
$ 
```

Test in QEMU with TPM 1.2
* x64 Debian, baremetal: can boot and identify TPM in dmesg
* x64 Debian, x64 XMHF: can boot and identify TPM in dmesg
* x64 Windows 10, baremetal: can boot and verify TPM in `tpm.msc`
* x64 Windows 10, x64 XMHF: can boot and verify TPM in `tpm.msc`

Still, for Windows 10 and QEMU only tested using BIOS with disabled SMM.

### Second 0xbb00 call

Git `a9fa84c67` serial `20220118201703` shows QEMU behavior when TPM is hided
by XMHF. The launch is paused manually. There are 4 0xbb00 calls in total.

Git `8bb72d55a` serial `20220118202526` shows QEMU behavior when XMHF does not
hide TPM. Serial `20220118202741` is HP behavior.

For QEMU, can see that 0xbb02 and 0xbb07 are also called. The sequence is:
`00, 07, 00, 07, 00, 02, 07, 00, 00, 02, 07`. For HP. After the second `00`
nothing happens: `00, 07, 00`.

We break at the second `00` call in QEMU and dump environment.

Can use GDB script with `waitamt`:
```
hb arch/x86_64/vmx/peh-x86_64vmx-main.c:300
c
c
d
```

The strange thing is, after hitting break point, cannot access XMHF space
variables like `vcpu` in CPU 0 (thread 1). If change to another thread, can
access. `info th` shows (notice "unavailable"):
```
(gdb) info th
  Id   Target Id                    Frame 
* 1    Thread 1.1 (CPU#0 [running]) _vmx_int1a_handleintercept (vcpu=<unavailable>, r=<unavailable>, OLD_AC=<unavailable>) at arch/x86_64/vmx/peh-x86_64vmx-main.c:300
  2    Thread 1.2 (CPU#1 [running]) 0x00000000102089f8 in xmhf_smpguest_arch_initialize (vcpu=0x10248f4c <g_vcpubuffers+5932>) at arch/x86_64/smpg-x86_64.c:86
  3    Thread 1.3 (CPU#2 [running]) 0x00000000102089ff in xmhf_smpguest_arch_initialize (vcpu=0x1024a678 <g_vcpubuffers+11864>) at arch/x86_64/smpg-x86_64.c:86
  4    Thread 1.4 (CPU#3 [running]) 0x00000000102089f8 in xmhf_smpguest_arch_initialize (vcpu=0x1024bda4 <g_vcpubuffers+17796>) at arch/x86_64/smpg-x86_64.c:86
(gdb) 
```

For some reason, this happens exactly when `int 1ah 0xbb??` calls take place?
We can ignore this for now, just jump back to guest mode and then can access
memory.

Updated git: `b8616fad7`
Updated gdb script:
```
d
hb arch/x86_64/vmx/peh-x86_64vmx-main.c:300
c
c
d
hb *0x1000
hb *0x1000 + 0x07c0 * 16
c
```

Can see at this point that `0x8c00 - 2 = 0x8bfe` is `int $0x1a`. We are now at
`0x8c00`. Need to dump memory to run objdump for 16-bit mode.

From CS=0x7c00, we guess that code starts from 0x7c00. Also looks like code
ends at 0x8f10 (starting there, a lot of 0x0). So dump from 0x7c00 to 0x9000,
file is `dump1.img`. Use
`objdump --adjust-vma=0x7c00 -b binary -m i8086 -D dump1.img` to disasm.
Can also remove `--adjust-vma=0x7c00` to show address seen in GDB.

While reverse engineering, looks like real mode memory access is using `%ds`.
For example `mov    0x28(%edx),%ecx` accesses `$edx + 0x28 + $ds * 16`.

Some annotation can be found in `dump1.s`. But for now it is more helpful to
implement single step using XMHF.

### Using Hypervisor Features to debug Guest OS

Ref:
<https://stackoverflow.com/questions/53771987/monitor-trap-flag-vm-exit-after-current-instruction>

* "24.5.1 VMX-Preemption Timer": Check “activate VMX-preemption timer”
  VM-execution control
* "24.5.2 Monitor Trap Flag": Check “monitor trap flag” VM-execution control

VM-execution control are defined in "23.6"
* Monitor trap flag is bit 27 of Primary Processor-Based VM-Execution Controls
* Activate VMX-preemption timer is bit 6 of Pin-Based VM-Execution Controls

After looking up previous logs, looks like the same for QEMU and HP
```
rdmsr(IA32_VMX_PINBASED_CTLS_MSR)  = 0x0000007f00000016
rdmsr(IA32_VMX_PROCBASED_CTLS_MSR) = 0xfff9fffe0401e172
rdmsr(IA32_VMX_MISC_MSR)           = 0x0000000020000065
```

We can see that both features should be supported. So let's try them.

After setting "monitor trap flag", will get intercept 37. The intercept handler
does not need to do anything. Implemented in git `a0e783305`. However the
execution will be very slow. HP and QEMU can run this.

To use the preemption timer, need to add the "VMX-preemption timer value" VMCS
field (Encoding = 0x482E). According to "A.6 MISCELLANEOUS DATA" and the value
of `IA32_VMX_MISC_MSR`, 1 in preemption timer is `(1 << 5) = 32` in timestamp
counter (TSC). Code in git `70aaa7ff2`. The preemption timer fires frequently
in QEMU. HP also works well.

During testing preemption timer, see the #MC behavior once in HP:
serial `20220119205342`. Normal execution on HP: serial `20220119205909`.
Both these exceptions seem to happen at the same place. Also note that after
the machine halts, the preemption timer no longer works.

Looks like at this point, the monitor trap is more helpful.

In git `d9c022cc9`, enable monitor trap later. Serial is `20220119211911`.
We can still see the last non-monitor-trap intercept at `CS:IP=0xf000:0xe211`
at line 4105. Looks like this call returns to user mode soon at
`0xf000:0x0000fea7 -> 0x07c0:0x00001000`. Then there's soon the dead loop.

`0xf000:0x0000fea8` looks like BIOS' timer interrupt. We should remove it.
The block to be removed is in `results/20220119211911_timer{,2,3}`. Use
the following shell script to remove timer interrupts.

```
PATTERN="`tr $'\n' '/' < results/20220119211911_timer`"
PATTERN2="`tr $'\n' '/' < results/20220119211911_timer2`"
PATTERN3="`tr $'\n' '/' < results/20220119211911_timer3`"
tail -n +4106 results/20220119211911 | dos2unix | tr $'\n' '/' | \
	sed "s|$PATTERN||g" | sed "s|$PATTERN2||g" | sed "s|$PATTERN3||g" | \
	tr '/' $'\n' | grep '0x07c0:0x00001000' -A 10000000
```

Can see that the infinite loop happens when `0x1085` calls `0xe83`. Then this
function never returns (e.g. `ea7` cannot be found).

We now have a good instruction flow. Next steps are:
* Write better program to remove timer code
* Print registers
* Compare with QEMU instruction flow
* Read "Windows Internals"

We try to run `d9c022cc9` on QEMU. After enabling the monitor trap looks like
the CPU enters the infinite loop. The two infinite loops should be similar.

### Source of bootstrap code

By dumping the first NTFS partition from QEMU, can see that `dump1.img` is
basically the same as first 0x1400 bytes of this NTFS partition. The difference
are in 0x220 - 0x260 and 0x280 - 0x2a0. Diff command used:
```diff
$ diff <(hexdump dump1.img -C) <(sudo hexdump -C /dev/nbd0p1 | head -n 307)
1,2c1,2
< 00000000  eb 52 90 4e 54 46 53 20  20 20 20 00 02 08 80 20  |.R.NTFS    .... |
< 00000010  00 b8 5a 00 00 f8 00 00  3f 00 ff 00 00 08 00 00  |..Z.....?.......|
---
> 00000000  eb 52 90 4e 54 46 53 20  20 20 20 00 02 08 00 00  |.R.NTFS    .....|
> 00000010  00 00 00 00 00 f8 00 00  3f 00 ff 00 00 08 00 00  |........?.......|
35,38c35,37
< 00000220  00 00 01 00 00 00 08 2c  00 00 08 30 00 00 08 34  |.......,...0...4|
< 00000230  00 00 e8 2d 00 00 40 32  00 00 90 36 00 00 08 38  |...-..@2...6...8|
< 00000240  00 00 00 00 00 00 08 3c  00 00 08 4c 00 00 08 2c  |.......<...L...,|
< 00000250  00 00 00 10 00 00 02 00  00 00 05 00 4e 00 54 00  |............N.T.|
---
> 00000220  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
> *
> 00000250  00 00 00 00 00 00 e9 c0  00 90 05 00 4e 00 54 00  |............N.T.|
41,42c40,41
< 00000280  4e 00 58 00 54 00 00 04  00 00 00 28 00 00 00 10  |N.X.T......(....|
< 00000290  00 00 01 00 00 00 08 00  00 00 0d 0a 41 6e 20 6f  |............An o|
---
> 00000280  4e 00 58 00 54 00 00 00  00 00 00 00 00 00 00 00  |N.X.T...........|
> 00000290  00 00 00 00 00 00 00 00  00 00 0d 0a 41 6e 20 6f  |............An o|
308c307
< 00001400
---
> 00002000  46 49 4c 45 30 00 03 00  51 15 10 00 00 00 00 00  |FILE0...Q.......|
$ 
```

This region is the same between HP and QEMU's first partition, except
0x48 - 0x50 (Volume Serial Number, see <https://en.wikipedia.org/wiki/NTFS>).

### Loop instruction

Now we need to understand how QEMU exits the `loop` instruction at `ea0`.

We can use `loop2.gdb` to see registers before executing the `loop`
instruction. GDB results are in `20220120120459gdb` and `20220120121211gdb`.
Can see that result across multiple runs are deterministic. The loop
instruction exits when entered with ECX = 1. This follows Intel's manual.

In this program, RCX starts from 32768 (0x8000). Then `-= 63`, then `-= 64`
forever until reaching 1. We should print at least ECX at e9d.
Also, in QEMU, RSI is `[0x3808] + [0x2060] * 512`, RDI increases from 0 to
0x8000, with increment of 64.

Can also see that the increment of 64 is defined by `$0x2060` and `$0x2020`
in this function:
```
     e83:	60                   	pusha  
     e84:	8b 36 18 20          	mov    0x2018,%si
     e88:	26 8a 05             	mov    %es:(%di),%al
     e8b:	88 04                	mov    %al,(%si)
     e8d:	47                   	inc    %di
     e8e:	46                   	inc    %si
     e8f:	66 ff 06 14 20       	incl   0x2014
     e94:	81 fe 60 20          	cmp    $0x2060,%si
     e98:	75 06                	jne    0xea0
     e9a:	e8 5b 00             	call   0xef8
     e9d:	be 20 20             	mov    $0x2020,%si
     ea0:	e2 e6                	loop   0xe88
     ea2:	89 36 18 20          	mov    %si,0x2018
     ea6:	61                   	popa   
     ea7:	c3                   	ret    
```

So in this function:
* CS = DS = 0x07c0
* ES = 0x2000
* CX = number of bytes remaining
* DI = number of bytes processed
* ES:DI = location to hash?
* SI: number of bytes in current block - 0x2020
* DS:SI = location to store current block (0x2020 - 0x2060)

However, debugging with monitor trap on is very slow.
* On QEMU, takes around 6 mins to process 0x1000 bytes

Should think of another way to set breakpoints. Should be able to use INT3
instruction and exception bitmap, see Intel volume 3
"24.2 OTHER CAUSES OF VM EXITS"

It looks like there is one possibility:
* VMX preemption timer does not work because the CPU halts
* Monitor trap looks like infinite loop because it is very slow
* Why does the CPU halt? Maybe the hash verification failed.
* However, I do not see a lot of `hlt` instructions in `dump1.s`

If we are willing to wait, we can see full HP logs with only monitor trap.
Serial `20220120223019.xz` (compressed 3.0M, original 189M). We can see that
`e83` is called from `1085` two times. It now becomes necessary to be able to
set break points, or debugging consumes too much time and memory (for log).

### INT3 break points

Intel volume 3 "6.3.1 External Interrupts" says INT3 will generate exception 3.
"23.6.3 Exception Bitmap" and "24.2 OTHER CAUSES OF VM EXITS" says can change
exception bitmap to intercept this exception. By testing, in QEMU currently
nothing will cause exception 3.

In git `aebb61037`, some breakpoint function is supported. QEMU serial in
`20220121134648`. We can see that `e83` is called 13 times. In the first 12
times, ECX = 0x8000. In the last time, ECX = 0x5156. After that, `ea8` is
called,
and returns at `ef7`. Then `int    $0x1a` is called in `10c0`. Looks like
the problem with HP happens during this process. `ea8` contains the `inc`
instruction at `eb2` (my guess is communication to TPM), which may cause the
problem in HP.

Two runs on HP are `20220121143120` and `20220121143221`. Can see that the
problem happens when `e83` is called. The problem is not deterministic. In
`20220121143120`, looks like the problem happens during hypervisor mode.

In `92c3d966d`, we halt all APs to make sure APs are not causing troubles. We
also remove printf lock to make sure deadlock does not block error messages.
Since only the BSP is running, most messages are still readable.

HP run resuls are `20220121145944` and `20220121150028` and `20220121150126`.
Can see that this time CPU 0 always receives the #MC exception. We believe that
in previous executions the exception is not printed because of deadlock on
printf.

Things to consider:
* Is it related to other CPUs? In previous runs CPU 4 will print #GP.
* Is it an exception or an interrupt?
* How do we know the source of this exception / interrupt?
	* Read MSRs (ref: <http://www.ctyme.com/intr/rb-0599.htm>)
* Can we disable maskable interrupts in hypervisor code?
* If force return `TCG_PC_TPM_NOT_PRESENT` in `TCG_StatusCheck`, what will be
  the problem? (see `8d3e47171..891c35c82`)
* Is it possible that ignoring is the correct choice?
	* Read BIOS' handler for this interrupt
* May it be related to `<unavailable>` in QEMU GDB?

Previous ideas
* What is being hashed?
	* Now we know the problem is likely related to Hypervisor, so 
* Test more on preemption timer
	* We already know that CPU stucks in hypervisor mode, so no need to test
	  now.

### Disable other CPUs

If we do not initialize other CPUs in XMHF, git in `76e18929e`. This is not a
clean hack, though. HP receives another error: Intercept 3 at
`0xf000:0x0000fea8`. This is the address of the suspected timer BIOS call.
Serial `20220122221525` and `20220122221631`.

We check the IVT. Git `eb621f1f1`, serial `20220122223951`. Can see that
`0xfea8` is indeed timer interrupt `08h`. At this point we should stick to
enabling other CPUs (probably halt them).

### Mask interrupts in hypervisor code

The possible causes of execution of interrupt/exception handler 18 are:
* Interrupt (Intel v3 "6.3 SOURCES OF INTERRUPTS")
	* External (hardware generated) interrupts.
	* Software-generated interrupts.
* Exception (Intel v3 "6.4 SOURCES OF EXCEPTIONS")

We now rule out the possibility of interrupt:
* It cannot be software interrupt, because we are running XMHF code, and we
  are not issueing software interrupts.
* It cannot be NMI, because NMI only happens at vector 2.
* Note that during the exception, RFLAGS = 0x2, so interrupts are disabled.

So we believe this is a legitimate machine check exception. Should read Intel
v3 "6.4.3 Machine-Check Exceptions"

For example, in `20220122223951`, RIP stops at `0x10214e30`. At exception,
AX=0x5000, DX=0x5085. This is after the `in` instruction. Is it possible that
Windows / TPM also uses this port, which causes the trouble?

```
0000000010214e1f <inb>:
        static inline unsigned char inb (u32 port){ 
    10214e1f:   55                      push   %rbp
    10214e20:   48 89 e5                mov    %rsp,%rbp
    10214e23:   48 83 ec 18             sub    $0x18,%rsp
    10214e27:   89 7d ec                mov    %edi,-0x14(%rbp)
          __asm__ __volatile__ ("inb %w1,%0":"=a" (_v):"Nd" ((u16)port));
    10214e2a:   8b 45 ec                mov    -0x14(%rbp),%eax
    10214e2d:   89 c2                   mov    %eax,%edx
    10214e2f:   ec                      in     (%dx),%al
    10214e30:   88 45 ff                mov    %al,-0x1(%rbp)
          return _v;
    10214e33:   0f b6 45 ff             movzbl -0x1(%rbp),%eax
        }
    10214e37:   c9                      leave  
    10214e38:   c3                      ret    
```

### Stack Discipline

When working with current optimization level, looks like each function starts
with `push   %rbp` and ends with `leave; ret`. So we can reconstruct the stack
without too many debug info. Use `read_stack.py`. We can also use `nm` to get
a symbol table easily, and resolve addresses to function + offset.

In `20220122223951`, the stack trace is:
```
10214e57 <dbg_x86_64_uart_putc_bare+30>
10214e97 <dbg_x86_64_uart_putc+29>
10214de3 <xmhf_debug_arch_putc+24>
10246fc3 <emhfc_putchar+28>
1022e5a0 <kvprintf+229>
1022e1b0 <vprintf+67>
1022e165 <printf+74>
10208576 <xmhf_parteventhub_arch_x86_64vmx_intercept_handler+249>
10206a5b <xmhf_parteventhub_arch_x86_64vmx_entry+40>
```

It looks like the first printf call in the intercept handler and the first inb
call on the port. Note that in the exception handler, printf works as normal.
So it is more likely that this #MC exception can be ignored, at least in
hypervisor space.

In `4b7e442f8`, we can see that the intercept causing the #MC exception is
actually intercept 3 at `0xf000:0x0000fea8`. From above RIP is probably a BIOS
call. Intercept 3 is INIT signal. Is it possible that TPM operation sets a
timer, and sends INIT when timer expires? Also, what is the default action of
the #MC exception in BIOS? It is possible to issue an interprocessor interrupt
of INIT to self.

This means we may need to reverse engineer the BIOS.

### Machine Check Exception
In git `cf3af8bc5`, serial `20220123122404`, we print #MC related MSRs.

After reading Intel v3 "CHAPTER 15 MACHINE-CHECK ARCHITECTURE", we need to find
`IA32_MCi_STATUS` with valid bit (MSB) set. Looks like it is `IA32_MC6_STATUS`:
`CPU(0x00): MSR[0x0419] = 0x be000000 0100014a`

In git `26d10cbe6`, serial `20220123123758`, print more MSRs just in case we
miss something. However, looks like another CPU also has an exception, and
output is corrupted. We add printf lock back and only release lock at beginning
of exception handler. Git `85531f626`, serial `20220123124425`.

This time, #MC happens during `spin_lock()`. Now my guess is that the #MC
happens because hardware detects that CPU 0 is still running after INIT signal
is received after some time. When printf does not have lock, `inb` instruction
is slow. When printf has lock, the spin lock is slow. For CPU with ID=4, it
receives #GP when it is halting (strange).

The interesting MSRs are:
```
CPU(0x00): MSR[0x0179] = 0x 00000000 00000c09	IA32_MCG_CAP
CPU(0x00): MSR[0x017a] = 0x 00000000 00000004	IA32_MCG_STATUS
CPU(0x00): MSR[0x0418] = 0x 00000000 000001ff	IA32_MC6_CTL
CPU(0x00): MSR[0x0419] = 0x be000000 0100014a	IA32_MC6_STATUS
CPU(0x00): MSR[0x041a] = 0x 00000000 000f8080	IA32_MC6_ADDR
CPU(0x00): MSR[0x041b] = 0x 00000000 a806202c	IA32_MC6_MISC
```

Use Intel v3 `15.3.2.2 IA32_MCi_STATUS MSRS`,
`IA32_MC6_STATUS` shows that MSCOD Model Specific Error Code = 0x0100 = 256,
MCA Error Code = 0x014a = 330. The `DisplayFamily_DisplayModel` of HP's CPU
should be `06_25h`.

We then look up 0x014a in "15.9 INTERPRETING THE MCA ERROR CODES". In
"15.9.2 Compound Error Codes" Table 15-10 it should be Cache Hierarchy Errors.
```
000F 0001 RRRR TTLL
0000 0001 0100 1010
F = 0
TT = G (Generic)
LL = L2 (Level 2)
RRRR = DWR (data write)
{TT}CACHE{LL}_{RRRR}_ERR -> GCACHEL2_DWR_ERR
```

### WBINVD

Git `017ad11f3` removes some breakpoints. We sometimes see two CPUs have
exceptions, and output is corrupted. See serial `20220123165340`

Git `b54a6de13` adds some wbinvd instructions to flush the cache, hoping to fix
the MC exception. Serial `20220123170727`, looks like the error changes. The
exception happens immediately after the WBINVD instruction. MSRs now become:
```
CPU(0x00): MSR[0x0418] = 0x 00000000 000001ff
CPU(0x00): MSR[0x0419] = 0x f2000000 80000106
CPU(0x00): MSR[0x041a] = 0x 00000000 00037cca
CPU(0x00): MSR[0x041b] = 0x 00000000 a8000011

000F 0001 RRRR TTLL
0000 0001 0000 0110
F = 0
TT = D (Data)
LL = L2
RRRR = ERR (Generic Error)
```

An interesting thing is that in git `017ad11f3`, CPU 0x04 will always have an
exception. Sometimes this exception and CPU 0x00's exception corrupt the serial
output, sometimes the serial output is good. In git `b54a6de13`, only CPU 0x00
has exception.

I think for now should focus on the INIT signal. Maybe #MC is a by product of
it. Another way is to try a different hardware (try VGA).

Previous ideas
* TODO: Is it possible that ignoring is the correct choice? Read BIOS' handler
  for this interrupt
	* Looks like the #MC exception is for the hypervisor. Ignoring is helpful
	  in confirming the (likely) root cause of Intercept 3, but cannot be an
	  actual fix.

### Other TPM configurations

The 0x1a calls for returning 0x0 (i.e. TPM available) are:
```
CS:IP=0x07c0:0x00db AX=0xbb00
CS:IP=0x07c0:0x010d AX=0xbb07
CS:IP=0x07c0:0x1000 AX=0xbb00
```

Git `fca83685a` sets TPM to be not present using XMHF. Serial in
`20220123172941` and `20220123173037`. Same as previous experiments, looks like
the CPU goes to protected mode. We now see that the exception is still
Intercept 3. An interesting thing is that both tries halt at
`0x0020:0x0048b213`.

The 0x1a calls for returning 0x23 (`TCG_PC_TPM_NOT_PRESENT`) are:
```
CS:IP=0x07c0:0x00db AX=0xbb00
CS:IP=0x07c0:0x1000 AX=0xbb00
CS:IP=0x2000:0x0a39 AX=0x0000 * 23
CS:IP=0x2000:0x0a39 AX=0x0200
CS:IP=0x2000:0x0a39 AX=0x0400
CS:IP=0x2000:0x0a39 AX=0xbb00
CS:IP=0x2000:0x0a39 AX=0x0200
CS:IP=0x2000:0x0a39 AX=0x0400
CS:IP=0x2000:0x0a39 AX=0x0000 * 3
```

If we hide TPM in BIOS, serial `20220123185700` (git `da0a5d51f`). The first
INIT signal happens at another place. Looks like this is different from
returning 0x0. We can use git `ea8ce7380` to set monitor trap, see serial
`20220123190706`. We can see that the CPU leaves CS=0x7c quickly.

So in the following 3 combinations, the CPU receives INIT signal.
* Normal (enable TPM)
* Hide TPM in XMHF using 0x23
* Hide TPM in BIOS using 0x8600

So now I think the INIT signal may be delivered based on some timing, instead
of related to TPM. Now we want to modify Windows' bootloader and make an
infinite loop.

### Infinite loop

Git `be5792b50` implements infinite loop. Serial in `20220124205200`. Can see
that ECX increases as expected, which means that we are running the infinite
loop. But we do not receive the init signal.

Between the two int1a calls, 0x1085 or 0x1097 calls 0xe83. For each 0x40 bytes
0xe83 calls 0xef8. In 0xf39 the `call   *(%bx)` calls 0xfa8.

Now we make the call to 0xef8 NOP, and see whether INIT will still be received.
Git `e73b930a9`, serial `20220124212907`, we can see that INIT is received.
Now our guess is that the problem may be related to accessing the memory to be
hashed (i.e. `%es:(%di)`).

Git `12bd43ede`, serial `20220124214647`; git `0dc578589` serial
`20220124215309`. We can see that each time ES increases by 0x800, and the data
to hash is 0x8000 bytes. The first call is `0x2000:0x0000` to `0x2000:0x8000`.
The second call is `0x2800:0x0000` to `0x2800:0x8000`. Etc. The total number of
bytes to hash is `0x65156` (ECX in `0x07c0:0x106e`). We can now
1. Dump the content of this memory
2. Simulate this memory access and see what causes the INIT signal
3. Set breakpoint at bios handlers (search for `0xf0002aef` in serial)

### Bios handlers

In results folder, run
`grep -h -E '^\*0x.... = 0x........' 20220124* | sort | uniq -c`, can see that
the IVT handlers are always the same (this is expected).

We now dump the bytes to hash (size = `0x65156`) and BIOS memory. Looks like
the thing hashed is `/bootmgr` in the boot MBR partition, due to size.

We dump the BIOS memory and bootmgr memory.
Git `84e173465`, QEMU serial `20220125112609`, HP serial `20220125112611`.
Looks like when HP tries to access bootmgr memory in Hypervisor mode, exception
happens.

HP Bios image in `hpbios4.img` and `hpbios4.s`. Generated using:
```sh
grep -A 4096 'Start dump BIOS' results/20220125112611 | tail -n +2 | cut -b 1-9,11- | xxd -r | dd bs=1K skip=960 > hpbios4.img
objdump -b binary -m i8086 -D hpbios4.img > hpbios4.s
```

Dump bootmgr from QEMU
```sh
grep -A 25878 'Start dump bootmgr' 20220125112609 | tail -n +2 | cut -b 1-9,11- | xxd -r | dd bs=1K skip=128
```

We can compare the above output with the `bootmgr` file. They are exactly the
same. The `bootmgr` file is also the same between HP and QEMU. Attached as
`bootmgr5.bin`

If we compare dumping bootmgr between HP and QEMU, we can see that at first
HP dumps successfully.
```sh
diff <(grep -A 2050 'Start dump bootmgr' 20220125112609) \
     <(grep -A 2050 'Start dump bootmgr' 20220125112611)
```

But then when reading 0x28000, some exception happens (likely #GP). The
exception output is cluttered because multiple exceptions are printing at the
same time. Also note that there are a lot of 0s from 0x27c00 to 0x28840.

Another run of git `84e173465` is HP serial `20220125121438`. The BIOS area
is almost the same, except 1 bit at 0xffa12:
```diff
4003c4003
< 000ffa10:  61c3 0c00 5053 9cfa e461 8ae0 e461 32c4
---
> 000ffa10:  61c3 0b00 5053 9cfa e461 8ae0 e461 32c4
```

This time the exception happens when reading 0x48000. The exception output
is still cluttered. From now on we mute all APs when an exception happens.

Git `b75dd49dc`, serial `20220125122531`. This time exception happens at
0x40000. The exception is #MC, happens during `inb` instruction. Now our guess
to the problem is:

Windows' bootmgr is loaded into multiple segments of 0x8000 bytes. When the
hypervisor reads the start of a segment, an exception (likely #MC) may happen,
asynchronously. For example when the hypervisor executes `inb` instruction
(which is slow), the exception fires.

The next step is to figure out how bootmgr is loaded to memory.

Also, git `13bbb8656` only reads head of segments. This makes reproducing this
problem faster. Serial `20220125123450`.

In git `3d6f88db3`, we read somewhere near the head. This time, no error
happens at that time. Serial `20220125123832`.

Git `cf7aa56a5`, serial `20220125124241`. The error happens at
`n * 0x8000 + 0x10`.

Git `2bc33b682`, serial `20220125124538`, `20220125124753`, `20220125125039`.
For some reason the exceptions happen at unexpected places. Then cut power of
HP and restart (need to reconfigure AMT). Serial `20220125130246`. This time
the read access error happens at 0x40bc0.

We also try to use WBINVD in `0418ed19b`. Serial `20220125130924` and
`20220125131041`. The WBINVD instruction causes the #MC. This may be a good
way to test whether the cache is corrupted.

### BIOS handler breakpoints

Git `ce6e0399e`, serial `20220126102626`. We tried to set breakpoints at all
BIOS interrupt handlers, except the timer handler. Looks like none of the break
points hit. So we think that the INIT signal is not generated by BIOS calling
APIC etc.

### Review memory type

When searching `"wbinvd" "machine check exception"` on Google, see this, may
be very related:
<https://community.intel.com/t5/Software-Archive/EPT-write-back-memory-type-and-Machine-Check-exception/td-p/766564>
(Archive: <https://web.archive.org/web/20220125181357/https://community.intel.com/t5/Software-Archive/EPT-write-back-memory-type-and-Machine-Check-exception/td-p/766564>)

This looks like a possible cause. MTRRs are documented in
"11.11 MEMORY TYPE RANGE REGISTERS (MTRRS)". EPT memory type is documented in
"27.2.7.2 Memory Type Used for Translated Guest-Physical Addresses".

XMHF uses `_vmx_getmemorytypeforphysicalpage()` to read MTRRs and assign EPT
memory types. However, the algorithm looks suspicious.

We use `a62e6a79b` and `429d0dcac` (in `xmhf64` branch) to make reading fixed
MTRR code cleaner.

In `88583ea6e`, we want to print out MTRRs, but looks like QEMU cannot boot
now. This is a regression, we need to bisect
```
git bisect start
git bisect bad 88583ea6e (good after make clean)
git bisect bad 11a4b4ce0 (good after make clean)
git bisect good ea8ce7380
git bisect bad cf7aa56a5 (good after make clean)
git bisect bad 0dc578589 (should be good)
git bisect bad be5792b50 (should be good)
git bisect bad 852162a6e (should be good)
```

So the regression is just fixed by `make clean`. We continue working on
`88583ea6e`. QEMU serial until GRUB is `20220126142552`. QEMU MTRRs are:
```
0x00000000 - 0x000a0000 : 6 (WB)
0x000a0000 - 0x000c0000 : 0 (UC)
0x000c0000 - 0x00100000 : 5 (WP)
```

HP serial in `20220126200728`. HP MTRRs are the same. Currently the logic in
`_vmx_setupEPT()` is basically: `EPT_type = (MTRR_type == 0) ? 0 : 6`. The
comment mentions that setting EPT type to 6 is for
`present, WB, track host MTRR`.

The MTRR types are documented in
"Table 11-8. Memory Types That Can Be Encoded in MTRRs" on page 447.
EPT memory types are defined in
"27.2.7.2 Memory Type Used for Translated Guest-Physical Addresses" on page
1023: `0 = UC; 1 = WC; 4 = WT; 5 = WP; and 6 = WB`.

A comment in
<https://community.intel.com/t5/Software-Archive/EPT-write-back-memory-type-and-Machine-Check-exception/td-p/766564>
mentions that MTRRs are not used when EPT is used. Looks like this is the case,
so the current XMHF logic is wrong. In Intel volume 3
"27.2.7.2 Memory Type Used for Translated Guest-Physical Addresses":
> The MTRRs have no effect on the memory type used for an access to a
  guest-physical address.

Other resources: searching for `ept mtrr` in Google:
* <http://blog.leanote.com/post/only_the_brave/%E8%A7%A3%E8%AF%BBMMU>
	* <https://revers.engineering/mmu-ept-technical-details/>

Committed `429d0dcac..835d832f6` related to MTRR. However the #MC problem in HP
still exists.

Git `2849f5c9f`, serial is `20220126213715`.

Git `54d4b8dd2`, serial `20220126215127`.

Git `4edda0a4b`, serial `20220126215340`. This is the same behavior as if
MTRR related commits were not made. QEMU still has the `<unavailable>` problem
in this commit.

Previous ideas
* Reverse engineer the second 0xbb00 call (maybe read Windows Internals)
	* Current analysis shows that the problem is closer to hardware, not Windows
* HP stucks at when APs are not awake. Can use APs to send NMI to CPU 0
	* Now we already have monitor trap and memory breakpoints to debug the guest
* Can consider reading debug commands from serial port
	* Likely too complicated to implement. Too overkill for this bug

### Testing on other hardware

Since this bug is very low level, we can try on machines that do not support
serial port by using `DEBUG_VGA` as output. When running `./configure`,
add `--disable-debug-serial --enable-debug-vga`.

But since VGA does not support scrolling, need to quiet some XMHF output.

VGA on QEMU and HP works as expected. So now we can try XMHF on more computers
without configuring serial port.

### How is bootmgr loaded into memory

Since this bug happens when we are trying to access bootmgr, we should know
how it is loaded into memory. Maybe something is wrong with it.

Git `c28d0fa3b` HP serial `20220127105928` shows how the second sector and
above of NTFS are loaded into memory. Before load, `*8000=0139e8811bbe5652`.
After load, `*8000=8ea166028e0e8966`. The change happens in BIOS code with
`CS=0xf000`. NTFS first sector code that calls BIOS is `0x07c0:0x0145`. This
code is called a few times. This is BIOS int 13h with AH=0x42. Refs:
* <http://www.ctyme.com/intr/rb-0708.htm>
* <https://wiki.osdev.org/Disk_access_using_the_BIOS_(INT_13h)>

# tmp notes

TODO: how is bootmgr loaded into memory?
TODO: test on more hardware using VGA
TODO: May it be related to `<unavailable>` in QEMU GDB?

## Commits

`43b3d6552..835d832f6`
* Fix bug of interpreting RSP in real mode
* Reduce code used to read fixed MTRRs
* Remove hardcoding of memorytype=WB in `_vmx_setupEPT()`
* Block guest's changes to MTRR

