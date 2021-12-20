# QEMU booting Debian 11 hangs due to RDTSCP

## Scope
* At least x86, Debian 11, QEMU

## Behavior
In QEMU, boot XMHF64, then Debian 11 Linux 5.10.0-9-686, serial port prints
```
MOV TO CR4 (flush TLB?), current=0x00002000, proposed=0x00000000
_vmx_handle_intercept_xsetbv: xcr_value=7
_vmx_handle_intercept_xsetbv: host cr4 value=00042030
_vmx_handle_intercept_xsetbv: xcr_value=7
_vmx_handle_intercept_xsetbv: host cr4 value=00042030
```
Then the system hangs

## Debugging

GDB shows that Linux code is busy waiting for a lock (`logbuf_lock`)

Stack trace:
```
(gdb) bt
#0  virt_spin_lock (lock=0xc1c9dd84 <logbuf_lock>) at arch/x86/include/asm/qspinlock.h:99
#1  native_queued_spin_lock_slowpath (lock=0xc1c9dd84 <logbuf_lock>, val=<optimized out>) at kernel/locking/qspinlock.c:326
#2  0xc17fa3da in pv_queued_spin_lock_slowpath (val=1, lock=0xc1c9dd84 <logbuf_lock>) at arch/x86/include/asm/paravirt.h:554
#3  queued_spin_lock_slowpath (val=1, lock=0xc1c9dd84 <logbuf_lock>) at arch/x86/include/asm/qspinlock.h:51
#4  queued_spin_lock (lock=0xc1c9dd84 <logbuf_lock>) at include/asm-generic/qspinlock.h:85
#5  do_raw_spin_lock (lock=0xc1c9dd84 <logbuf_lock>) at include/linux/spinlock.h:183
#6  __raw_spin_lock (lock=0xc1c9dd84 <logbuf_lock>) at include/linux/spinlock_api_smp.h:143
#7  _raw_spin_lock (lock=lock@entry=0xc1c9dd84 <logbuf_lock>) at kernel/locking/spinlock.c:151
#8  0xc10ca594 in console_unlock () at kernel/printk/printk.c:2475
#9  0xc10ccb8e in console_unblank () at kernel/printk/printk.c:2601
#10 0xc13dafbd in bust_spinlocks (yes=yes@entry=0) at lib/bust_spinlocks.c:28
#11 0xc101c3cb in oops_end (flags=flags@entry=2097222, regs=regs@entry=0xc1a6fa94, signr=11) at arch/x86/kernel/dumpstack.c:361
#12 0xc101c679 in die (str=str@entry=0xc19535e9 "invalid opcode", regs=regs@entry=0xc1a6fa94, err=err@entry=0) at arch/x86/kernel/dumpstack.c:446
#13 0xc10191bb in do_trap_no_signal (error_code=0, regs=0xc1a6fa94, str=<optimized out>, trapnr=6, tsk=0xc1a73480 <init_task>) at arch/x86/kernel/traps.c:119
#14 do_trap (trapnr=trapnr@entry=6, signr=signr@entry=4, str=str@entry=0xc19535e9 "invalid opcode", regs=0xc1a6fa94, error_code=0, sicode=2, addr=0xc10547e3 <pvclock_clocksource_read+19>) at arch/x86/kernel/traps.c:157
#15 0xc101927c in do_error_trap (regs=regs@entry=0xc1a6fa94, error_code=error_code@entry=0, str=str@entry=0xc19535e9 "invalid opcode", trapnr=6, signr=4, sicode=2, addr=0xc10547e3 <pvclock_clocksource_read+19>) at arch/x86/kernel/traps.c:177
#16 0xc17f0a24 in handle_invalid_op (regs=0xc1a6fa94) at arch/x86/kernel/traps.c:214
#17 exc_invalid_op (regs=0xc1a6fa94) at arch/x86/kernel/traps.c:260
#18 0xc17fb3ef in handle_exception () at /build/linux-Zn7N0z/linux-5.10.84/arch/x86/entry/entry_32.S:1168
#19 0xc1c99000 in ?? ()
Backtrace stopped: previous frame inner to this frame (corrupt stack?)
(gdb) 
```

The busy wait happens when the kernel dies. In frame `#16 handle_invalid_op`,
can see register values and faulty instruction:
```
(gdb) p *regs
$21 = {bx = 3251212288, cx = 3251088896, dx = 0, si = 3251241440, di = 0, bp = 3248945928, ax = 4, ds = 123, __dsh = 24933, es = 123, __esh = 30768, fs = 216, __fsh = 0, gs = 224, __gsh = 0, orig_ax = 4294967295, ip = 3238348771, cs = 96, __csh = 8192, flags = 2162690, sp = 3248945904, ss = 104, __ssh = 0}
(gdb) p (void*) regs->ip
$22 = (void *) 0xc10547e3 <pvclock_clocksource_read+19>
(gdb) x/i regs->ip
   0xc10547e3 <pvclock_clocksource_read+19>:	rdtscp 
(gdb) 
```

Then look up Intel manual, find that RDTSCP needs to be enabled in hypervisor

## Fix
`3b8955c74..81a80d216`
Set VMCS control field to enable the RDTSCP instruction

