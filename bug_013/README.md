# Debian x64 XMHF trustvisor demo not stable

## Scope
* x86 Debian 11, x64 XMHF, QEMU
* Looks like good: x86 Debian 11, x86 XMHF, QEMU

## Behavior
This bug is not always reproducible. When cannot reproduce, try to boot XMHF
and Debian again.

After selecting XMHF64 in GRUB, then Debian 11 Linux `5.10.0-10-686`, then run
the `pal_demo` a few times using `./main 1 2`, then suddenly virtual machine
stucks.

Serial shows:
```
...
TV[0]:scode.c:hpt_scode_switch_scode:917:          *** to scode ***
TV[0]:scode.c:hpt_scode_switch_scode:921:          got pal_running lock!
TV[0]:scode.c:hpt_scode_switch_scode:927:          scode return vaddr is 0x4265fb
TV[0]:scode.c:hpt_scode_switch_scode:930:          saved guest regular stack 0xbf8cd09c, switch to sensitive code stack 0xb7f2aff0
TV[0]:scode.c:scode_marshall:801:                  marshalling scode parameters!
TV[0]:scode.c:scode_marshall:806:                  parameter page base address is 0xb7f29010
TV[0]:scode.c:scode_marshall:820:                  params number is 2
TV[0]:scode.c:scode_marshall:831:                  copying param 1
TV[0]:scode.c:scode_marshall:852:                  scode_marshal copied metadata to params area
TV[0]:scode.c:scode_marshall:869:                  PM 1 is a pointer (size 4, value 0xbf8cd0ec)
TV[0]:scode.c:scode_marshall:831:                  copying param 0
TV[0]:scode.c:scode_marshall:852:                  scode_marshal copied metadata to params area
TV[0]:scode.c:scode_marshall:860:                  PM 0 is a integer (size 4, value 0x1)
TV[0]:scode.c:hpt_scode_switch_scode:949:          host stack pointer before running scode is 0xb7f2afe4
TV[0]:scode.c:hpt_scode_switch_scode:954:          change NPT permission to run PAL!

[
```

The last line of serial output is nondeterministic result

## Debugging

### Guess about stack overflow

After the machine stucks, print thread info in GDB shows:
```
(gdb) info th
  Id   Target Id                    Frame 
* 1    Thread 1.1 (CPU#0 [running]) 0x000000001020df47 in xmhf_baseplatform_arch_x86_64_smpinitialize_commonstart (vcpu=0x1022e350 <strncmp+118>)
    at arch/x86_64/bplt-x86_64-smp.c:180
  2    Thread 1.2 (CPU#1 [running]) 0x0000000010209be1 in xmhf_smpguest_arch_x86_64vmx_eventhandler_dbexception (vcpu=0x19956b90 <g_cpustacks+31632>, 
    r=0x1022a3cd <hptw_insert_pmeo_alloc+37>) at arch/x86_64/vmx/smpg-x86_64vmx.c:350
  3    Thread 1.3 (CPU#2 [running]) 0x0000000010209be7 in xmhf_smpguest_arch_x86_64vmx_eventhandler_dbexception (vcpu=0x1067e01e <g_vmx_ept_pml4_table_buffers+8222>, 
    r=0x1995af08 <g_cpustacks+48904>) at arch/x86_64/vmx/smpg-x86_64vmx.c:353
  4    Thread 1.4 (CPU#3 [running]) 0x0000000010209be1 in xmhf_smpguest_arch_x86_64vmx_eventhandler_dbexception (
    vcpu=0x1020e4e3 <xmhf_baseplatform_arch_x86_64vmx_allocandsetupvcpus+100>, r=0x0)
    at arch/x86_64/vmx/smpg-x86_64vmx.c:350
(gdb) 
```

Another run (serial stops at a different place):
```
(gdb) info th
  Id   Target Id                    Frame 
* 1    Thread 1.1 (CPU#0 [running]) 0x0000000010209be1 in xmhf_smpguest_arch_x86_64vmx_eventhandler_dbexception (
    vcpu=0x10208071 <xmhf_parteventhub_arch_x86_64vmx_intercept_handler+1478>, r=0x0)
    at arch/x86_64/vmx/smpg-x86_64vmx.c:350
  2    Thread 1.2 (CPU#1 [running]) 0x0000000010209be1 in xmhf_smpguest_arch_x86_64vmx_eventhandler_dbexception (vcpu=0x0, r=0xffffffff)
    at arch/x86_64/vmx/smpg-x86_64vmx.c:350
  3    Thread 1.3 (CPU#2 [running]) 0x0000000010209be1 in xmhf_smpguest_arch_x86_64vmx_eventhandler_dbexception (vcpu=0x1067e01e <g_vmx_ept_pml4_table_buffers+8222>, 
    r=0x1995af08 <g_cpustacks+48904>) at arch/x86_64/vmx/smpg-x86_64vmx.c:350
  4    Thread 1.4 (CPU#3 [running]) 0x000000001020df47 in xmhf_baseplatform_arch_x86_64_smpinitialize_commonstart (vcpu=0x1022e350 <strncmp+118>)
    at arch/x86_64/bplt-x86_64-smp.c:180
(gdb) 
```

Search for `vcpu=0x` above, looks like vcpu values are corrupted. Sometimes
can see `vcpu=0xffffffff`.

`xmhf_parteventhub_arch_x86_64vmx_entry` relies on vcpu to be at `128(%rsp)`.
Is it possible that nested interceptions happen? Or stack overflow?

### Getting normal value of vcpu

In `bplt-data.c`, can see that stacks and VCPU are fixed size and have
deterministic address. So the stack must be corrupted.

```
//runtime stacks for individual cores
u8 g_cpustacks[RUNTIME_STACK_SIZE * MAX_PCPU_ENTRIES] __attribute__(( section(".stack") ));

//VCPU structure for each "guest OS" core
VCPU g_vcpubuffers[MAX_VCPU_ENTRIES] __attribute__(( section(".data") ));
```

Since each stack element in x64 is 2 times larger than each element in x86,
we guess the stack size increase is around 2. So double the stack size should
mostly fix the problem.

### Detecting stack overflow

A possible way to detect runtime stack overflow is to add a canary at the end
of stack. However, for unknown reason it is not effective. Before stack
overflow is detected, another exception usually happens.

See `git.diff` for changes to code that implements the stack overflow detection.
It should be applied to commit `ae0968f19`.

## Fix

`e945f7b27..ae0968f19`
* Double runtime stack size for x64

