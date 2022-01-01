# Cannot run x86 `pal_demo` in x64 XMHF on HP

## Scope
* bad: x86 `pal_demo`, x64 / x86 Debian, x64 XMHF, HP
* good: x86 `pal_demo`, x86 Debian, x86 XMHF, HP
* good: x86 `pal_demo`, x64 / x86 Debian, x64 XMHF, QEMU

```rst
+----+---+------------------+------------+-------------------------------------+
|    |   |                  |            | Status                              |
|    |   |Operating         |            +------------------+------------------+
|XMHF|DRT|System            |Application | HP               | QEMU             |
+====+===+==================+============+==================+==================+
| x86| N | Debian 11 x86    |pal_demo x86| good             | good             |
+----+   +------------------+------------+------------------+------------------+
| x64|   | Debian 11 x86    |pal_demo x86| bad              | good             |
|    |   +------------------+------------+------------------+------------------+
|    |   | Debian 11 x64    |pal_demo x86| bad              | good             |
+----+---+------------------+------------+------------------+------------------+
```

## Behavior

Also see `bug_017`

When running `pal_demo`, get unexpected exceptions of different types.

## Debugging

### Quiesce locking logic

I suspect that there is a low possibility that quiesce code has a bug related
to concurrent programming. So review the logic. A simplified code is:
```
xmhf_smpguest_arch_x86_64vmx_eventhandler_nmiexception
	if(vcpu->quiesced)
		return;
	vcpu->quiesced=1;
	g_vmx_quiesce_counter++;
	while(!g_vmx_quiesce_resume_signal);
	g_vmx_quiesce_resume_counter++;
	vcpu->quiesced=0;

xmhf_smpguest_arch_x86_64vmx_quiesce
	emhfc_putchar_linelock(emhfc_putchar_linelock_arg);
	spin_lock(&g_vmx_lock_quiesce);
	vcpu->quiesced = 1;
	g_vmx_quiesce_counter=0;
	g_vmx_quiesce=1;
	_vmx_send_quiesce_signal(vcpu);
	while(g_vmx_quiesce_counter < (g_midtable_numentries-1) );
	emhfc_putchar_lineunlock(emhfc_putchar_linelock_arg);

xmhf_smpguest_arch_x86_64vmx_endquiesce
	g_vmx_quiesce_resume_counter=0;
	g_vmx_quiesce_resume_signal=1;
	while(g_vmx_quiesce_resume_counter < (g_midtable_numentries-1) );
	vcpu->quiesced=0;
	g_vmx_quiesce=0;
	g_vmx_quiesce_resume_signal=0;
	spin_unlock(&g_vmx_lock_quiesce);
```

Looks like a deadlock will still occur if something happens in the body of
`xmhf_smpguest_arch_x86_64vmx_quiesce()`. For debugging this bug may need to
remove lock in printf.

### Collecting behaviors

Patch commit `2a207ec98` with `smpg1.diff` to get some verbose output.

This patch is used to collect `20211230092707 - 20211230094426`.

This patch is used to collect `20211230095148 - 20211230100452` (with printf
lock removed, see below).

### Multiplexing printf

Can try to remove the printf lock to make sure that there are no error messages
blocked by deadlock. But multiple CPUs will print multiple lines at the same
time.

Patch commit `2a207ec98` with `vprintf_encode.diff`, the serial port will
multiplex each CPU's output. `vprintf_decode.py` can decode the output (use
color to differentiate different CPU's output).

However, looks like when CPUs are accessing serial I/O at the same time, there
are still collisions. So give up for now

### Quiesce locking logic 2

Patch commit `2a207ec98` with `smpg1.diff` to get some verbose output on
the quiesce process. Collected `20211230113236 - 20211230113739`

After reading the results the I still think there is a problem related to
concurrent programming.

The problem in the log output is that after a CPU prints
"EOQ received, resuming", it prints "Quiesced", but no other CPUs should be
sending NMIs at that time.

