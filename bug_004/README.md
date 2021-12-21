# QEMU Debian 11 unhandled exception d

## Scope
* At least x86 and x86-64, Debian 11, QEMU

## Behavior
In QEMU, boot XMHF or XMHF64, then Debian 11 Linux 5.10.0-10-686. After screen
change, see unhandled exception d in XMHF

## Debugging

First try to break in gdb
```
# Start from gdb/x86_vm_entry.gdb
b xmhf_xcphandler_arch_hub if vector == 0xd
```

When using break points, QEMU has a problem
```
qemu-system-i386: ../target/i386/kvm/kvm.c:645: kvm_queue_exception: Assertion `!env->exception_has_payload' failed.
```

So, connect the gdb after CPU halts by `unhandled exception d`. Then load
symbol file
```
symbol-file xmhf/src/xmhf-core/xmhf-runtime/runtime.exe
```

EIP is at `nop` instruction after `hlt` in `xmhf_xcphandler_arch_hub`.
Can print `vector`, etc. Serial port shows:
```
[00]: unhandled exception d, halting!
[00]: state dump follows...
[00] CS:EIP 0x0008:0x10206c1c with EFLAGS=0x00010006
[00]: VCPU at 0x10244aa0
[00] EAX=0x0000064d EBX=0xd9b5fe58 ECX=0x0000064d EDX=0xc143d620
[00] ESI=0x10244aa0 EDI=0xd9b5fe7c EBP=0x1994af1c ESP=0x1994af0c
[00] CS=0x0008, DS=0x0010, ES=0x0010, SS=0x0010
[00] FS=0x0010, GS=0x0010
[00] TR=0x0018
```

EIP is at `0x10206c1c <_vmx_handle_intercept_rdmsr+145>`, this is the default
branch. From AMT term output we see ecx, so it is trying to read MSR `0x64d`
`MSR_PLATFORM_ENERGY_COUNTER`

Looks like this MSR is not supported. In QEMU I use Haswell, but the MSR is in
6th Generation Core(TM) Processors and above.

Intel volume 4 shows that cpuid(eax=1) can display CPU
`DisplayFamily_DisplayModel` information. In QEMU it is `06_3CH`, which is
correct.

Can see that the PC in Linux is 0xc143d515
```
(gdb) p/x vcpu->vmcs.guest_RIP
$7 = 0xc143d515
(gdb) 
```

Guess is that Debian blindly calls RDMSR, and catches the exception using IDT

### Analyzing Debian's Behavior

The PC in Linux corresponds to `__rdmsr_safe_on_cpu+21`
```
(gdb) p (void*) 0xc143d515
$9 = (void *) 0xc143d515 <__rdmsr_safe_on_cpu+21>
(gdb) 
```

<https://elixir.bootlin.com/linux/v5.10.84/source/arch/x86/lib/msr-smp.c>

Looks like my guess is correct

Try to debug Debian on QEMU, without XMHF. See that if `__rdmsr_safe_on_cpu()`
reads an invalid MSR, it will do something strange. Looks like handling
exception.
```
# b __rdmsr_safe_on_cpu
b arch/x86/lib/msr-smp.c:158 if rv->msr.msr_no == 0x64d
c
b handle_exception thread 1
```

### Implementation of `read_msr_safe()` in Linux

The main implementation is in `native_read_msr_safe()`
<https://elixir.bootlin.com/linux/v5.10.84/source/arch/x86/include/asm/msr.h#L135>.

```C
	asm volatile("2: rdmsr ; xor %[err],%[err]\n"
		     "1:\n\t"
		     ".section .fixup,\"ax\"\n\t"
		     "3: mov %[fault],%[err]\n\t"
		     "xorl %%eax, %%eax\n\t"
		     "xorl %%edx, %%edx\n\t"
		     "jmp 1b\n\t"
		     ".previous\n\t"
		     _ASM_EXTABLE(2b, 3b)
		     : [err] "=r" (*err), EAX_EDX_RET(val, low, high)
		     : "c" (msr), [fault] "i" (-EIO));
```

There are 3 labels in this function. First 2 is executed. If rdmsr succeeds, it
will clear err and return. If rdmsr fails, the exception handler will run code
in another section (`.fixup`), starting at 3. It sets the registers and jumps
to 1.

The `__EX_TABLE` macro is defined in
<https://elixir.bootlin.com/linux/v5.10.84/source/arch/x86/include/asm/asm.h#L156>.
It adds an entry in the `.__ex_table` section, which says that an error in 2
should be handled in 3.

The IDT exception handler is `handle_exception`. It calls
`exc_general_protection`. See `arch/x86/kernel/{traps,idt}.c`.

### Stopping in RDMSR's exception handler

It is useful to step through the exception handler in GDB. But due to QEMU's
bug, we have to start GDB very late. So add a dead loop as a break point.
```
asm volatile("1: nop; jmp 1b; nop; nop; nop; nop; nop; nop; nop; nop");
```

After GDB connects, edit PC (EIP / RIP) to break out of loop. Need to use
explicit break points, rather than `ni` / `si`.
```
b *($rip + 5)
p $rip += 3
c
```

### Observing Linux's response to injected event

First run Linux in x86 without XMHF

```
source gdb/linux-sym.gdb
b arch/x86/lib/msr-smp.c:158 if rv->msr.msr_no == 0x64d
```

Then step to `0xc143d515 <__rdmsr_safe_on_cpu+21>:	rdmsr`, set a break point
at `handle_exception` and `exc_general_protection`, then si.

```
b handle_exception thread 1
b exc_general_protection thread 1
si
```

Now the PC suddenly jumps to `0xc17fb2af <handle_exception>:	cld`. Also
check that `$ecx` is `0x64d`.

The exception call stack is
```
handle_exception
	-> exc_general_protection
		-> fixup_exception(...) = 1
```

If the exception is not injected correctly, will see a state dump in dmesg

## Fix

`81a80d216..d12b97c0c`
* Implement `rdmsr_safe()`, which executes RDMSR safely. The implementation is
  similar to Linux's `native_read_msr_safe()`.
* Inject event to Linux using vmcs fields

