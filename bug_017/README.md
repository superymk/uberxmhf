# x86 `pal_demo` in x64 Debian x64 XMHF is not stable

## Scope
* bad: x86 `pal_demo`, x64 Debian, x64 XMHF, QEMU and HP
* bad: x86 `pal_demo`, x86 Debian, x64 XMHF, HP
* good: x86 `pal_demo`, x86 Debian, x64 XMHF, QEMU
* good: x86 `pal_demo`, x86 Debian, x86 XMHF, HP

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
|    |   | Debian 11 x64    |pal_demo x86| bad              | bad              |
+----+---+------------------+------------+------------------+------------------+
```

## Behavior

After booting Linux, run `for i in {1..100}; do ./main $i $i; done`.
Then the system will stuck. AMT behavior sometimes similar to the stack
overflow bug `bug_013`.

Behavior 1 (on (x64 Debian, x64 XMHF, QEMU and HP), more frequent):
```
TV[0]:appmain.c:tv_app_handleintercept_hwpgtblviolation:584: CPU(0x00): gva=0xf7f88006, gpa=0xd357006, code=0x4
TV[0]:scode.c:hpt_scode_npf:1190:                  CPU(00): nested page fault!(rip 0xf7f88006, gcr3 0x452b801, gpaddr 0xd357006, errorcode 4)
TV[0]:scode.c:scode_in_list:199:                   no matching scode found for gvaddr 0xf7f88006!
TV[3]:scode.c:hpt_scode_npf:1229:                  incorrect regular code EPT configuration!
TV[0]:appmain.c:tv_app_handleintercept_hwpgtblviolation:587: FATAL ERROR: Unexpected return value from page fault handling
```

Behavior 2 (on (x64 Debian, x64 XMHF, QEMU), similar to stack overflow bug):
```
TV[0]:pt.c:scode_lend_section:209:                 Mapping from 00000000f7f09000 to 00000000f7f09000, size 4096, pal_prot 3
TV[0]:pt.c:scode_lend_section:236:                 got pme 000000000cfba867, level 1, type 2
TV[0]:pt.c:scode_lend_section:268:                 got reg gpt prots:0x7, user:1
TV[0]:pt.c:scode_lend_section:209:                 Mapping from 00000000f7f08000 to 00000000f7f08000, size 4096, pal_prot 3
TV[0]:pt.c:scode_lend_section:236:                 got pme 000000000cfbb867, level 1, type 2
TV[0]:pt.c:scode_lend_section:268:                 got reg gpt prots:0x7, user:1
TV[0]:pt.c:scode_lend_section:209:                 Mapping from 00000000f7f07000 to 00000000f7f07000, size 4096, pal_prot 3
TV[0]:pt.c:scode_lend_section:236:                 got pme 000000000cfbc867, level 1, type 2
TV[0]:pt.c:scode_lend_section:268:                 got reg gpt prots:0x7, user:1
TV[2]:scode.c:scode_register:623:                  skipping scode_clone_gdt
TV[0]:scode.c:scode_register:647:                  generated pme for return gva address 4: lvl:1 5
TV[0]:pages.c:pagelist_get_page:72:                num_used:4 num_alocd:127
TV[0]:pages.c:pagelist_get_page:72:                num_used:5 num_alocd:127

VM-ENTRY erro
```

Behavior 3 (on (x86 Debian, x64 XMHF, HP)):
```
TV[0]:scode.c:scode_unmarshall:1021:               unmarshalling scode parameters!
TV[0]:scode.c:scode_unmarshall:1026:               parameter page base address is 0xb7f0a010
TV[0]:scode.c:scode_unmarshall:1036:               params number is 2
TV[0]:scode.c:scode_unmarshall:1074:               PM 0 is a pointer (size 16, addr 0xbfa5845c)
TV[0]:scode.c:scode_unmarshall:1056:               skip an integer parameter!
TV[0]:scode.c:hpt_scode_switch_regular:1134:       change NPT permission to exit PAL!
TV[0]:scode.c:hpt_scode_switch_regular:1140:       switch from scode stack 0xb7f0bfe8 back to regular stack 0xbfa5840c
TV[0]:scode.c:hpt_scode_switch_regular:1147:       stack pointer before exiting scode is 0xbfa58410
TV[0]:scode.c:hpt_scode_switch_regular:1154:       released pal_running lock!

