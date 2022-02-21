# ACPI problem in x64 DRT

## Scope
* HP, x64 XMHF, DRT
* Good: x86 XMHF
* Good: no DRT

## Behavior
See `### ACPI problem` in `bug_037`. Looks like some kind of triple fault
happens in `xmhf_baseplatform_arch_x86_64_acpi_getRSDP()`.

## Debugging

We use git `837bcbda2`'s `put_hex()` to print in assembly mode.
Git `df0ea08d5`, serial `20220219202043`. Can see that all segments are good.

Note that in the comparision mentioned in `bug_037`, the value of `rsdp` is
different. This means the stack address is different. In x64 without DRT this
is `0x1d9f7f84 <x_init_stack+3972>`. In x64 with DRT this is `0x0000ff7c`
(strange value). Did we mess up the stack?
* Later: looks like `bug_037`'s README is incorrect.

<del>In `sl-x86.lds.S` and `sl-x86_64.lds.S`, can see that the SL stack is
intended to be `0x0000ff7c`. We need to check x86 version's behavior</del>

<del>Git `0a8378392`, x64 drt serial `20220219204921`, x64 nodrt serial
`20220219205053`.</del>

I found that segment registers are not touched in `sl-x86_64.lds.S` at all.
Maybe we should change them. Commited `87a2a70a9`.

Things are becoming strange. For example, in git `6a2662cc1`, before the main
loop in `_acpi_computetablechecksum()` everything looks fine. Serial ends with:
```
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 1998 0x0000ff7c 0x00000014
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 1999 0x0000ff7c 0x00000014
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:69 0x0000ff7c
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:69 0x0000ff7d
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:69 0x0000ff7e
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:69 0x0000ff7f
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:69 0x0000ff80
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:69 0x0000ff81
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:69 0x0000ff82
```

In git `82ff960f1`, the situation is similar. Looks like
`xmhf_baseplatform_arch_flat_copy()` is accessing the physical memory
correctly.

Git `2842b4265`, looks like even adding wbinvd does not work.

Git `c03c4cff4`, serial below. Now looks like the problem happens before
`_acpi_computetablechecksum()`'s man loop. Also, the problem seems
deterministic (tried 3 times, the serial output ends with the same thing):
```
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 32 0x0000ff7c 0x00000014
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 33 0x0000ff7c 0x00000014
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 34 0x0000ff7c 0x00000014
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 35 0x0000ff7c 0x0
```

Git `bc82aa13f`, only changed a constant from previous commit, but the result
changes. The result is still deterministic:
```
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 47 0x0000ff7c 0x00000014
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 48 0x0000ff7c 0x00000014
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:66 49 0x0000ff7c 0x00000014
FILE:LINE a
```

I think it is better to set up IDT and see whether there is an exception.

### TXT reference

There should be a manual called "Intel MLE Developer's Guide
(Dec. 2009 revision)", but I have trouble finding the official version.
<https://kib.kiev.ua/x86docs/Intel/TXT/315168-006.pdf> has an unoffical
version.

Found that `acm_hdr_t` should be packed. Committed `81563bccd`.

Other resources
* <https://i.blackhat.com/briefings/asia/2018/asia-18-Seunghun-I_Dont_Want_to_Sleep_Tonight_Subverting_Intel_TXT_with_S3_Sleep.pdf>

### TLB

During review of `set_mtrrs_for_acmod()`, looks like the code may not flush
TLBs under if CR4.PGE = 0 before calling this function. However this should be
a minor issue

### Serial delay

Git `0e16facff`, found that serial ends with
```
000fc6c0:  662e 8b36 35c6 662e 0fb7 0e3f c683 f900
000fc6d0:  7411 4967 6626 8b1e 663b d874 2b66 83c6
000fc6e0:  0ce2 f066 2e8b 362d c666 2e
```

Likely `INVPCID` caused an exception (well, maybe I wrote it incorrectly).
Now I suspect that the serial output has not been sent to the debugger machine,
and the debugee machine gets reset.

Git `5fde40e63`, serial ends with following. The change is to add a `sleep(1)`
after each printf(). Can see that the hypothesis about delayed serial output is
valid. The problem is after ACPI get RSDP.
```
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:100 0x0000ff8d
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:100 0x0000ff8e
FILE:LINE arch/x86_64/bplt-x86_64-acpi.c:100 0x0000ff8f
DONE
```

In the mean time, found and fixed `bug_045`.