From the NMI handler logic, nothing prevents the CPU from entering the while
loop again. So there is a deadlock. The fact that the CPU receives unexpected
NMI may also be another problem.
```c
xmhf_smpguest_arch_x86_64vmx_eventhandler_nmiexception
	if (g_vmx_quiesce)
		if(vcpu->quiesced)
			return;
		vcpu->quiesced=1;
		g_vmx_quiesce_counter++;
		"Quiesced"
		while(!g_vmx_quiesce_resume_signal);
		"EOQ received, resuming"
		g_vmx_quiesce_resume_counter++;
		vcpu->quiesced=0;
	else
		normal handling

xmhf_smpguest_arch_x86_64vmx_quiesce
	"got quiesce signal..."
	emhfc_putchar_linelock(emhfc_putchar_linelock_arg);
	spin_lock(&g_vmx_lock_quiesce);
	vcpu->quiesced = 1;
	g_vmx_quiesce_counter=0;
	g_vmx_quiesce=1;
	_vmx_send_quiesce_signal(vcpu);
	while(g_vmx_quiesce_counter < (g_midtable_numentries-1) );
	emhfc_putchar_lineunlock(emhfc_putchar_linelock_arg);
	"all CPUs quiesced successfully"

xmhf_smpguest_arch_x86_64vmx_endquiesce
	"ending quiesce"
	g_vmx_quiesce_resume_counter=0;
	g_vmx_quiesce_resume_signal=1;
	while(g_vmx_quiesce_resume_counter < (g_midtable_numentries-1) );
	vcpu->quiesced=0;
	g_vmx_quiesce=0;
	g_vmx_quiesce_resume_signal=0;
	spin_unlock(&g_vmx_lock_quiesce);
	"releasing quiesce lock"
```

In AMT term, can see for example:
```
CPU(0x00): ending quiesce.
CPU(0x01): EOQ received, resuming...
CPU(0x04): EOQ received, resuming...
CPU(0x05): EOQ received, resuming...
CPU(0x04): Quiesced
CPU(0x00): releasing quiesce lock.
CPU(0x00): got quiesce signal...
```

Also, in `xmhf_smpguest_arch_x86_64vmx_eventhandler_nmiexception()`, access
to `vcpu->quiesced` is not atomic. If in between a nested NMI happens, will
cause problem.

```
			if(vcpu->quiesced)
				return;
				
			vcpu->quiesced=1;
```

### Redesign NMI handling

Related documentation
* Intel v3 "6.7 NONMASKABLE INTERRUPT (NMI)"
* Intel v3 "6.7.1 Handling Multiple NMIs": NMI cannot be nested

Define states for NMI handling
* 0: normal execution
* 1: waiting for other CPUs to stop
* 2: executing hypapp
* 3: waiting for other CPUs to resume
* (go back to 0)

Now I realize that re-designing to using states is not necessary. The problem
is the order of instructions in `xmhf_smpguest_arch_x86_64vmx_endquiesce()`.
Before setting `g_vmx_quiesce_resume_signal = 1`, should set
`g_vmx_quiesce = 0`.

After fix, success rate in (x64 XMHF, x64 Debian 11, HP) increases a lot.

### Dmesg

When running the PAL demo, see in `dmesg -w`:
```
[   79.615174] perf: interrupt took too long (3252 > 3187), lowering kernel.perf_event_max_sample_rate to 61500
[  332.435354] perf: interrupt took too long (4100 > 4065), lowering kernel.perf_event_max_sample_rate to 48750
[  336.567681] perf: interrupt took too long (5193 > 5125), lowering kernel.perf_event_max_sample_rate to 38500
[  341.979527] perf: interrupt took too long (6499 > 6491), lowering kernel.perf_event_max_sample_rate to 30750
[  356.519521] perf: interrupt took too long (8247 > 8123), lowering kernel.perf_event_max_sample_rate to 24250
[  366.878673] perf: interrupt took too long (10322 > 10308), lowering kernel.perf_event_max_sample_rate to 19250
[  490.175542] smpboot: Scheduler frequency invariance went wobbly, disabling!
```

Looks like sometimes error relates to external events to the CPU. For example,
in `20211230155651`, when I try to establish a new ssh connection to HP, the
error happens.

### Print-debugging handlers

By print debugging, it looks like initiating a ssh connection will cause an NMI
interrupt. So guess that the problem is related to nested NMIs.

In QEMU, ssh will not cause NMI interrupt?

The problem is that printf cannot be called in a lot of places, due to 
deadlock. The solution is to release print lock before busy-waiting for other
CPUs. The possible problem is whether the NMI interrupt or the lock change will
arrive first to other CPUs.

After patching commit `29065357e` with `print3.diff`, collected
`20211230172255 - 20211230180053`. However, maybe better to read code.

### Debugging using QEMU

In QEMU, can send nmi to CPU using `nmi` command in QEMU. If run `pal_demo` and
type `nmi` a lot of times, can see this bug.

