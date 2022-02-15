# HP cannot boot Windows 10 x64 (WRMSR)

## Scope
* HP, x64 Windows 10, x64 XMHF
* good: HP, x86 Windows 10, x64 XMHF
* good: QEMU, Windows 10
* good: QEMU, Fedora
* bad, probably related: ThinkPad, Fedora 35

## Behavior

When booting x64 Windows 10, x64 XMHF on HP, see exception in hypervisor. Use
`read_stack.py` to annotate:
```
[00]: unhandled exception 14 (0xe), halting!
[00]: error code: 0x0000000000000000
[00]: state dump follows...
[00] CS:RIP 0x0010:0x0000000010204a30 with RFLAGS=0x0000000000010083
...
[00]-----stack dump-----
[00]  Stack(0x000000001d985ea8) -> 0x0000000010204a30 CS	<_vmx_handle_intercept_wrmsr+784>
[00]  Stack(0x000000001d985eb0) -> 0x0000000000000010 RFLAGS	???
[00]  Stack(0x000000001d985eb8) -> 0x0000000000010083 RSP	???
[00]  Stack(0x000000001d985ec0) -> 0x000000001d985ed8 SS	<g_cpustacks+65240>
...
[00]  Stack(0x000000001d985f28) -> 0x000000001d985f68 RBP #0	<g_cpustacks+65384>
[00]  Stack(0x000000001d985f30) -> 0x0000000010205859 RIP #1	<xmhf_parteventhub_arch_x86_64vmx_intercept_handler+1278>
...
[00]  Stack(0x000000001d985f68) -> 0x0000000000000000 RBP #1	???
[00]  Stack(0x000000001d985f70) -> 0x0000000010203f2e RIP #2	<xmhf_parteventhub_arch_x86_64vmx_entry+40>
...
```

## Debugging

### Fedora

After installing x64 Fedora 35 on HP, can reproduce the same issue.
Git around `ae6d89527`, serial `20220208231648`. We think Windows 10 and Fedora
35 should have similar issue. Currently this issue is reproduced on:
* HP, x64 Windows 10, x64 XMHF
* HP, x64 Fedora 35, x64 XMHF
* ThinkPad, x64 Fedora 35, x64 XMHF

### Print debugging

We use Fedora on HP to debug.

Git `a7ad36456`, serial `20220211191516`, can see that the problem happens when
Writing `0xffff88802d036cb4` to MSR `0x00000079`. This MSR is
`IA32_BIOS_UPDT_TRIG (BIOS_UPDT_TRIG)`. Intel volume 4 links to volume 3
"9.11 MICROCODE UPDATE FACILITIES".

I guess we can just ignore this instruction?

After turning kaslr off, serial `20220211193500`. The Linux kernel should be
`vmlinuz-5.14.10-300.fc35.x86_64`. Install debuginfo (download 735 M, install
3.8 G):
```sh
sudo dnf debuginfo-install kernel-5.14.10-300.fc35
```

Installed files are mainly:
```
/usr/src/debug/kernel-5.14.10/linux-5.14.10-300.fc35.x86_64/
/usr/lib/debug/usr/lib/modules/5.14.10-300.fc35.x86_64/vmlinux
```

Can see that the problematic RIP is in `apply_microcode_early()`
```
(gdb) x/i 0xffffffff8104b923
   0xffffffff8104b923 <apply_microcode_early+99>:	wrmsr
(gdb) x/10i apply_microcode_early+99
```

### Ignore microcode update

Git `522a7b607`, serial `20220211200042`. Looks Fedora can continue booting.
But Fedora outputs too many RDMSR messages, so we need to declutter.

Git `0226a48cd`, serial `20220211200701`. Can see that CPU 0 looks fine, but
CPU 1 keeps retrying write microcode. So we cannot simply ignore.

But x64 Windows 10 at this commit can boot correctly (x86 PAL tests also pass),
serial `20220211213522`. So maybe a problem in Linux?

### Microcode

Now we survey things related to microcode. Can search for "microcode" in Intel
manuals.

* Volume 2
	* CPUID will change `IA32_BIOS_SIGN_ID`
