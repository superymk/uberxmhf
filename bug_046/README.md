# Support waking APs in DRT

## Scope
* HP, x64 XMHF, DRT
* Good: x86 XMHF
* Good: no DRT
* Git `92a9e94dc`

## Behavior
After fixing `bug_042`, DRT mode can enter runtime, but when BSP wakes up AP,
no AP shows up and BSP waits forever. Serial ends with following.
```
xmhf_xcphandler_arch_initialize: setting up runtime IDT...
xmhf_xcphandler_arch_initialize: IDT setup done.
Runtime: Re-initializing DMA protection...
Runtime: Protected SL+Runtime (10000000-1da07000) from DMA.
BSP: _mle_join_start = 0x1020bec0, _ap_bootstrap_start = 0x1020be70
BSP: joining RLPs to MLE with MONITOR wakeup
BSP: rlp_wakeup_addr = 0x0
Relinquishing BSP thread and moving to common...
BSP rallying APs...
BSP(0x00): My RSP is 0x000000001d987000
```

## Debugging

Note that when no DRT, BSP uses APIC to wake up APs. But now BSP uses something
else.

The difference can be seen in `xmhf_baseplatform_arch_x86_64vmx_wakeupAPs()`.
We need to compare behavior of x86 and x64 XMHF with DRT.

Can see that in x86 `rlp_wakeup_addr` should be `0xbb701d20`, but in x64 it is
`0x0`. We need to fix this.

Add more print statements in git `d3b19466a`, x64 DRT serial `20220302231320`.
Same flags added in x86 in git `2aba8feba`, x86 DRT serial `20220302231449`.
Can see that most other values make sense.

`sinit_mle_data` are all `0xbb7301c0`, but `sinit_mle_data->rlp_wakeup_addr`
are different. In GDB, can see that `p sizeof(sinit_mle_data_t)` is different.
In x86 is 148, in x64 is 152. This is likely caused by alignment issues.

`sinit_mle_data_t` is a structure defined in TXT MLE developer's guide section
"C.4 SINIT to MLE Data Format". So we add `__attribute__((packed))` and problem
solved.

Git `e187be88d`, serial `20220302232918`. This time see less output. This is
because machine is killed by some triple fault and serial output has not
reached debugger machine yet. We add some sleeps.

Git `026bddfd6`, serial `20220302233758`. Our guess is correct. Remove some
printf in git `7ba6af9ab`, serial `20220302234730`. Can see that write to
`rlp_wakeup_addr` causes the problem. Likely APs are up at this point.

Note that in above situations, after the error the machine reboots
automatically. APs are likely to start in `bplt-x86_64-smptrampoline.S`'s
`_ap_bootstrap_start()`. If we add an infinite loop there, may find something
useful. Another possibility is that the AP's start address of 0x10000 is not
passed correctly to TXT. But I think this is unlikely. See TXT MLE dev guide
"2.1 MLE Architecture Overview" -> "Table 1. MLE Header structure" and
`mle_hdr_t` in XMHF.

We can see that `mle_join` in `xmhf_baseplatform_arch_x86_64vmx_wakeupAPs()`
is defined in `_mle_join_start()` (assembly code). The assembly code points to
`_ap_clear_pipe()`. However adding a infinite loop at `_ap_clear_pipe()` does
not solve the problem.

Note that at this point BSP already has IDT set up, so I think the triple fault
likely comes from AP.

### Bochs

Debugging triple fault on HP is very difficult. Can we get a CPU debugger?

Looks like SENTER instructions are called "SMX instruction set", which is
related to Intel TXT. The documentation for SMX should be intel volume 2
"CHAPTER 6 SAFER MODE EXTENSIONS REFERENCE"
* Ref: <http://linasm.sourceforge.net/docs/instructions/smx.php>

Looks like Bochs supports SMX instructions, but not sure whether it supports
TXT. If it suppors, we can try Bochs because what we need to debug is low level
(e.g. the guest OS does not need to be up)
* Ref: <https://bochs.sourceforge.io/cgi-bin/lxr/source/docs-html/cpu_configurability.txt>

This can be a future direction.

### Review struct size

Git `e211ec67a`, we add a use of all structs of `_txt*.h` in the XMHF runtime
code. This way GDB recognizes the symbol. Then we use `txt1.gdb` to print
all their size. Find 2 mismatches, all caused by alignment problems
* `bios_data_t`: 36 (x86) vs 40 (x64)
	* Intel's specification is not aligned ("C.1 BIOS Data Format")
* `os_sinit_data_t`: 92 (x86) vs 96 (x64)
	* Intel's specification is not aligned ("C.3 OS to SINIT Data Format")

There is also another unaffected struct `sinit_mdr_t` in `_txt_heap.h`, but
the structure is defined by Intel, so we still add alignment
("Table 22. SINIT Memory Descriptor Record")

So we add alginment on these fields. Git `a08f148a1`, alginment problems fixed.
However, still see `BSP: rlp_wakeup_addr = 0xbb701d20` and system reboots.

# tmp notes

TODO: compare x86 and x64 logs (why is there `TXT.ERRORCODE=80000000`?)
TODO: does join data structure need to be 64 bits for x64?
TODO: try to use x86 version of `_ap_bootstrap_start()`
TODO: try `__getsec_wakeup()` (do not use `rlp_wake_monitor`), or use ACPI
TODO: how is BSP loaded? According to TXT docs AP should be similar
TODO: read TXT docs
TODO: try Bochs
TODO: how is `g_sinit_module_ptr` used?
TODO: study tboot
TODO: maybe write a TXT program from scratch
TODO: in an extreme case, set up tboot for x64 and rewrite it to xmhf

## Fix

`78f0eef8e..3ae6989ba`, `4d6217bd5..` (a08f148a1)
* Fix type size in `xmhf_baseplatform_arch_x86_64vmx_wakeupAPs()`
* Fix `x86_64` alignment problem for `_txt_heap.h`