CPU(0x0
```

Behavior 4 (on (x86 Debian, x64 XMHF, HP)):
```
TV[0]:scode.c:scode_unregister:740:                zeroing section 1
TV[0]:scode.c:scode_unregister:740:                zeroing section 2
TV[0]:scode.c:scode_unregister:740:                zeroing section 3

CPU(0x00): Unhandled intercept: 0x00000002
	CPU(0x00): EFLAGS=0x00210206
	SS:ESP =0x0068:0xd9aedf50
```

## Debugging

### Adding stack size

Since sometimes output is similar to `bug_013`, we add stack size.

In `xmhf/src/xmhf-core/include/arch/x86_64/_configx86_64.h`, change
`RUNTIME_STACK_SIZE` to 1048576 (1 MiB).

Tested in (x64 Debian, x64 XMHF, QEMU). Still see Behavior 1, but looks like
less frequent? Also other behaviors less frequent?

### Automated testing

Wrote a Python script `test.py` to automatically test using QEMU, and then
write result (number of tests passed, complete serial output) to a file.
Each test run lasts around 1 min.

After changing stack size to 1MiB, looks like only Behavior 1 happens. So
likely `bug_013` did not increase stack size by an enough amount.

Also use automated testing to test 32K stack size.

Sample test command:
```sh
time p test.py --qemu-image $QEMU_DIR/c3a.qcow2 --nbd 0 --work-dir /tmp/0 --test-num 100 --no-display && mv -n /tmp/0/result result_stack_1M/`date +%Y%m%d%H%M%S` && echo SUCCESS
```

View the last line of serial output to categorize result
* `zeroing section 3`: all tests pass
* `...tv_app_handleintercept_hwpgtblviolation:587: FATAL ERROR: ...`: behavior 1
* `num_used:5 num_alocd:127`: behavior 2
* `VM-ENTRY erro`: can be behavior 2 / 3 / 5, check earlier lines
* `...:1154:       released pal_running lock!`: likely behavior 3
* `GDTR base:limit=0xfffffe0000077000:0x007f`: likely behavior 3 (intercept 2)
* `...954:          change NPT permission to run PAL!`: behavior 5

Can use `for i in *; do tail -n 1 $i; done | sort | uniq -c` to count
occurrence of each line.

Test result:
| Index | Stack | Test Name       | Pass | Behavior 1 | 2  | 3  | 4  | 5  |
|-------|-------|-----------------|------|------------|----|----|----|----|
| 1     | 1M    | `stack_1M`      | 2    | 12         |    |    |    | 1  |
| 2     | 32K   | `stack_32K`     | 4    | 53         | 2  |    |    | 2  |
| 3     | 32K   | `stack_32K_c1a` | 1    | 39         | 2  | 2  |    | 2  |
| 4     | 64K   | `stack_64K`     | 4    | 58         |    | 1  |    | 1  |

I guess that there are two or more problems. Behavior 2 - 4 are stack overflow.
Behavior 1 is another bug. Maybe behavior 5 is another bug.

Now should focus on behavior 1. This one is more reproducible & stable.

### New behavior 5

Behavior 5 sample output
```
TV[0]:scode.c:scode_marshall:831:                  copying param 1
TV[0]:scode.c:scode_marshall:852:                  scode_marshal copied metadata to params area
TV[0]:scode.c:scode_marshall:869:                  PM 1 is a pointer (size 4, value 0xffed984c)
TV[0]:scode.c:scode_marshall:831:                  copying param 0
TV[0]:scode.c:scode_marshall:852:                  scode_marshal copied metadata to params area
TV[0]:scode.c:scode_marshall:860:                  PM 0 is a integer (size 4, value 0x1)
TV[0]:scode.c:hpt_scode_switch_scode:949:          host stack pointer before running scode is 0xf7f9ffe4
TV[0]:scode.c:hpt_scode_switch_scode:954:          change NPT permission to run PAL!

VM-ENTRY error: reason=0x80000021, qualification=0x0000000000000000
Guest State follows:
```

### Two types of behavior 1

Behavior 1A
```
TV[0]:pages.c:pagelist_get_page:72:                num_used:5 num_alocd:127
TV[0]:appmain.c:tv_app_handleintercept_hwpgtblviolation:584: CPU(0x03): gva=0xf7fb4006, gpa=0x1decb006, code=0x4
TV[0]:scode.c:hpt_scode_npf:1190:                  CPU(03): nested page fault!(rip 0xf7fb4006, gcr3 0x3975803, gpaddr 0x1decb006, errorcode 4)
TV[0]:scode.c:scode_in_list:199:                   no matching scode found for gvaddr 0xf7fb4006!
TV[3]:scode.c:hpt_scode_npf:1229:                  incorrect regular code EPT configuration!
TV[0]:appmain.c:tv_app_handleintercept_hwpgtblviolation:587: FATAL ERROR: Unexpected return value from page fault handling
```

Behavior 1B
```
TV[0]:scode.c:scode_unregister:740:                zeroing section 3
TV[0]:appmain.c:tv_app_handleintercept_hwpgtblviolation:584: CPU(0x03): gva=0x9e0a5000, gpa=0x1e0a5000, code=0x2
TV[0]:scode.c:hpt_scode_npf:1190:                  CPU(03): nested page fault!(rip 0xa10d3097, gcr3 0x5c22001, gpaddr 0x1e0a5000, errorcode 2)
TV[3]:scode.c:hpt_scode_npf:1193:                  EU_CHK( hpt_error_wasInsnFetch(vcpu, errorcode)) failed
TV[0]:appmain.c:tv_app_handleintercept_hwpgtblviolation:587: FATAL ERROR: Unexpected return value from page fault handling
```

### Setting break points

After booting, enter XMHF address space
```gdb
symbol-file xmhf/src/xmhf-core/xmhf-runtime/runtime.exe
hb xmhf_parteventhub_arch_x86_64vmx_intercept_handler
c
d
```

Set break point for Behavior 1
```
# 1A and 1B
# eu_trace("FATAL ERROR: Unexpected return value from page fault handling");
b appmain.c:587

# 1B
# eu_trace("no matching scode found for gvaddr %#x!", gvaddr);
b scode.c:199 if gvaddr != 4

c
```

### Debugging behavior 1

For 1A, always `violationcode = 0x4`, page offset = 0x006.

For 1B, mostly `violationcode = 0x2`, page offset = 0x000.
`result_stack_64K/20211228203018` is an exception.

### Integer size problem

After randomly browsing through code, found in `trustvisor.h` that
`struct tv_pal_section`'s definitions are using `uint32_t`. Should it be
changed to `uint64_t`?

This field is guest virtual address, and is compared with guest RIP. In
compatibility mode this should be OK. But better to change the sizes for the
future.

Note that currently `pal_demo` is compiled in x86, but TrustVisor is compiled
in x64. This difference will cause problems in parameter passing. Need to take
care. Then I realized that `trustvisor.h` cannot contain `uintptr_t`, so need
to discard my changes in `uintptr1.diff` on `c23d4ff6c`

Changed list
* `struct tv_pal_section` -> `start_addr`, `page_num` (done)
* `struct tv_pal_param` -> `size` (done)
* `scode_in_list()`'s second argument (done)
* `whitelist_entry_t` -> `gpmp`, `gssp`, `gpm_size`, `gss_size`, ... (done)
* `hpt_scode_npf()`'s 2nd argument (done)
* `copy_from_current_guest()`'s 4th argument (done)
* `copy_to_current_guest()`'s 4th argument (done)

### Continue debugging

In 1B `scode_in_list()`, set a break point and print variables.
`gcr3 = 0x55bb805, whitelist[0].gcr3 = 0x55bb803`.

Lower 12 bits in CR3 are PCID. When CR4 != 0 looks like this field is ignored?
See multiple answers in <https://stackoverflow.com/questions/20155304/>.

Also useful:
<https://kernelnewbies.org/Linux_4.14#Longer-lived_TLB_Entries_with_PCID>.

Can see in logs that in x86, `CR3[0:12]` is always `0x000`, but in x64,
it is one of `0x80[1-6]` (all values are equally likely)

In Intel's volume 3, "2.5 CONTROL REGISTERS" says "If PCIDs are enabled, CR3
has a format different from that illustrated in Figure 2-7. See Section 4.5,
'4-Level Paging and 5-Level Paging.'". Then in section 4.5, Table 4-12 shows
that most bits in `CR3[0:12]` are ignored. Only bit 3 and 4 (PWT and PCD) are
used.

For now, should be able to ignore all of `CR3[0:12]`.

### GDB snippet for performing page walk

(This snippet assumes PML4 = 0)
```gdb
set $a = vcpu->vmcs.guest_RSP
set $c = vcpu->vmcs.guest_CR3
set $p0 = $c & ~0xfff
set $p1 = (*(long*)($p0)) & ~0xfff | ((($a >> 30) & 0x1ff) << 3)
set $p2 = (*(long*)($p1)) & ~0xfff | ((($a >> 21) & 0x1ff) << 3)
set $p3 = (*(long*)($p2)) & ~0xfff | ((($a >> 12) & 0x1ff) << 3)
set $p4 = (*(long*)($p3)) & ~0xfff
set $b = $p4 | ($a & 0xfff)
```

### Fixing behavior 1

Use `cr3_mask2.diff` as a quick way to test whether ignoring `CR3[0:12]` will
fix behavior 1. Looks like the answer is yes.

For other behaviors, having an incomplete printf line is related to
`xmhf_smpguest_arch_x86svm_quiesce()`. When interrupt is delivered to the CPU,
the CPU is printing a line and holding the serial port lock. So the screen
freezes.

After commit `324855612`, success rate of testing 100 `pal_demo`s increases a
lot in QEMU. So looks like behavior 1 is fixed.

However, on HP, other behaviors still exist.

### Quiesce code analysis

In `peh-x86_64vmx-main.c`, there are 3 locations where hypervisor wants to
communicate with guest. `xmhf_smpguest_arch_x86_64vmx_quiesce(vcpu);` enters
critical section and `xmhf_smpguest_arch_x86_64vmx_endquiesce(vcpu);` leaves
critical section.

`xmhf_smpguest_arch_x86_64vmx_quiesce()` send NMI to all other CPUs and wait
until these CPUs enter
`xmhf_smpguest_arch_x86_64vmx_eventhandler_nmiexception()`.

If other CPUs receive NMI when in hypervisor mode (i.e. VMX root operating),
it will call `xmhf_xcphandler_arch_hub`, then
`xmhf_smpguest_arch_x86_64_eventhandler_nmiexception`, then
`xmhf_smpguest_arch_x86_64vmx_eventhandler_nmiexception`. If in guest mode
(i.e. VMX non-root operating), it will call
`xmhf_parteventhub_arch_x86_64vmx_intercept_handler`, then
`xmhf_smpguest_arch_x86_64vmx_eventhandler_nmiexception`.

The problem is that if another process is holding the printf lock and is NMI
interrupted, there will be a deadlock.

A few possible solutions:
* If NMI is like a Linux signal, we can mask it during critical section of
  printf. However, it looks like not the case.
* We can check the status of the lock in
  `xmhf_smpguest_arch_x86_64vmx_quiesce()`. If locked, force it to unlock.
  However, it is difficult to check whether a lock is deadlocked, so give up.
* We can acquire the printf lock in `xmhf_smpguest_arch_x86_64vmx_quiesce()`
  before sending NMI interrupts. (from `bug_018`, this can still cause
  deadlock if `xmhf_smpguest_arch_x86_64vmx_quiesce()` raises an exception).
* For strange behaviors like this, can remove the lock on printf.
  Can encode each ASCII character (16 * x + y) on CPU z with two characters
  (only need 64 possibilities for 4 CPUs): (16 * z + x, 16 * z + y)
  This should work as long as parallel printing each character works on serial
  port.

Will handle other behaviors in `bug_018`

### Attachments

Test result of different stack sizes: `results.7z`
* Contains 4 directories: `result_stack_1M`, `result_stack_32K`,
  `result_stack_32K_c1a`, `result_stack_64K`

## Fix

`c23d4ff6c..36fc963ae`
* Change `scode.c` to extend integer sizes to 64-bits
* Consider CR3 as match if address bits match (allow lower bits to be different)
* Increase runtime stack size from 32K to 64K (just in case)

