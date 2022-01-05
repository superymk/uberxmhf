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

(Commit `aa2d65ef0` is found to be unnecessary later)

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
is expected. (later: I think this implementation is incorrect, because NMI in
exception handler may overwrite other injections. The correct way is to always
set NMI window exiting, and inject NMI in next VMEXIT due to NMI windowing)

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

### Implementing IRET

Implemented iret in `4a7f5b2bb..e0149361e`. Looks like no longer have
deadlocks. But still have other problems, like page fault in Linux.

In my current experiment, looks like the page fault happens after an NMI
exception during intercept 31 (RDMSR). See `20220102113103{,.gdb}`. CPU 3 in
Linux got the page fault, and the page fault address is `<native_write_msr+8>`.
However, `<native_write_msr+4>` is rdmsr, `<native_write_msr+6>` is nop
(5 bytes). So the hypervisor did not return RIP correctly?

In `20220102120442`, see VMENTRY failure.

### vmwrite in 64-bit

VMX instruction reference says that the operand size for VMREAD and VMWRITE are
32 bits in x86 and 64 bits in x64.

Use the following gdb script to debug
```
# From gdb/x64_rt_pre.gdb
b xmhf_baseplatform_arch_x86_64vmx_getVMCS
b xmhf_baseplatform_arch_x86_64vmx_putVMCS
c
```

Debug result is:
```
Thread 1 hit Breakpoint 4, 0x00000000102138c0 in ?? ()
(gdb) b xmhf_baseplatform_arch_x86_64vmx_getVMCS
Breakpoint 5 at 0x1020ea17: file arch/x86_64/vmx/bplt-x86_64vmx-vmcs.c, line 74.
(gdb) b xmhf_baseplatform_arch_x86_64vmx_putVMCS
Breakpoint 6 at 0x1020e98b: file arch/x86_64/vmx/bplt-x86_64vmx-vmcs.c, line 59.
(gdb) c
Continuing.

Thread 1 hit Breakpoint 6, xmhf_baseplatform_arch_x86_64vmx_putVMCS (vcpu=0x10249820 <g_vcpubuffers>) at arch/x86_64/vmx/bplt-x86_64vmx-vmcs.c:59
59	    for(i=0; i < g_vmx_vmcsrwfields_encodings_count; i++){
(gdb) n
77	        __vmx_vmread(encoding, field);
(gdb) p encoding
$1 = 0
(gdb) p field
$2 = (unsigned long *) 0x1024aca4 <g_vcpubuffers+5252>
(gdb) p vcpu->vmcs
$3 = {control_vpid = 1, control_VMX_pin_based = 30, ...}
(gdb) p vcpu->vmcs.control_vpid
$4 = 1
(gdb) p vcpu->vmcs.control_VMX_pin_based
$5 = 30
(gdb) p g_vmx_vmcsrwfields_encodings[i].fieldoffset
$6 = 0
(gdb) n
74	    for(i=0; i < g_vmx_vmcsrwfields_encodings_count; i++){
(gdb) p vcpu->vmcs.control_vpid
$7 = 1
(gdb) p vcpu->vmcs.control_VMX_pin_based
$8 = 0
(gdb) n
75	        unsigned int encoding = g_vmx_vmcsrwfields_encodings[i].encoding;
(gdb) 
76	        unsigned long *field = (unsigned long *)((hva_t)&vcpu->vmcs + (u32)g_vmx_vmcsrwfields_encodings[i].fieldoffset);
(gdb) 
77	        __vmx_vmread(encoding, field);
(gdb) 
74	    for(i=0; i < g_vmx_vmcsrwfields_encodings_count; i++){
(gdb) p vcpu->vmcs.control_VMX_pin_based
$9 = 30
(gdb) 
```

Can see that during vmread of a 32-bit field, other fields are incorrectly
cleared. The VMCS may be read incorrectly if order of read is not same as order
of fields declared in the structure.

This problem does not happen in x86 mode because all memory accesses are 4
bytes, and all data types in the struct are >= 4 bytes.

Fixed this problem in `231f0d65d..4b8f8b277` (only VMREAD is affected; ideally
should change VMWRITE, but not now), but `bug_018` still not fixed.

### Sleeping and NMI

