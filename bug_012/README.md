# x64 Debian x64 XMHF cannot run trustvisor demo

## Scope
* bad: x64 Debian 11, x64 XMHF, QEMU
* good: x86 Debian 11, x64 XMHF, QEMU

## Behavior
Run `pal_demo` (x86 binary) in x64 Debian x64 XMHF, see assertion error
```
TV[0]:scode.c:scode_register:508:                  cpu 0 setting root pm from 0x1067c000 to 0x1067d000
TV[0]:scode.c:scode_register:508:                  cpu 1 setting root pm from 0x1067d000 to 0x1067d000
TV[0]:scode.c:scode_register:508:                  cpu 2 setting root pm from 0x1067e000 to 0x1067d000
TV[0]:scode.c:scode_register:508:                  cpu 3 setting root pm from 0x1067f000 to 0x1067d000
TV[0]:scode.c:scode_register:525:                  *** scode register ***
TV[0]:scode.c:scode_register:529:                  CPU(0x01): add to whitelist,  scode_info 0x56ba15b0, scode_pm 0x56ba1630, gventry 0xf7f2f006
.../xmhf/src/libbaremetal/libxmhfutil/hpto.c:107: assert failed: hpt_pme_is_page(pmeo->t, pmeo->lvl, pmeo->pme)
```

## Debugging

### Setting breakpoint

Looks like we can set a break point in `scode_register()`.
After Linux completely boots, set a hardware breakpoint. GDB script:
```
symbol-file xmhf/src/xmhf-core/xmhf-runtime/runtime.exe
hb *scode_register
c
```

Then run `./main 1 2` in QEMU, will hit break point

### Call stack

By setting a break point at `hpt_pmeo_va_to_pa`, can see call stack at the time
of assertion error.
```
scode_register:540
	hptw_va_to_pa
		hpt_pmeo_va_to_pa
			assert(...)
```

In `hptw_va_to_pa()`, `ctx->t == HPT_TYPE_PAE`. But it should probably be
`HPT_TYPE_LONG`.

This value is obtained in `hpt_emhf_get_guest_hpt_type()`. Call stack:
```
scode_register:521
	hptw_emhf_checked_guest_ctx_init_of_vcpu
		hpt_emhf_get_guest_hpt_type
	hptw_emhf_checked_guest_ctx_init
```

Also `hpt_emhf_get_guest_hpt_type()`'s comment shows that check for 64-bit is
not done:
```
155 static inline hpt_type_t hpt_emhf_get_guest_hpt_type(VCPU *vcpu) {
156   /* XXX assumes NORM or PAE. Need to check for 64-bit */
157   return (VCPU_gcr4(vcpu) & CR4_PAE) ? HPT_TYPE_PAE : HPT_TYPE_NORM;
158 }
```

### Implementing `hpt_emhf_get_guest_hpt_type`

Intel volume 3 "4.1.1 Four Paging Modes" shows how to check hpt type

Need to access MSR EFER. Perform some code clean up for `MSR_EFER`

### Problem after switching to `load IA32_EFER`

When AP are woken up, there is a wrmsr instruction, where
`ECX = MSR_EFER`, `EAX = 0x00000901`, `EDX = 0xffff8880`. Clearly EDX is
invalid (should be 0).

`RIP = 0x9ae79`, the code are

```
b9 10 00 01 c0       	mov    $0xc0010010,%ecx
0f 32                	rdmsr  
0f ba e8 17          	bts    $0x17,%eax
72 02                	jb     0x59
0f 30                	wrmsr  
a1 10 d0 09 00       	mov    0x9d010,%eax
0f 22 e0             	mov    %eax,%cr4
b8 00 c0 09 00       	mov    $0x9c000,%eax
0f 22 d8             	mov    %eax,%cr3
a1 08 d0 09 00       	mov    0x9d008,%eax
8b 15 0c d0 09 00    	mov    0x9d00c,%edx
b9 80 00 00 c0       	mov    $0xc0000080,%ecx
0f 30                	wrmsr  							<- 0x9ae79
b8 01 00 01 80       	mov    $0x80010001,%eax
0f 22 c0             	mov    %eax,%cr0
ea e0 ae 09 00 10 00 	ljmp   $0x10,$0x9aee0
```

The source looks like `arch/x86/realmode/rm/trampoline_64.S`
```
	# Set up EFER
	movl	pa_tr_efer, %eax
	movl	pa_tr_efer + 4, %edx
	movl	$MSR_EFER, %ecx
	wrmsr
```

However, cannot figure out how `pa_tr_efer` (likely relocation of `tr_efer`) is
set. Also after reverting the change (to commit `ae0968f19`), no longer see
incorrect `EDX`.

Currently the git commits are
```
* 27fdb28d0 (HEAD -> xmhf64, origin/xmhf64) Implement VCPU_glm (getting EFER is tricky)
* 66265c823 Revert "Do not use MSR load area for MSR_EFER; instead, use VMCS {host,guest}_IA32_EFER"
* 9b2327510 Revert "Fix regression in x64 Debian 11 caused by f29cabbd297b63e943b8c30f79690cfe5c62a5af"
* 9b3bee219 Fix regression in x64 Debian 11 caused by f29cabbd297b63e943b8c30f79690cfe5c62a5af
* f33f8effe Update hpt_emhf_get_guest_hpt_type() to detect long mode paging
* f29cabbd2 Do not use MSR load area for MSR_EFER; instead, use VMCS {host,guest}_IA32_EFER
* ae0968f19 Double runtime stack size for x64
```

* `ae0968f19` is the commit before working on this bug.
* `f29cabbd2` and `9b3bee219` are trying to move `MSR_EFER` to VMCS.
* `9b2327510` and `66265c823` revert these changes.
* `f33f8effe` and `27fdb28d0` fix `hpt_emhf_get_guest_hpt_type()` to detect
  long mode paging correctly.

### Assertion error on level

After `27fdb28d0`, see another assertion error:
```
.../xmhf/src/libbaremetal/libxmhfutil/hpt.c:355: assert failed: lvl<=3
```

This looks like a typo. First, looks like `HPT_TYPE_LONG` has never been used
before. Second, in other places in the same file, `assert(lvl<=4)` are used
for `HPT_TYPE_LONG`
```
   354    } else if (t == HPT_TYPE_LONG) {
   355      assert(lvl<=3);
   356      return lvl == 1 || ((lvl==2 || lvl==3) && BR64_GET_BIT(entry, HPT_LO
NG_PS_L32_MP_BIT));
   357    } else if (t == HPT_TYPE_EPT) {
--
   409    } else if (t == HPT_TYPE_LONG) {
   410      assert(lvl<=4);
   411      if (hpt_pme_is_page(t, lvl, entry)) {
   412        if(lvl==1) {
--
   482    } else if (t == HPT_TYPE_LONG) {
   483      assert(lvl<=4);
   484      if (hpt_pme_is_page(t, lvl, entry)) {
   485        if(lvl==1) {
```

## Fix

`ae0968f19..565cef3cf`
* Remove x2APIC capability in CPUID, disallow x2APIC MSR access
* Update `hpt_emhf_get_guest_hpt_type()` to detect long mode paging
* Fix typo of `assert(lvl<=3);` for `HPT_TYPE_LONG`

