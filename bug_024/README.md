# HP running `pal_demo` not stable, likely Linux stuck

## Scope
* HP only
* Reproducible x64, not tested on x86

## Behavior
When running `bug_018`'s test 2 on HP, sometimes the machine gets stuck

For example, serial output is `20220105212903`

```
for i in {1..1000..2}; do ./main $i $i; done & for i in {0..1000..2}; do ./main $i $i; done
```

## Debugging

There is exactly one drop NMI message printed: `CPU(0x04): drop NMI`. Does
Linux crash etc when NMI is not delivered correctly?

Looks like not the case. code version is `144d32e7f`, serial output is
`20220106102426`. There is no dropped NMI. However, notice that the last PAL
is entered and never returned.

Also, only need to run 1 thread of pal demo to reproduce this problem. No need
to run 2 threads.

Change code to `4998e0ef0`, serial is `20220106104620`
```
CPU(0x04): endept,  0x00000000{1,I,31}{0,I,0}{1,i,31}{0,i,31}{4,I,48}{0,I,31}
{1,I,31}{4,i,31}{5,i,31}{1,i,31}{0,i,31}{4,I,31}{0,I,31}{4,i,31}{1,I,31}{5,I,31}
{1,i,31}{4,I,31}{1,I,31}{5,i,31}{4,i,18}{5,I,31}{1,i,31}{5,i,31}{1,I,31}
```

Can see that `{4,i,18}` is requesting quiesce. After serial stucks, CPU 0 and 1
are in guest mode, CPU 5 is handling intercept 31. A possible cause is the
location of `emhfc_putchar_lineunlock(emhfc_putchar_linelock_arg);` in
`xmhf_smpguest_arch_x86_64vmx_quiesce()`.

This problem is then reproduced by running `pal_demo`'s `test`. SSH to HP and
run `for i in {1..1000..2}; do ./test $i; done`, at the same time run
`while sleep 0.1; do ssh hp.lxy date; done`. Serial in `20220106110633`

```
{5,I,31}{4,i,31}{5,i,31}{4,I,31}{5,I,31}{4,i,10}{5,i,32}{4,I,10}{5,I,32}{4,i,10}
{5,i,32}{4,I,10}{5,I,32}{4,i,10}{1,i,10}{0,i,31}{1,I,10}{4,I,10}{0,I,31}{4,i,18}
{1,i,31}{0,i,31}{1,I,31}{0,I,31}{1,i,31}{0,i,31}{1,I,31}{0,I,31}{1,i,31}{0,i,31}
{1,I,31}{0,I,31}{1,i,31}
```

`{4,i,18}` is requesting quiesce, at point of stuck, CPU 1 is in hypervisor
mode, CPU 0 and 5 are in guest mode.

We are now sure that this problem is not related to TrustVisor. It is very
likely not related to injecting NMI to guest. It is likely related to the
deadlock in quiesce code.

So we change the quiesce code to release the printf lock after a significant
amount of time. The complete code is in `9fa5971fe`:
```
        {
            u32 lock_held = 1;
            for (u32 i = 0; g_vmx_quiesce_counter < (g_midtable_numentries-1); i++) {
                if (i == 0x10000000 && lock_held) {
                    lock_held = 0;
                    emhfc_putchar_lineunlock(emhfc_putchar_linelock_arg);
                }
            }
            if (lock_held) {
                emhfc_putchar_lineunlock(emhfc_putchar_linelock_arg);
            }
        }
```

After the change, test looks good.

The above code can only be considered a temporary fix. The long term way should
still be unlocking after `g_vmx_quiesce_counter < (g_midtable_numentries-1)`
becomes false.

Now port the change to `xmhf64` branch (previous changes in `xmhf64-nmi`).
Test looks good.

### Attachments
Test results: `results.7z`

## Fix

`716d95b93..592fbd12c`
* Fix deadlock in `xmhf_smpguest_arch_x86{,_64}vmx_quiesce()`