In `9d853eed9`, able to use VMCALL to sleep in hypervisor mode. Then can use
QEMU to inject NMI and see whether hypervisor mode is blocking NMI.
Use `while ! ./test 1000000; do true; done` to force executing sleep on CPU 0.
QEMU will always inject NMI to CPU 0.

Result is in `20220102211601`. NMIs are injected after "start busy loop", but
after some time see "end busy loop", and then see NMI as interception
`{0,p}{0,n,1,1}{0,N7}{0,P}`.

Way to reproduce is:
1. Inject an NMI
2. Start busy loop
3. Before busy loop ends, inject an NMI
4. After when busy loop ends, see NMI handled (unexpected behavior):
   `{0,p}{0,n,1,1}{0,N7}{0,P}`

So in hypervisor mode NMIs are effectively blocked. NMIs can only be seen when
in guest mode through NMI exiting.

However, before the first NMI interception, if all NMIs are handled through
exception handler, hypervisor will not be blocking NMI. e.g. `20220102212211`:
1. Start busy loop
2. Before busy loop ends, inject an NMI
3. See exception handler handles NMI (expected):
   `{0,e,2,0x19963dc8,18}{0,s}{0,n,0,0}{0,N7}{0,S}`
4. See busy loop ends

Also, note that `vcpu->vmcs.guest_interruptibility == 0` at all times.

Currently, the code is using virtual NMIs. To remove virtual NMIs, just need
to remove the `vcpu->vmcs.control_VMX_pin_based |= (1 << 5);` line, because
we have already disabled injecting NMIs to guest. The behavior is the same.

Also, when any CPU is busy looping in hypervisor mode, SSH freezes. Why?

Then I tested this bug on x86, looks like the deadlock also occurs. In previous
tests did not find the problem because the critical section is executed too
quickly. If add the `printf("\nCPU(0x%02x): got quiesce signal...", vcpu->id);`
line in `xmhf_smpguest_arch_x86vmx_quiesce`, can easily reproduce this bug.

In x86, also see the same behavior by letting hypervisor busy wait and
injecting NMI. Code is at `a3a9bfb45`, serial output in `20220103092828`.

In x86, if use iret to unblock NMIs, the problem is fixed. Code in `a9caaed31`.

### Reverting `aa2d65ef0`

After reviewing the code using new understanding about NMI blocking, can see
that commit `aa2d65ef0` is not necessary. Because when executing the critical
section NMI is always blocked, regardless called from interception handler or
exception handler.

In `e7ff86ac0`, reverted `aa2d65ef0`

### Updating code

Did some code update on `xmhf64` branch `4b8f8b277..3cf7c7a38`. Revised the
design of logic of nmi handler.

When CPU 0 requests quiesce, on other CPUs look like:

```
time ----->

hypervisor          ------- Quiesce ---- iret ----
                   /(NMI)                         \
guest       -------                                ---------

event            ^ NMI for quiesce
```

When an NMI for guest is delivered during vmx-nonroot operation, look like:

```
hypervisor          ------- inject exception to guest ----
                   /(NMI)                                 \
guest       -------                                        ---------

event            ^ NMI for guest
```

When an NMI for guest is delivered during vmx-root operation (e.g. by VMCALL):

```
exception                     ---- Quiesce ---
                             /                \(iret)
hypervisor          ---------                  -----------
                   /(other)                               \
guest       -------                                        ---------

event                       ^ NMI for NMI
```

In this case, no need to execute iret explicitly because exception return is
iret.

When an NMI for guest is delivered during NMI intercept handler for quiesce:

```
hypervisor          ----- Quiesce -- iret ---   ------- inject --
                   /(NMI)                    \ /(NMI)            \
guest       -------                           -                   ---

event             ^ NMI for quiesce
                        ^ NMI for guest
```

This is actually the same for NMI for guest happening before NMI for quiesce.

For each NMI received by the hypervisor, either through intercept or exception,
it should either be handled as a quiesce request or an event injection to the
guest. How it should be handled does not depend on whether called from
intercept handler or exception.

Since the guest needs to handle NMIs, virtual NMIs is likely needed (assuming
my understanding about NMIs in VMX is correct).

For the guest, the NMI handler may be executed late if it is blocked by
quiesce. This is likely a valid behavior, and is difficult to be fixed.

### NMI Write up

