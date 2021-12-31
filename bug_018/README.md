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

## Fix

`0b37c866d..`
* Fix race condition in `xmhf_smpguest_arch_x86_64vmx_endquiesce()`

# tmp notes

TODO: try NMI in QEMU
TODO: print stack pointers to make sure no stack overflow occurs, also observe nested calls
TODO: may need to move `emhfc_putchar_lineunlock(emhfc_putchar_linelock_arg);` line in smpg, or add documentation

TODO: guess the reason for `vcpu->quiesced`, is it possible to be VM intercept then exception?
TODO: remove deadlock

TODO: consider disabling other CPUs

TODO: try to simply remove printf lock
TODO: or can try to use something like base64
TODO: collect results of multiple runs


TODO: for analyzing NMI, may be able to generate NMI in Linux with `perf`:
 <https://winddoing.github.io/post/98e04b95.html>

