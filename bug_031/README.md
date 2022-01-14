# XMHF cannot boot Windows 10 on QEMU due to triple fault

## Scope
* bad: x64 and x86 Windows 10, x64 and x86 XMHF, QEMU
* bad: Windows 10, HP (but another bug)

## Behavior
See `bug_027` "Behavior" section for logs

After selecting XMHF and then Windows 10, see triple fault.
x86 and x64 behavior are the same.
```
CPU(0x00): Unhandled intercept: 0x00000002
	CPU(0x00): EFLAGS=0x00000046
	SS:ESP =0xda00:0x000e97d4
	CS:EIP =0xf000:0x000f7d0b
	IDTR base:limit=0x000f615e:0x0000
	GDTR base:limit=0x000f6120:0x0037
```

For `CS:EIP` above, the result is when using `qemu-system-x86_64`. If using
`qemu-system-i386`, the result is `0x0008:0x000f7d0b`.

## Debugging

First debug x86 Windows 10 on x86 XMHF.

Using GDB, see that CR0 = 0x20, CR4 = 0x2000, `CS:EIP = 0x0008:0x000f7d0b`,

`0xf7d0a` is `hlt`. We need to know execution history. First print all
intercepts. Git is `b2fd96af9`, serial is `20220113140640`. Using
`grep Intercept results/20220113140640 | uniq -c`, can see that for intercept
18 (VMCALL, i.e. BIOS e820), the first 9 occurrences are from GRUB, then 162
occurrences are from Windows. However, the last occurrence is not e820? Likely
AMT did not print anything and returned, and then guest halted.
```
      1 CPU(0x00): Intercept 10 @ 0x0008:0x00009321 0x00000000
      9 CPU(0x00): Intercept 18 @ 0x0040:0x000000ac
      1 CPU(0x00): Intercept 10 @ 0x0008:0x00009ae7 0x00000001
      1 CPU(0x00): Intercept 10 @ 0x0008:0x00009af5 0x00000000
      1 CPU(0x00): Intercept 10 @ 0x0008:0x00009b36 0x00000000
      1 CPU(0x00): Intercept 10 @ 0x0008:0x00009b7b 0x00000000
     72 CPU(0x00): Intercept 10 @ 0x0008:0x00009a95 0x00000000
    163 CPU(0x00): Intercept 18 @ 0x0040:0x000000ac
      1 CPU(0x00): Intercept 2 @ 0x0008:0x000f7d0b
```

Now we need to be able to print EIP when guest calls E820.
`_vmx_int15_initializehook` shows how the VMCALL and IRET is set up. We just
need to read EIP from stack.

Git `1c99e1c2a`, serial `20220113145440`