Possible configurations
* NMI exiting = 0, virtual NMIs = 0
* NMI exiting = 1, virtual NMIs = 0
* NMI exiting = 1, virtual NMIs = 1

CPU states
* VMX operation (root = hypervisor / non-root = guest)
* NMI blocking
* Virtual NMI blocking

Events
* CPU receive NMI
* CPU executes iret
* Inject NMI to guest during VMENTRY

Table (may have errors and typo):

|Event|Virtual NMI blocking|NMI blocking|VMX|NMI Exiting = 0, virtual NMIs = 0|NMI Exiting = 1, virtual NMIs = 0|NMI Exiting = 1, virtual NMIs = 1|
|-|-|-|-|-|-|-|
|Receive NMI|0|0|root|HV NMI handler, NMI Blocking = 1|HV NMI handler, NMI Blocking = 1|HV NMI handler, NMI Blocking = 1|
||0|0|non-root|Guest NMI handler, NMI Blocking = 1|VMEXIT, NMI Blocking = 1|VMEXIT, NMI Blocking = 1|
||0|1|root|Blocked|Blocked|Blocked|
||0|1|non-root|Blocked|VMEXIT, NMI Blocking = 1|VMEXIT, NMI Blocking = 1|
||1|0|root|N/A|N/A|HV NMI handler, NMI Blocking = 1|
||1|0|non-root|N/A|N/A|VMEXIT, NMI Blocking = 1|
||1|1|root|N/A|N/A|Blocked|
||1|1|non-root|N/A|N/A|VMEXIT, NMI Blocking = 1|
|VM Entry, host inject to guest|0|0|root -> non-root|?|NV NMI handler|NV NMI handler|
||0|1|root -> non-root|?|NV NMI handler|NV NMI handler|
||1|0|root -> non-root|N/A|N/A|Disallowed|
||1|1|root -> non-root|N/A|N/A|Disallowed|
|Execute iret|0|0|root|-|-|-|
||0|0|non-root|-|-|-|
||0|1|root|NMI Blocking = 0|NMI Blocking = 0|NMI Blocking = 0|
||0|1|non-root|NMI Blocking = 0|-|-|
||1|0|root|N/A|N/A|-|
||1|0|non-root|N/A|N/A|Virtual NMI Blocking = 0|
||1|1|root|N/A|N/A|NMI Blocking = 0|
||1|1|non-root|N/A|N/A|Virtual NMI Blocking = 0|

I believe that for our use case, virtual NMI have to be implemented.

Also a blog post in Chinese that may be helpful:
<https://www.cnblogs.com/haiyonghao/p/14389529.html>

### Testing update

After implemeting virtual NMIs in `xmhf64` branch, looks like the program is
even less stable. Current commit is `26d22939d`.
In x64 XMHF x64 Debian x86 pal, running
`for i in {0..100..2}; do ./main $i $i; done` will result in VMENTRY error. For
example `20220103155652`. Running `for i in {0..100..2}; do ./test $i $i; done`
will also result in error. For example `20220103161912` (code `761be4751`).

Also can see problems in x86 XMHF x86 Debian x86 pal.

### Comparing VCPU dump

A few errors are VMENTRY error. For example `20220103161912`. Collect the VCPU
dump result in `results/20220103161912_vcpudump`, and dump VCPU during a normal
run (result in `results/20220103162941_vcpudump`), find the difference.

`vcpu->vmcs.info_vminstr_error=0x00000007` shows that control field has an
error. Looks like the problem is that `control_VMX_seccpu_based` is changed
to another value. This value is for `control_VMX_cpu_based`
```diff
@@ -46,3 +46,3 @@
 CPU(0x03): vcpu->vmcs.control_VMX_cpu_based=0x86006172
-CPU(0x03): vcpu->vmcs.control_VMX_seccpu_based=0x86006172
+CPU(0x03): vcpu->vmcs.control_VMX_seccpu_based=0x000010aa
 CPU(0x03): vcpu->vmcs.control_exception_bitmap=0x00000000
```

Changed code to `238eb4c68`. Found that serial shows
```
Fatal: Halting! Condition 'value != 0x86006172' failed, line 78, file arch/x86_64/vmx/bplt-x86_64vmx-vmcs.c
```

This means that VMREAD is called with the correct encoding, but get wrong
value. The next step is to check whether VMWRITE is called with the right
value. If not, then maybe there is something wrong with KVM / QEMU.