After some reductions, git `1e7c45149`, serial ends with following. Can see
that the problem is in `xmhf_sl_arch_sanitize_post_launch()`
```
xmhf_baseplatform_arch_x86_64_pci_initialize: Done with PCI bus enumeration.
FILE:LINE arch/x86_64/bplt-x86_64.c:129
FILE:LINE sl.c:228
```

### Look into `xmhf_sl_arch_sanitize_post_launch()`

Git `ea1615c2b`, serial following. Can see the problem is in
`get_os_mle_data_start()`.
```
FILE:LINE arch/x86_64/sl-x86_64.c:197
FILE:LINE arch/x86_64/sl-x86_64.c:200
SL: txt_heap = 0x00000000
FILE:LINE arch/x86_64/sl-x86_64.c:202
```

Note `txt_heap = 0x00000000` above. In x86 this should be `0xbb720000`

The problem is caused by:
* `xmhf_sl_arch_sanitize_post_launch()` is called
	* `get_txt_heap()` returns `heap = 0xfffffff0000000` (-256M)
	* `get_os_mle_data_start()` accesses `*heap`, which results in page fault

Can see that `get_txt_heap()` calls `read_pub_config_reg()`, which calls
`read_config_reg()`, which is implemented using segment FS (only valid in x86;
in x64 should use PA = VA + 256M).

We should review use of FS and GS. Committed `7dd9a985d`.

Git `102a34fda`, serial below. Can see that `txt_heap` is fixed.
```
SL: txt_heap = 0xbb720000
FILE:LINE arch/x86_64/sl-x86_64.c:200
FILE:LINE arch/x86_64/sl-x86_64.c:203
SL: os_mle_data = 0xab720034
FILE:LINE arch/x86_64/sl-x86_64.c:205
FILE:LINE arch/x86_64/sl-x86_64.c:211
```

However after line 211 still see triple fault. Should be in `restore_mtrrs()`.

Git `c7e7d821c`, serial following. The problem is in `set_all_mtrrs()`
(called by `restore_mtrrs()`)
```
FILE:LINE arch/x86_64/vmx/bplt-x86_64vmx-mtrrs.c:520
FILE:LINE arch/x86_64/vmx/bplt-x86_64vmx-mtrrs.c:524
```

`set_all_mtrrs(false)` wants to change `IA32_MTRR_DEF_TYPE` from 0x800 to 0.
This causes an error. We should test it on other configurations (x64 no drt and
x86)

### Legality of disabling MTRRs

Linux provides a way to modify MTRR through `/proc/mtrr`
<https://www.kernel.org/doc/Documentation/x86/mtrr.txt>

Git `43576ebab`, x64 no DRT shows `FILE:LINE sl.c:264 0x00000000 00000c00`,
x64 DRT shows `FILE:LINE sl.c:264 0x00000000 00000800`. Both can successfully
disable MTRR and enable MTRR before call to
`xmhf_sl_arch_sanitize_post_launch()`.

Git `1a7568211`, looks like the problem may be related to the use of
`wrmsr64()`. It may have different behavior from `wrmsr()`.

Can see that the problem is indeed `wrmsr64()`. For x86, "A" in asm means
EDX:EAX
```
static inline void wrmsr64(uint32_t msr, uint64_t newval)
{
1020d205:       55                      push   %ebp
1020d206:       89 e5                   mov    %esp,%ebp
1020d208:       83 ec 08                sub    $0x8,%esp
1020d20b:       8b 45 0c                mov    0xc(%ebp),%eax
1020d20e:       89 45 f8                mov    %eax,-0x8(%ebp)
1020d211:       8b 45 10                mov    0x10(%ebp),%eax
1020d214:       89 45 fc                mov    %eax,-0x4(%ebp)
    __asm__ __volatile__ ("wrmsr" : : "A" (newval), "c" (msr));
1020d217:       8b 45 f8                mov    -0x8(%ebp),%eax
1020d21a:       8b 55 fc                mov    -0x4(%ebp),%edx
1020d21d:       8b 4d 08                mov    0x8(%ebp),%ecx
1020d220:       0f 30                   wrmsr  
}
1020d222:       90                      nop
1020d223:       c9                      leave  
1020d224:       c3                      ret    
```

