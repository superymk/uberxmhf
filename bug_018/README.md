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

# tmp notes

TODO: try to simply remove printf lock
TODO: or can try to use something like base64
TODO: collect results of multiple runs

TODO: for all log messages, print CPU ID
TODO: For behavior 2 - 5, may need to rewrite printf to print faster