Also, looks like KVM nested virtualization does not use the region provided in
VMPTRLD to store data. All fields are 0 except the version:
```
(gdb) p/x ((long*) g_vmx_vmcs_buffers)[0]@2048
$22 = {0x11e57ed0, 0x0 <repeats 511 times>, 0x11e57ed0, 0x0 <repeats 511 times>, 0x11e57ed0, 0x0 <repeats 511 times>, 0x11e57ed0, 0x0 <repeats 511 times>}
(gdb) 
```

Tried to check whether VMWRITE writes the incorrect value, but looks like not
the case. Then changed VMWRITE such that when writing 32-bit fields, force
upper-32 bits to 0. Currently upper-32 bits may be another 32-bit field. Then
the problem is fixed. However, how it is fixed is strange.

Currently, 
`761be4751` is bad (see unhandled exception 3)
`1e7d5f77c` is good (fix vmwrite)
`b40aa6a5b` is good (essentially remove fix in vmwrite)
`ce15316eb` is bad (`20220103213830`, VMENTRY error)

The problem with `20220103213830` is that `guest_RFLAGS` is corrupted. Using
`diff <(cut -b 10- 20220103213830_vcpudump) <(cut -b 10- 20220103162941_vcpudump)`
to compare, can see that it is similar to `guest_RIP`. Looks like `guest_RFLAGS`
is `guest_RIP` before VMEXIT, and the intercept handler added to `guest_RIP`.
```
118,120c117,119
< : vcpu->vmcs.guest_RSP=0xffffc900000c8f50
< : vcpu->vmcs.guest_RIP=0xffffffff8106b3e6
< : vcpu->vmcs.guest_RFLAGS=0xffffffff8106b3e4
---
> : vcpu->vmcs.guest_RSP=0xffffc90000120e38
> : vcpu->vmcs.guest_RIP=0xffffffff8106b3e4
> : vcpu->vmcs.guest_RFLAGS=0x0000000000000003
```

Is the problem fixed by calling VMREAD slower?

### Suspecting exception handler

After reviewing serial log, looks like exception handler may have some
problems.

`20220103161912` (look for `{3,`):
```
I,32}{0,I,10}{3,i,32}{0,i,10}{3,I,32}{0,I,10}{1,i,32}{1,I,32}{0,i,10}{1,i,32}{0,
I,10}{1,I,32}{0,i,10}{0,I,10}{0,i,10}{0,I,10}{0,i,32}{0,I,32}{0,i,18}{3,i,32}
CPU(0x00): got quiesce signal...{3,I,32}{3,s}{2,i,0}{1,i,0}{2,p}{1,p}
CPU(0x00): all CPUs quiesced successfully.TV[0]:appmain.c:do_TV_HC_TEST:202:    
             CPU(0x00): test hypercall, ecx=0x0000001c

CPU(0x00): ending quiesce.
CPU(0x00): releasing quiesce lock.{2,P}{3,S}{1,P}{0,I,18}{2,I,0}{1,I,0}{3,i,32}{
0,i,32}{3,I,32}{0,I,32}
[03]: unhandled exception 3, halting!{1,i,32}{0,i,32}{1,I,32}{0,I,32}{1,i,32}
[03]: state dump follows...{1,I,32}
[03] CS:RIP 0x0010:0x0000000010206a3f with EFLAGS=0x0000000000000042{1,i,32}
```

`20220103213830` (look for `{1,`):
```
i,10}{1,i,32}{2,I,10}{0,i,32}{1,I,32}{2,i,32}{0,I,32}{2,I,32}{2,i,10}{2,I,10}{2,
i,10}{2,I,10}{2,i,10}{2,I,10}{2,i,10}{2,I,10}{2,i,10}{2,I,10}{0,i,32}{2,i,32}{0,
I,32}{2,I,32}{2,i,10}{2,I,10}{2,i,18}
CPU(0x02): got quiesce signal...{1,s}{3,s}{0,i,0}{0,p}
CPU(0x02): all CPUs quiesced successfully.TV[0]:appmain.c:do_TV_HC_TEST:202:    
             CPU(0x02): test hypercall, ecx=0x0000002a

CPU(0x02): ending quiesce.{0,P}
CPU(0x02): releasing quiesce lock.{3,S}{1,S}{0,I,0}{2,I,18}{1,i,32}{0,i,32}{2,i,
32}{1,I,32}{2,I,32}{0,I,32}
CPU(0x01): VM-ENTRY error, reason=0x80000021, qualification=0x0000000000000000{2
,i,10}
Guest State follows:{0,i,32}{2,I,10}
guest_CS_selector=0x0010
guest_DS_selector=0x0000{2,i,32}{0,I,32}{2,I,32}
```