Serial output in `20211231102445qemu`, looks like all cores are in guest mode
in GDB
```
(gdb) info th
  Id   Target Id                    Frame 
* 1    Thread 1.1 (CPU#0 [running]) sched_show_task (p=0xfffffe000000dc30) at kernel/sched/core.c:6428
  2    Thread 1.2 (CPU#1 [halted ]) 0xffffffff81035a1e in prefer_mwait_c1_over_halt (c=0x0 <fixed_percpu_data>) at arch/x86/kernel/process.c:782
  3    Thread 1.3 (CPU#2 [halted ]) 0xffffffff81035a1e in prefer_mwait_c1_over_halt (c=0x0 <fixed_percpu_data>) at arch/x86/kernel/process.c:782
  4    Thread 1.4 (CPU#3 [halted ]) 0xffffffff81035a1e in prefer_mwait_c1_over_halt (c=0x0 <fixed_percpu_data>) at arch/x86/kernel/process.c:782
(gdb) bt
#0  sched_show_task (p=0xfffffe000000dc30) at kernel/sched/core.c:6428
#1  sched_show_task (p=0xfffffe000000dc30) at kernel/sched/core.c:6418
#2  0xfffffe000000dbe0 in ?? ()
#3  0x577ebd705ac4b600 in ?? ()
Backtrace stopped: Cannot access memory at address 0xc000
(gdb) t 2
[Switching to thread 2 (Thread 1.2)]
#0  0xffffffff81035a1e in prefer_mwait_c1_over_halt (c=0x0 <fixed_percpu_data>) at arch/x86/kernel/process.c:782
782		if (!cpu_has(c, X86_FEATURE_MWAIT) || boot_cpu_has_bug(X86_BUG_MONITOR))
(gdb) bt
#0  0xffffffff81035a1e in prefer_mwait_c1_over_halt (c=0x0 <fixed_percpu_data>) at arch/x86/kernel/process.c:782
#1  select_idle_routine (c=0x0 <fixed_percpu_data>) at arch/x86/kernel/process.c:825
#2  0xffffc90000083e20 in ?? ()
Backtrace stopped: Cannot access memory at address 0xffffc900000c9000
(gdb) 
```

The backtraces for thread 2 - 4 are incorrect. `0xffffc90000083e20` is stack
address. By manually backtracing, the possible call stack is.
```
0xffffffff81a010e2 <asm_call_on_stack+2>
0xffffffff818b65d2 <xen_build_mfn_list_list+82>
0xffffffff81a00d82 <asm_sysvec_reboot+18>
```

In Debian's virtual terminal, see "BUG: kernel NULL pointer dereference", and
`RIP: 0010:0x0`.

Can try QEMU's QMP to send NMIs automatically:
<https://wiki.qemu.org/Documentation/QMP>

In `20211231114559`, looks like something happend during TrustVisor code.
However, TrustVisor is printing something, so it should not fault. Other CPUs
should be quiesced at this time. So there should be an interrupt or NMI that
cause this problem.

For now, things to try:
* Try to reproduce this problem in QEMU
* Replace TrustVisor with some simple code that does nothing (e.g. helloworld)
* Read code carefully

### QEMU Machine Protocol (QMP)
Add `-qmp tcp:localhost:4444,server,nowait` to QEMU command line

Then telnet to port 4444, first send `{ "execute": "qmp_capabilities" }`,
then `{ "execute": "inject-nmi" }`.

Using `qemu_inject_nmi.py` can do this automatically, with a fixed interval.

Currently I guess the problem is that when multiple NMIs happen close in time,
XMHF does not correctly inject NMI back to guest machine.

### Execution sequences

(These are markdown tables. Render this markdown file or view in GitHub)

Normal quiesce:

|Index|CPU 0|CPU 1|CPU 2|CPU 3|
|-|-|-|-|-|
|1|In guest|In guest|In guest|In hypervisor|
|2|VMEXIT||||
|3|quiesce()||||
|4||NMI interrupted|NMI interrupted|NMI interrupted|
|5||VMEXIT|VMEXIT|Xcph handler|
|6||Update counter|Update counter|Update counter|
|7||busy wait|busy wait|busy wait|
|8|Call hypapp||||
|9|Return from hypapp||||
|10|endquiesce()||||
|11||Update counter|Update counter|Update counter|
|12||VMENTER|VMENTER|Iret|

Normal guest exception:

|Index|CPU 0|CPU 1|CPU 2|CPU 3|
|-|-|-|-|-|
|1|In guest|In hypervisor|In guest|In guest|
|2|||Send NMI||
|3|NMI interrupted|NMI interrupted||...|
|4|VMEXIT|Xcph handler|||
|5|Inject NMI to guest|Inject NMI to guest|||
|6|VMENTRY|Iret|||

