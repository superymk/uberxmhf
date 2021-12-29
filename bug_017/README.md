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
b appmain.c:587

# 1B
b scode.c:199

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
* `copy_from_current_guest()`'s 4th argument (TODO)
* `copy_to_current_guest()`'s 4th argument (TODO)

TODO: change remaining

# tmp notes

TODO: for all log messages, print CPU ID
TODO: For behavior 2 - 5, may need to rewrite printf to print faster

```
time p test.py --qemu-image $QEMU_DIR/c3a.qcow2 --nbd 0 --work-dir /tmp/0 --test-num 100 --no-display && mv -n /tmp/0/result result_stack_1M/`date +%Y%m%d%H%M%S` && echo SUCCESS

# after boot

b scode.c

b appmain.c:587
c

```