Can see that the sequences are: i, I, s, S, i, I, error. Intercept logic is
read VMCS, i, handle, I, write VMCS. s and S means handling NMI exception.
So likely the exception happens during read VMCS or write VMCS.

Other 3 similar errors are `20220102120442` (same sequence), `20220103120431`
(not in verbose mode), and `20220103155652` (not in verbose mode)

Next steps:
* If print before read VMCS / after write VMCS, can the bug still be reproduced?
* In exception handlers, try to print the stack / RIP
* Randomly add MFENCEs
* Write an infinite loop to read VMCS, and inject NMI manually using QEMU

Modified code to `e20011121`

`20220104095611`: double fault in CPU 3
`grep -oE '\{[^\{\}]+\}' 20220104095611 | grep s` shows
```
{3,s,0x1020ed27} (right after VMWRITE)
{3,s,0x102069fb} (after first instruction of xmhf_parteventhub_arch_x86_64vmx_entry)
{3,s,0x1020ed5e} (right after VMREAD)
```

`20220104100947`: VMRESUME fails on CPU 2, can see the problem is again at
`0x1020ed5e` (right after VMREAD). Also there are 2 VMWRITE intercepts before
it. The next step is to print RAX and RBX, to see the arguments for VMREAD and
VMWRITE.

Modified code to `06034005a`

`20220104103003`: VMRESUME fail on CPU 2. Events are:
```
{2,s,0x1020e797,0x00000010257918,0000000000000000}
{2,s,0x1020e797,0x00000010257918,0000000000000000}
{2,s,0x1020ed6e,0x00000000002007,0000000000000000}
```

Can see that the return value for encoding `0x2007` is correct (should be 0) at
this point, but afterwards it becomes incorrect:
```
CPU(0x02): vcpu->vmcs.control_VM_exit_MSR_store_address_full=0x1900f000{3,I,32}
CPU(0x02): vcpu->vmcs.control_VM_exit_MSR_store_address_high=0x1900f000{3,i,32}
CPU(0x02): vcpu->vmcs.control_VM_exit_MSR_load_address_full=0x18fff000{0,I,32}
```

Modified code to `3b9dbe4c7`

`20220104104650`: triple fault in CPU 3.
`0x1020ed7e` is still immediately after the VMREAD instruction. Can see that
due to this instruction, `guest_IDTR_base` becomes `0xfffffe00000b2000`, but
should be `0xfffffe0000000000`. Also, rbx is correct before exception handler
returns:
```
CPU(0x00): got quiesce signal...{3,s,0x1020ed7e,0x00000000006818,0xfffffe0000000000}{2,s,0x1020ed7e,0x00000000002
811,0000000000000000}{1,i,0}{1,p}
CPU(0x00): all CPUs quiesced successfully.TV[0]:appmain.c:do_TV_HC_TEST:202:                 CPU(0x00): test hype
rcall, ecx=0x0000001a

CPU(0x00): ending quiesce.{2,S,0x1020ed7e,0x00000000002811,0000000000000000}
CPU(0x00): releasing quiesce lock.{2,i,32}{0,I,18}{2,I,32}{3,S,0x1020ed7e,0x00000000006818,0xfffffe0000000000}{0,
i,32}{2,i,32}{0,I,32}{2,I,32}{3,i,32}{0,i,32}{2,i,32}{0,I,32}{2,I,32}{3,I,32}{0,i,32}{2,i,32}{0,I,32}{2,I,32}{1,P
}{3,i,2}{1,I,0}
CPU(0x03): Unhandled intercept in long mode: 0x00000002{1,i,32}
        CPU(0x03): EFLAGS=0x00010286{1,I,32}{2,i,32}
        SS:RSP =0x0018:0xffffc90000120fa8{0,i,32}
        CS:RIP =0x0010:0xffffffff81c0006f{0,I,32}
        IDTR base:limit=0xfffffe00000b2000:0x0fff
        GDTR base:limit=0xfffffe00000b2000:0x007f
```

