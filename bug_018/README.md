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

### Checking behaviors

Now re-test different configurations based on `1f0be0167`.

* Test 1: one terminal run `pal_demo`'s `test`, one terminal inject NMIs
  (for QEMU) or run SSH connections (for HP)
* Test 2: two terminals running `pal_demo`
* Test 3: test 1 + another terminal running `pal_demo`
  (combine test 1 and test 2)
* Test 4: Inject NMIs using `qemu_inject_nmi.py` with interval = 0 (QEMU only)

Result
* x86 Debian
	* QEMU: Test 4 pass (detail below)
* x64 Debian
	* QEMU: Test 4 pass (detail below)
* x86 XMHF, x86 Debian
	* HP: Test 1 and 3 pass (assume test 2 also pass)
	* QEMU: Test 1 and 3 pass (assume test 2 also pass),
	  test 4 pass (same as bare-metal)
* x64 XMHF, x86 Debian
	* HP: Test 1 pass, test 2 and 3 fail
	* QEMU: Test 1 pass, test 2 and 3 fail, test 4 pass (same as bare-metal)
* x64 XMHF, x64 Debian
	* HP: Test 1 fail, test 2 fail (assume test 3 fail)
	* QEMU: Test 1 fail, test 2 fail (assume test 3 fail), test 4 fail

For test 4, SSH may stuck if NMIs are continuously sent. After manually stop
sending NMIs, the machine will resume.

Now we found that test 2 is a way to reproduce this problem without relying on
external things (e.g. NMI injections). Also due to the behavior of test 4 on
bare-metal, should try not to use NMI injections to debug.

This also means that the problem is likely to be within interceptions (less
related to exception handling)

### Continue debugging

After debugging in x64 XMHF x64 Debian and test 2, found a deadlock in debug
printf code. The debug code is to print a message in
`xmhf_smpguest_arch_x86_64vmx_quiesce()` while waiting for other CPUs to update
counter.

Normal execution is

|Index|CPU 0|CPU 1|
|-|-|-|
|1|lock printf||
|2|lock quiesce||
|3|send NMI||
|4|wait for other CPUs||
|5|print debug message||
|6||handle NMI|
|7||update counter|
|8|check counter||
|9|unlock printf||

However, deadlock happens when

|Index|CPU 0|CPU 1|CPU 2|
|-|-|-|-|
|1|lock printf|||
|2|lock quiesce|||
|3|send NMI|||
|4|wait for other CPUs|||
|5||handle NMI||
|6||update counter||
|7|||lock printf|
|8|||lock quiesce|
|9|print debug message||(blocked)|
|10|(blocked)|||

See `20220101125907` and `20220101125907.gdb`

Looks like a fix is to lock quiesce first, then lock printf.

A strange thing is why CPU 2 stucks at that place. Normally it should execute
"handle NMI" first. In GDB the counter is 0, so looks like NMI has not arrived
yet?

After fixing, git is at `5dd564c78`, test result is `20220101132039{,.gdb}`.
Looks like NMI is really blocked for some unknown reason. Should probably also
print NMI blocking values in VMCS.

In `20220101150830`, found something interesting:
```
{0,I,32}{3,i,10,0}...
CPU(0x03): got quiesce signal...{1,i,32,0}{0,e,2}{1,I,32}{0,s}{0,n,0,0}{1,i,0,0}{0,N2}{1,p}
CPU(0x00): Quiesced{1,n,1,1}{2,i,0,0}{1,N2}
CPU(0x01): Quiesced{2,p}{2,n,1,1}{2,N2}
CPU(0x03): all CPUs quiesced successfully.
CPU(0x02): QuiescedTV[0]:appmain.c:do_TV_HC_TEST:202:                 CPU(0x03): test hypercall, ecx=0x00000002

CPU(0x03): ending quiesce.
CPU(0x02): EOQ received, resuming...
CPU(0x00): EOQ received, resuming...
CPU(0x01): EOQ received, resuming...{2,P}
CPU(0x03): releasing quiesce lock.{2,I,0}{0,S}{1,P}{0,i,32,0}{1,I,0}{0,I,32}
```

For CPU 0, `{0,I,32}` is return from an intercept, then it immediately sees an NMI exception in `{0,e,2}`. After NMI exception handler returns at `{0,S}`, it see another intercept `{0,i,32,0}`. The exception is unlikely to happen before the previous intercept returns, because a lot of other things happend in between. It is possible that the exception happens before the next interception.

So we try to set a break point at `xmhf_xcphandler_arch_hub`

### Studying NMI

"26.1 ARCHITECTURAL STATE BEFORE A VM EXIT"
> * If an event causes a VM exit directly, it does not update architectural
    state as it would have if it had it not caused the VM exit:
>   * An NMI causes subsequent NMIs to be blocked, but only after the VM exit
      completes.
> * If an event causes a VM exit indirectly, the event does update
  architectural state:
>   * An NMI causes subsequent NMIs to be blocked before the VM exit commences.

"24.3 CHANGES TO INSTRUCTION BEHAVIOR IN VMX NON-ROOT OPERATION"
> IRET. Behavior of IRET with regard to NMI blocking (see Table 23-3) is
  determined by the settings of the "NMI exiting" and "virtual NMIs"
  VM-execution controls:
> * If the "NMI exiting" VM-execution control is 0, IRET operates normally and
  unblocks NMIs.
