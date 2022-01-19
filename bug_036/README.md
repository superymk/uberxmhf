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

# tmp notes

TODO: what is "VMX-Preemption Timer" and "Monitor Trap Flag"? See i3 24.5
	Need to check whether supported in MSR
	<https://stackoverflow.com/questions/53771987/monitor-trap-flag-vm-exit-after-current-instruction>
TODO: reverse engineer the second 0xbb00 call (maybe read Windows Internals)
TODO: HP stucks at when APs are not awake. Can use APs to send NMI to CPU 0
TODO: Can consider reading debug commands from serial port
TODO: Reading about TXT, DRTM, SRTM