So it is most likely that `XtRtmIdtStub*` has a bug, or a hardware problem.
However, after reading `XtRtmIdtStub*` code, I don't see a problem.

Another question is how the incorrect value is read. Is rbx somewhere before?
By stepping through `xmhf_baseplatform_arch_x86_64vmx_getVMCS()` using GDB,
looks like in `xmhf_baseplatform_arch_x86_64vmx_read_field()`, `u64 value;` is
defined without initialized value. This variable is on the stack, and its value
is the value of previous reading. If `__vmx_vmwrite()` fails to assign to this
variable, this behavior can be observed.

The above guess is proven to be correct. Code is `3af22cc36`, run result in
`20220104114644`. Can see that CPU 0 and CPU 1 run into this problem.
`0x1020ed7e` is right after VMREAD instruction.

This can also be reproduced by sending NMI from guest using `./test 1000008`.
For example `20220104120913`. After changing code to `47ec0e592`, result in
`20220104121208`, can see that the problem is not related to quiesce code.

Test script using while loop
```
while true; do ./test 1000009; done
```

Tried to add a `nop` after `vmread`, still see error. Add 100 nop, still see
error.

Then tried around. `18c1fbb55` and `942ca94e4` are good. Also by comparing
`47ec0e592..18c1fbb55` can see that adding `HALT_ON_ERRORCOND(status != 3);`
to `__vmx_vmread()` seems to change the problem. Indeed, it did. The generated
code is (commit `20c71b31c`):
```
0000000010209902 <__vmx_vmread>:
static inline u32 __vmx_vmread(unsigned long encoding, unsigned long *value){
    10209902:   55                      push   %rbp
    10209903:   48 89 e5                mov    %rsp,%rbp
    10209906:   48 89 5d f8             mov    %rbx,-0x8(%rbp)
    1020990a:   48 89 7d e0             mov    %rdi,-0x20(%rbp)
    1020990e:   48 89 75 d8             mov    %rsi,-0x28(%rbp)
        __asm__ __volatile__("vmread %%rax, %%rbx \r\n"
    10209912:   48 8b 45 e0             mov    -0x20(%rbp),%rax
    10209916:   0f 78 c3                vmread %rax,%rbx
    10209919:   76 09                   jbe    10209924 <__vmx_vmread+0x22>
    1020991b:   48 c7 c2 01 00 00 00    mov    $0x1,%rdx
    10209922:   eb 07                   jmp    1020992b <__vmx_vmread+0x29>
    10209924:   48 c7 c2 00 00 00 00    mov    $0x0,%rdx
    1020992b:   48 89 55 f0             mov    %rdx,-0x10(%rbp)
    1020992f:   48 89 d9                mov    %rbx,%rcx
    10209932:   48 8b 45 d8             mov    -0x28(%rbp),%rax
    10209936:   48 89 08                mov    %rcx,(%rax)
        return status;
    10209939:   48 8b 45 f0             mov    -0x10(%rbp),%rax
}
    1020993d:   48 8b 5d f8             mov    -0x8(%rbp),%rbx
    10209941:   5d                      pop    %rbp
    10209942:   c3                      ret    
```