* Volume 3
* Volume 4
	* `IA32_PLATFORM_ID (MSR_PLATFORM_ID, 17h)`: determine which microcode to
	  use
	* `IA32_BIOS_UPDT_TRIG (BIOS_UPDT_TRIG, 79h)`: perform update
	* `IA32_BIOS_SIGN_ID (BIOS_SIGN/BBL_CR_D3, 8bh)`: show updated signature

From Intel documentation looks like there are no easy way to report to the
guest that microcode update is not supported. But when running Fedora on QEMU,
Fedora does not try to write to `IA32_BIOS_UPDT_TRIG`.

### Linux code

Now we need to know:
* Why does Fedora CPU 1 tries to update microcode forever
* Why does Fedora not updating microcode in QEMU

Related Linux source files are:
* `arch/x86/kvm/x86.c`: how KVM reacts to microcode updates
	* Search for `MSR_IA32_UCODE_`, can see that `MSR_IA32_UCODE_WRITE` is NOP
* `arch/x86/kernel/cpu/microcode/intel.c`: where WRMSR is called

Maybe the reason that CPU 1 retries forever is that:
* `apply_microcode_early()` is the function that tries to update microcode
* `load_ucode_intel_bsp()` calls `apply_microcode_early()` once, without
  checking return code
* `load_ucode_intel_ap()` calls `apply_microcode_early()`, then has a
  `goto reget;` instruction.

Before `apply_microcode_early()` is called, `__load_ucode_intel()` is called
to find a patch. We can see whether on QEMU this function is called.

Suspected call stack is
```
x86_64_start_kernel()
	load_ucode_bsp()
		load_ucode_intel_bsp()
			__load_ucode_intel()
			apply_microcode_early()
```

After debugging on QEMU Fedora without XMHF, can see that `load_ucode_bsp()`
is called, but then `check_loader_disabled_bsp()` causes the function to return
(before it can call `load_ucode_intel_bsp()`).

Then after some debugging, can see that `check_loader_disabled_bsp()` has:
```c
	/*
	 * CPUID(1).ECX[31]: reserved for hypervisor use. This is still not
	 * completely accurate as xen pv guests don't see that CPUID bit set but
	 * that's good enough as they don't land on the BSP path anyway.
	 */
	if (native_cpuid_ecx(1) & BIT(31))
		return *res;
```

This allows the hypervisor to tell the guest OS that the guest OS is running
in a hypervisor. So Linux does not try to update microcode. On Wikipedia
<https://en.wikipedia.org/wiki/CPUID#EAX=1:_Processor_Info_and_Feature_Bits>:
> Hypervisor present (always zero on physical CPUs)

Next steps:
* Try to change `CPUID[1].ECX` in QEMU
* Can we set this bit in XMHF? Will this solve Windows XP problem (`bug_029`)?

### Hypervisor Present

We try to set the hypervisor present flag in XMHF. Git `4359bfe56`, Windows 10
serial `20220211213857`. Can see that Windows still tries to update microcode.

After this change, Fedora can boot correctly, without trying to update
microcode.

### Related issues

After some testing, looks like removing hypervisor present flag does not solve
Windows XP problem (`bug_029`)

For QEMU behavior, looks like changing QEMU's CPUID is too complicated. So
instead set a breakpoint at checking `ECX & (1 << 31)` and change ECX. Looks
like `__load_ucode_intel()` will return 0 (i.e. no microcode found), but I am
not 100% sure.

### Conclusion

Our conclusion for this bug is:
* Ignore request of updating microcode
* Guest OS will see that updating microcode fails
	* On Fedora AP will retry forever, solve this by setting hypervisor present

Drawbacks
* Fedora AP will retry forever -> likely a bug in Linux, can test further
* Setting hypervisor present -> should be justified in XMHF, but should test
  to make sure other OS do not have regression

Now both Fedora 35 and Windows 10 can boot correctly in x64 mode.

## Fix

`69bad8e08..221e7b84b`
* Ignore WRMSR to IA32_BIOS_UPDT_TRIG
* Set hypervisor present flag in CPUID