> * If the "NMI exiting" VM-execution control is 1, IRET does not affect
  blocking of NMIs. If, in addition, the "virtual NMIs" VM-execution control
  is 1, the logical processor tracks virtual-NMI blocking. In this case, IRET
  removes any virtual-NMI blocking.

So my current guess to the intention of these settings is:
* If set NMI exiting = 0, virtual NMIs = 0, then all NMIs will be handled by
  the guest machine. IRET will also behave as expected.
* If set NMI exiting = 1, virtual NMIs = 0, then all NMIs will be handled by
  the host machine. In this case if the host injects an NMI to the guest,
  IRET in guest will not correctly unblock.
	* From what I can see in VMX documentation, after VMEXIT caused directly by
	  NMI, NMI will still block.
	* After VMENTRY, the NMI blocking is determined by interruptibility-state
	  (`vcpu->vmcs.guest_interruptibility`)
* If set NMI exiting = 1, virtual NMIs = 1, then all NMIs will be handled by
  the host machine. If the host injects an NMI to the guest, IRET will behave
  well.
	* <del>
	  The "NMI-window exiting" control is basically saying that "let me
	  (hypervisor) know when the guest is ready to handle an NMI".
	  Maybe this way the host can do something and then inject an NMI?
	  </del>
	* The "NMI-window exiting" control is basically the "NMI pending" bit
	  for the CPU. When the guest can handle NMIs, VMEXIT will occur and
	  the CPU should inject the NMI.

The problem with Intel's manual is that it defines the behavior of virtual
NMIs, but not the use case / design objective.

<del>
The VM-ENTRY Error I see earlier may be caused by this problem. When virtual
NMIs is disabled, and the guest blocks NMIs, injecting NMI to guest will cause
VM-ENTRY failure.
</del> (actually VM-ENTRY Error happens when virtual NMI enabled)

Also, a problem is that it is difficult to know whether currently hypervisor
is blocking NMIs.

Next steps:
* Try to switch to virtual NMIs
* Try to manually use IRET to unblock NMIs in hypervisor

Important VMCS fields
* `vcpu->vmcs.guest_interruptibility`: bit 3 / 0x8 is "Blocking by NMI" (p903)
* `vcpu->vmcs.control_VMX_pin_based`: bit 3 / 0x8 is "NMI exiting" (p906)
* `vcpu->vmcs.control_VMX_pin_based`: bit 5 / 0x20 is "Virtual NMIs" (p906)
* `vcpu->vmcs.control_VMX_cpu_based`: bit 22 / 0x400000 is "NMI-window exiting"
  (p907)
* `vcpu->vmcs.control_VM_entry_interruption_information`: control NMI injection
  when 0x80000202 (p918)

If remove `vcpu->vmcs.guest_interruptibility = 0;` in injection handler, will
see VM-ENTRY error easily. However in Intel's manual it says
> If the "virtual NMIs" VM-execution control is 0, there is no requirement that
  bit 3 be 0 if the valid bit in the VM-entry interruption-information field is
  1 and the interruption type in that field has value 2.

Not sure whether this is a bug in QEMU

### Implementing virtual NMI

First enable virtual NMI in pin based control. This is not enough, because
`vcpu->vmcs.guest_interruptibility = 0;` in injection handler will not honor
NMI blocking and corrupt Linux's state.

Then remove `vcpu->vmcs.guest_interruptibility = 0;`. However, doing this will
cause VM-ENTRY error. This is expected.

Now in NMI handler, check interruptibility. If not interruptible, set NMI
window exiting. Now see "Unhandled intercept in long mode: 0x00000008". This
is expected.

We then set up intercept code for exit reason = "NMI window". The changed code
is `294de9b55..4a7f5b2bb`.

However, after the change test 4 still fails. Behavior: Linux panics because of
NULL pointer dereference. The erroring instruction should be:
```
0xffffffff818b5185 <exc_nmi+181>:	mov    %gs:0x7e765e13(%rip),%rsi        # 0x1afa0 <nmi_dr7>
0xffffffff818b7015 <irqentry_nmi_exit+5>:	mov    %gs:0x7e764b64(%rip),%eax        # 0x1bb80 <__preempt_count>
```

Now I suspect that Linux may have a problem in this. May not be able to fix
this problem at this point.

Can try to print guest RIPs, but not very useful.

After changing to virtual NMI, test 2 still see deadlock.

## Fix

`0b37c866d..`
* Fix race condition in `xmhf_smpguest_arch_x86_64vmx_endquiesce()`
* Make access to `vcpu->quiesced` atomic in NMI handler

# tmp notes

```
for i in {1..1000..2}; do ./test $i; done & for i in {0..1000..2}; do ./test $i; done
```

TODO: use sleep to try to achieve deterministic run, test whether NMI is blocked
TODO: in exception handler, when write to `vcpu->vmcs`, should also update the
real VMCS using VMWRITE.

TODO: print vmcs fields related to NMI. Likely Linux's state is corrupted (is it possible that XMHF injected 2 NMIs that violates NMI blocking)

TODO: maybe EPT or XMHF's page table is not large enough?
TODO: is "virtual NMIs" enabled? Is it needed?
TODO: may need to move `emhfc_putchar_lineunlock(emhfc_putchar_linelock_arg);` line in smpg, or add documentation

TODO: guess the reason for `vcpu->quiesced`, is it possible to be VM intercept then exception?
TODO: remove deadlock

TODO: consider disabling other CPUs

TODO: collect results of multiple runs