For x64, looks like "A" in asm means only RAX, which is not what we want,
because EDX now becomes undefined.
```
static inline void wrmsr64(uint32_t msr, uint64_t newval)
{
    1020e5f0:   55                      push   %rbp
    1020e5f1:   48 89 e5                mov    %rsp,%rbp
    1020e5f4:   48 83 ec 10             sub    $0x10,%rsp
    1020e5f8:   89 7d fc                mov    %edi,-0x4(%rbp)
    1020e5fb:   48 89 75 f0             mov    %rsi,-0x10(%rbp)
    __asm__ __volatile__ ("wrmsr" : : "A" (newval), "c" (msr));
    1020e5ff:   48 8b 45 f0             mov    -0x10(%rbp),%rax
    1020e603:   8b 55 fc                mov    -0x4(%rbp),%edx
    1020e606:   89 d1                   mov    %edx,%ecx
    1020e608:   0f 30                   wrmsr  
}
    1020e60a:   90                      nop
    1020e60b:   c9                      leave  
    1020e60c:   c3                      ret    
```

Committed `5a3bee662` to fix `rdmsr64()` and `wrmsr64()`. In `8589df17b`, fixed
other occurrences (e.g. `rdtsc64()`).

Git `6f1bf915e`, serial following.
```
FILE:LINE arch/x86_64/vmx/bplt-x86_64vmx-mtrrs.c:623 0x0000000000000000
FILE:LINE arch/x86_64/vmx/bplt-x86_64vmx-mtrrs.c:625 0x0000000000000000
FILE:LINE arch/x86_64/vmx/bplt-x86_64vmx-mtrrs.c:503
FILE:LINE arch/x86_64/vmx/bplt-x86_64vmx-mtrrs.c:511
```

Git `bfa5f9010`, serial `20220220205824`. Find 2 strange things
* print debugging in `save_mtrrs()` does not work.
	* This is because the bootloader will call x86 version, not x64 version.
* `saved_state->num_var_mtrrs` is a strange value. Maybe structs in
  `_txt_mtrrs.h` are not the same between x86 and x64?
	* Verified using gdb below

```
$ gdb xmhf/src/xmhf-core/xmhf-secureloader/sl_syms.exe

(gdb) p sizeof(mtrr_state_t)
$1 = 272
(gdb) p sizeof(mtrr_def_type_t)
$2 = 8
(gdb) p sizeof(mtrr_physbase_t)
$3 = 8
(gdb) p sizeof(mtrr_physmask_t)
$4 = 8
(gdb) 

$ gdb xmhf/src/xmhf-core/xmhf-bootloader/init_syms.exe 

(gdb) p sizeof(mtrr_state_t)
$1 = 268
(gdb) p sizeof(mtrr_def_type_t)
$2 = 8
(gdb) p sizeof(mtrr_physbase_t)
$3 = 8
(gdb) p sizeof(mtrr_physmask_t)
$4 = 8
(gdb) 
```

The problem is alignment. Most fields in the struct are 8 bytes, but
`int num_var_mtrrs;` is 4 bytes. in x86 there will be no padding because max
alignment is 4. But in x64 there will be 4 bytes of padding.

Git `5723924ae`, serial `20220220212355`. Can see that there are still problems
in passing the arguments. From gdb now `sizeof(mtrr_state_t) = 272` for both
x86 and x64. The problem is in `os_mle_data_t`:
```
$ gdb xmhf/src/xmhf-core/xmhf-secureloader/sl_syms.exe
(gdb) p sizeof(os_mle_data_t)
$1 = 65832
(gdb) 

$ gdb xmhf/src/xmhf-core/xmhf-bootloader/init_syms.exe 
(gdb) p sizeof(os_mle_data_t)
$1 = 65820
(gdb) 
```

By reviewing code, can see that the problem is the same: uint32 followed by
uint64.

Final git is `92a9e94dc`, now x64 SL can jump to runtime. (But problem with
booting APs in runtime.)

### Untried ideas

* Should `xmhf_sl_arch_sanitize_post_launch()` be called before
  `xmhf_baseplatform_initialize()`?
	* Actually not needed
* Set up IDT
	* Survived with print debugging
* Review `set_mtrrs_for_acmod()`
	* Looks like no longer needed
* Read Intel MLE Developer's Guide
	* Skimmed through it. Very helpful reading it.
* Study tBoot
	* Maybe in the future
* What if in `xmhf_baseplatform_arch_x86_64_acpi_getRSDP()` we jump straight to
  the address (0xfc600) containing RSDP?
	* Not needed (because bug is not related to ACPI RSDP)

## Fix

`da97a50e1..81563bccd`, `72356c5a7..92a9e94dc`
* Reset segment descriptors in x64 sl entry
* Add attribute packed to `_txt_acmod.h`
* Change use of FS in x64 sl code to use of paging
* Fix use of "A" to represent EDX:EAX in x64 code
* Change data structures to be the same for x86 and x64