Can see that the last interrupt is 0x2400, instead of 0xe820.
(good ref: <http://www.ctyme.com/intr/int-15.htm>)

For all the e820 intercepts, the `int $0x15` comes from `EIP = 0x9102`, and
then CS:EIP jumps to `0x40:0xac`, which is VMCALL. Then at `0x40:0xaf` perform
IRET, back to `EIP = 0x9104`. These are real mode code.

Since the EIPs are all the same, I think the 162 calls to E820 are also from
GRUB. For Windows 10 it is 9 + 162, for Linux it is 9 + 18, for Windows XP it
is also 9 + 162. Also for Windows XP there is a call to int15 EAX=0x2400:
`20220113145917`. So this is likely not the cause.

`-d int` option in QEMU does not help, because this option does not work in
KVM.

### Booting Windows 10 in QEMU, without XMHF, without PAE

Using `qemu-system-i386`,
* `-m 512M -cpu Haswell,vmx=yes --enable-kvm` works well (can completely boot).
* `-m 512M -cpu Haswell,vmx=yes` will reboot (same as triple fault behavior)
	* Using `-d cpu_reset` can see that there is a triple fault
	* Also helpful: `-no-reboot -no-shutdown`
	* But this triple fault it in protected mode with paging, EIP = 0x007e3a0a
* `-m 512M -cpu Haswell --enable-kvm` works well
* `-m 512M -cpu Haswell` will reboot (same as triple fault behavior)
* If remove the Linux disk (just boot Windows), successful

May need to analyze the difference between KVM and TCG's supported features.
There is a list of features printed at the start of booting.

KVM:
```
qemu-system-i386: warning: host doesn't support requested feature: CPUID.07H:EBX.hle [bit 4]
qemu-system-i386: warning: host doesn't support requested feature: CPUID.07H:EBX.rtm [bit 11]
qemu-system-i386: warning: host doesn't support requested feature: CPUID.80000001H:EDX.lm [bit 29]
```

TCG:
```
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.01H:ECX.fma [bit 12]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.01H:ECX.pcid [bit 17]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.01H:ECX.x2apic [bit 21]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.01H:ECX.tsc-deadline [bit 24]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.01H:ECX.avx [bit 28]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.01H:ECX.f16c [bit 29]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.07H:EBX.hle [bit 4]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.07H:EBX.avx2 [bit 5]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.07H:EBX.invpcid [bit 10]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.07H:EBX.rtm [bit 11]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.80000001H:EDX.syscall [bit 11]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.80000001H:EDX.lm [bit 29]
```

Difference:
```
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.01H:ECX.fma [bit 12]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.01H:ECX.pcid [bit 17]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.01H:ECX.x2apic [bit 21]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.01H:ECX.tsc-deadline [bit 24]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.01H:ECX.avx [bit 28]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.01H:ECX.f16c [bit 29]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.07H:EBX.avx2 [bit 5]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.07H:EBX.invpcid [bit 10]
qemu-system-i386: warning: TCG doesn't support requested feature: CPUID.80000001H:EDX.syscall [bit 11]
```

Experiments:
* `-m 512M --enable-kvm -cpu Haswell,x2apic=no`: can show Windows logo
* `-m 512M --enable-kvm -cpu Haswell,x2apic=no,avx=no,avx2=no,f16c=no,fma=no,pcid=no,tsc-deadline=no`: can show Windows logo
* `-m 512M --enable-kvm -cpu Haswell,x2apic=no,avx=no,avx2=no,f16c=no,fma=no,pcid=no,tsc-deadline=no,invpcid=no,syscall=no`: can show Windows logo, can boot
* `-m 512M -cpu Haswell,x2apic=no,avx=no,avx2=no,f16c=no,fma=no,pcid=no,tsc-deadline=no,invpcid=no,syscall=no`: triple fault

Looks like the problem is not related to features.

Even after running `update-grub` in Debian, still the same behavior.

This should be another bug with Windows 10 / grub / QEMU. Since the EIP of this
triple fault is different from XMHF's, ignore for now.

If remove `vmx=yes` in KVM's setting, can still boot Windows 10. Also checked
Windows Security, Hyper-V, and Device Guard. Looks like VMX related features
not enabled.

### Continue debugging

Set a break point at the int15 EAX=0x2400 intercept, and step through using
GDB's `si` command. See `202201131643gdb`

The instruction execution is basically looping:
```
   0x7d47:      pop    %cx
   0x7d45:      int    $0x13
   0xfa6f9:     rep insl (%dx),%es:(%di)
   0xfd3d9:     iret   
   0x9fbd9:     push   %ebp
   0x9fbe3:     iret   
   0x7d47:      pop    %cx
```

The normal execution sequence is:
```
   0x7d69:      ret    
   0x7cd2:      sub    %eax,%ecx
   0x7cd4:      ja     0x7cc5
   0x7cc5:      add    (%esi),%edx
   0x7cc9:      mov    %edx,%es
```

But the last iteration is:
```
   0x7d69:      ret    
   0x7cd2:      sub    %eax,%ecx
   0x7cd4:      ja     0x7cc5
   0x7cd6:      mov    $0x1acdbb00,%eax
   0x7cd9:      int    $0x1a
   0xf7d8b:     add    $0x24,%al
```

We should try to break at `0x7cd6` (`0x07c0:0x000000d6`). Also note that
`0x7c00` is the first sector of the first NTFS partition if booting from GRUB
(see `bug_033`), so we know what we are debugging.

The unassembly is in `real1.s` and `real1.s.16`.

# tmp notes

TODO: when booting Windows as the first disk, shouldn't the MBR sector be
the first sector?

hb arch/x86/vmx/peh-x86vmx-main.c:622
c