Good nested exception case

|Index|CPU 0|CPU 1|CPU 2|CPU 3|Hardware|
|-|-|-|-|-|-|
|1|In guest|In guest|In guest|In hypervisor||
|2|VMEXIT|||||
|3|quiesce()|||||
|4||NMI interrupted|NMI interrupted|NMI interrupted||
|5||VMEXIT|VMEXIT|Xcph handler||
|6||Update counter|Update counter|Update counter||
|7||busy wait|busy wait|busy wait||
|8|Call hypapp|||||
|9|||||Send NMI|
|10||NMI interrupted|...|...||
|11||Xcph handler||||
|12||Inject NMI to guest||||
|13||Iret||||
|14|Return from hypapp|||||
|15|endquiesce()|||||
|16||Update counter|Update counter|Update counter||
|17||VMENTER|VMENTER|Iret||

Race condition

|Index|CPU 0|CPU 1|CPU 2|CPU 3|
|-|-|-|-|-|
|1|In guest|In hypervisor|In guest|In guest|
|2||||Send NMI|
|3||NMI interrupted|||
|4||Xcph handler|||
|5|VMEXIT||||
|6|quiesce()||||
|7||(Blocked NMI)|NMI interrupted|NMI interrupted|
|8|||VMEXIT|VMEXIT|
|9||Update counter|Update counter|Update counter|
|10||busy wait|busy wait|busy wait|
|11|Call hypapp||||
|12|Return from hypapp||||
|13|endquiesce()||||
|14||Update counter|Update counter|Update counter|
|15||Iret|VMENTER|VMENTER|
|16||(NMI unblocked)|||
|17||NMI interrupted|||
|18||(Ignore interrupt)|||
|19||Iret|||
|20||...|||
|21||VMEXIT|||

CPU 1 handles two NMI interrupts without nesting. The first one is from guest
(should forward to guest). The second one is from quiesce.

After index 8, CPU 1 checks the variable `g_vmx_quiesce`. This variable is
true since CPU 0 is faster. So it executes quiesce code. At the second
interrupt, it checks `g_vmx_quiesce` and see false. This interrupt is not from
guest, so CPU 1 will not forward this interrupt to guest.

As a result, CPU 1 will never deliver the NMI to guest. This is a bug in
`xmhf_smpguest_arch_x86_64vmx_eventhandler_nmiexception()` and should be fixed.

At this point, code repo is equivalent to patching commit `29065357e` with
`print4.diff`.

However, this bug is not so easy to fix. Currently committed `aa2d65ef0`
(Make access to `vcpu->quiesced` atomic in NMI handler). The next step should
be fixing the race condition, but not sure how to implement at this point.
Created another temporary branch.

### Ruling out problems in TrustVisor

Wrote `test.c` in `pal_demo` to make `TV_HC_TEST` VMCALL. Now the behavior can
be simplified:

Create a loop using `for i in {1..10000}; do ./test $i; done`. In QEMU run
successfully. In HP or in (QEMU with manually injected NMIs), will crash.
To make HP more likely to fail, can create a lot of new SSH connections.

We are now sure that the problem is not related to TrustVisor. We also know
that it is very unlikely to be a stack overflow. As a next step, we print-debug
events (intercepts and exceptions) in XMHF.


## Fix

`0b37c866d..`
* Fix race condition in `xmhf_smpguest_arch_x86_64vmx_endquiesce()`
* Make access to `vcpu->quiesced` atomic in NMI handler

# tmp notes

TODO: test on {x86,x64} QEMU x86 Debian
TODO: print vmcs fields related to NMI. Likely Linux's state is corrupted (is it possible that XMHF injected 2 NMIs that violates NMI blocking)
TODO: is there a problem if inject a lot of NMIs in QEMU, but not run any VMCALL?

TODO: maybe EPT or XMHF's page table is not large enough?
TODO: is "virtual NMIs" enabled? Is it needed?
TODO: may need to move `emhfc_putchar_lineunlock(emhfc_putchar_linelock_arg);` line in smpg, or add documentation

TODO: guess the reason for `vcpu->quiesced`, is it possible to be VM intercept then exception?
TODO: remove deadlock

TODO: consider disabling other CPUs

TODO: try to simply remove printf lock
TODO: or can try to use something like base64
TODO: collect results of multiple runs


TODO: for analyzing NMI, may be able to generate NMI in Linux with `perf`:
 <https://winddoing.github.io/post/98e04b95.html>