Note that this code accesses address below RSP. This can be confirmed using GDB
```
(gdb) p $rip
$1 = (void (*)()) 0x1020ed87 <__vmx_vmread+16>
(gdb) x/20i __vmx_vmread
   0x1020ed77 <__vmx_vmread>:	push   %rbp
   0x1020ed78 <__vmx_vmread+1>:	mov    %rsp,%rbp
   0x1020ed7b <__vmx_vmread+4>:	mov    %rbx,-0x8(%rbp)
   0x1020ed7f <__vmx_vmread+8>:	mov    %rdi,-0x20(%rbp)
   0x1020ed83 <__vmx_vmread+12>:	mov    %rsi,-0x28(%rbp)
=> 0x1020ed87 <__vmx_vmread+16>:	mov    -0x20(%rbp),%rax
   0x1020ed8b <__vmx_vmread+20>:	vmread %rax,%rbx
   0x1020ed8e <__vmx_vmread+23>:	jbe    0x1020ed99 <__vmx_vmread+34>
   0x1020ed90 <__vmx_vmread+25>:	mov    $0x1,%rdx
   0x1020ed97 <__vmx_vmread+32>:	jmp    0x1020eda0 <__vmx_vmread+41>
   0x1020ed99 <__vmx_vmread+34>:	mov    $0x0,%rdx
   0x1020eda0 <__vmx_vmread+41>:	mov    %rdx,-0x10(%rbp)
   0x1020eda4 <__vmx_vmread+45>:	mov    %rbx,%rcx
   0x1020eda7 <__vmx_vmread+48>:	mov    -0x28(%rbp),%rax
   0x1020edab <__vmx_vmread+52>:	mov    %rcx,(%rax)
   0x1020edae <__vmx_vmread+55>:	mov    -0x10(%rbp),%rax
   0x1020edb2 <__vmx_vmread+59>:	mov    -0x8(%rbp),%rbx
   0x1020edb6 <__vmx_vmread+63>:	pop    %rbp
   0x1020edb7 <__vmx_vmread+64>:	ret    
   0x1020edb8 <xmhf_baseplatform_arch_x86_64vmx_putVMCS>:	push   %rbp
(gdb) p $rbp - 0x20
$2 = (void *) 0x19963e68 <g_cpustacks+65128>
(gdb) p $rsp
$3 = (void *) 0x19963e88 <g_cpustacks+65160>
(gdb) 
```

However, if add `HALT_ON_ERRORCOND(status != 3);` back, version is `18c1fbb55`,
code becomes
```
0000000010209902 <__vmx_vmread>:
static inline u32 __vmx_vmread(unsigned long encoding, unsigned long *value){
    10209902:   55                      push   %rbp
    10209903:   48 89 e5                mov    %rsp,%rbp
    10209906:   48 89 5d f8             mov    %rbx,-0x8(%rbp)
    1020990a:   48 83 ec 30             sub    $0x30,%rsp
    1020990e:   48 89 7d d8             mov    %rdi,-0x28(%rbp)
    10209912:   48 89 75 d0             mov    %rsi,-0x30(%rbp)
        __asm__ __volatile__("vmread %%rax, %%rbx \r\n"
    10209916:   48 8b 45 d8             mov    -0x28(%rbp),%rax
    1020991a:   0f 78 c3                vmread %rax,%rbx
    1020991d:   76 09                   jbe    10209928 <__vmx_vmread+0x26>
    1020991f:   48 c7 c2 01 00 00 00    mov    $0x1,%rdx
    10209926:   eb 07                   jmp    1020992f <__vmx_vmread+0x2d>
    10209928:   48 c7 c2 00 00 00 00    mov    $0x0,%rdx
    1020992f:   48 89 55 e8             mov    %rdx,-0x18(%rbp)
    10209933:   48 89 d9                mov    %rbx,%rcx
    10209936:   48 8b 45 d0             mov    -0x30(%rbp),%rax
    1020993a:   48 89 08                mov    %rcx,(%rax)
        HALT_ON_ERRORCOND(status != 3);
    1020993d:   48 8b 45 e8             mov    -0x18(%rbp),%rax
    10209941:   48 83 f8 03             cmp    $0x3,%rax
    10209945:   75 21                   jne    10209968 <__vmx_vmread+0x66>
    10209947:   b9 e0 9d 25 10          mov    $0x10259de0,%ecx
    1020994c:   ba 7b 02 00 00          mov    $0x27b,%edx
    10209951:   be 33 9e 25 10          mov    $0x10259e33,%esi
    10209956:   bf 40 9e 25 10          mov    $0x10259e40,%edi
    1020995b:   b8 00 00 00 00          mov    $0x0,%eax
    10209960:   e8 2f 6e 02 00          call   10230794 <printf>
    10209965:   f4                      hlt    
    10209966:   eb fd                   jmp    10209965 <__vmx_vmread+0x63>
        return status;
    10209968:   48 8b 45 e8             mov    -0x18(%rbp),%rax
}
    1020996c:   48 8b 5d f8             mov    -0x8(%rbp),%rbx
    10209970:   c9                      leave  
    10209971:   c3                      ret    
```

Note the `sub    $0x30,%rsp` instruction.

Looks like the problem is related to the calling convention. As adviced by
<https://stackoverflow.com/a/28694274>, should use `-mno-red-zone` to prevent
this problem. Osdev also has an article:
<https://wiki.osdev.org/Libgcc_without_red_zone>

The fix should be adding `-mno-red-zone` to `CFLAGS`. Looks like after adding
this to `20c71b31c`, the generated code becomes good.

### Checking whether behaviors solved

See `### Checking behaviors` above, but now use `b04ca351a`

* Test 1: `pal_demo` + (SSH / NMI)
* Test 2: `pal_demo` * 2
* Test 3: `pal_demo` * 2 + (SSH / NMI)
* Test 4: NMI at c (speed of light)
* Test 5: `test` * 2

* x86 Debian
	* QEMU: (assume test 4 pass)
* x64 Debian
	* QEMU: (assume test 4 pass)
* x86 XMHF, x86 Debian
	* HP: ?
	* QEMU: test 1 fail (Failure A below), test 2 pass, test 4 pass, test 5 pass
* x64 XMHF, x86 Debian
	* HP: ?
	* QEMU: ?
* x64 XMHF, x64 Debian
	* HP: test 2 fail (Failure C), test 5 pass
	* QEMU: test 1 fail (Failure A below), test 2 pass, test 4 fail (Failure B),
	  test 5 pass

```
Test 1:
for i in {1..100}; do ./main $i $i; done
p qemu_inject_nmi.py 4444 0.1

Test 2:
for i in {1..100..2}; do ./main $i $i; done & for i in {0..100..2}; do ./main $i $i; done

Test 4:
p qemu_inject_nmi.py 4444 0.1

Test 5:
for i in {1..1000..2}; do ./test $i; done & for i in {0..1000..2}; do ./test $i; done
```

Failure A: while running PAL, an exception in guest happens.
```
VMEXIT-EXCEPTION:
control_exception_bitmap=0xffffffff
interruption information=0x80000b0e
errorcode=0x00000000
```

Failure B: Linux does not respond because of IRETQ infinite loop. At this
state, executing `iretq` will set RIP and RSP to exactly the current value,
so CPU 0 never returns to do normal work.
```
(gdb) x/i $rip
=> 0xffffffff81a01480 <asm_exc_nmi+240>:	iretq  
(gdb) x/6gx $rsp
0xfffffe000000dfd8:	0xffffffff81a01480	0x0000000000000010
0xfffffe000000dfe8:	0x0000000000000082	0xfffffe000000dfd8
0xfffffe000000dff8:	0x0000000000000018	Cannot access memory at address 0xfffffe000000e000
(gdb) 
```

Failure C: HP cannot receive new SSH connection, dmesg shows a CPU hang for 22s.
May actually be VMEXIT-EXCEPTION, if look through serial log (need to declutter
PAL output)

### Other ideas not tried

* Maybe EPT or XMHF's page table is not large enough?
	* Not for now, since physical addresses are always < 4GB
* Consider disabling other CPUs
	* May be useful
* Guess the reason for `vcpu->quiesced`, is it possible to be VM intercept then
  exception?
	* The actual answer is "yes". But this can only happen for CPU requesting
	  quiesce

### Conclusion

There are a few problems found in this bug:
* NMI blocking is handled incorrectly. This would cause deadlock
* GCC generated code accesses memory below RSP. This causes vmread get strange
  values

Other problems found:
* Exception handler accesses memory below RSP
* Reading VMCS in x86 will overwrite other fields

Other problems still existing:
* Failure A & C: when running PAL, when NMI for guest happens, get
  VMEXIT-EXCEPTION?
* Failure B: test 4 fails, related to NMI

### Attachments

Test results: `results.7z`
Git branch `xmhf64-nmi`: contains code used to debug this bug

## Fix

`0b37c866d..b04ca351a`
* Fix race condition in `xmhf_smpguest_arch_x86_64vmx_endquiesce()`
* Make access to `vcpu->quiesced` atomic in NMI handler (actually unnecessay,
  reverted later)
* Prevent accessing memory below RSP in xcph (likely unrelated)
* Prevent overwriting other fields when reading 32-bit VMCS fields
  (likely unrelated)
* Implement virtual NMI, unblock NMI when needed
* Add gcc `-mno-red-zone` to prevent vmread fail

