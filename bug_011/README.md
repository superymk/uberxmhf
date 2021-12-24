# `do_boot_cpu` failed in QEMU x64 Debian 11

## Scope
* x64, Debian 11, QEMU
* Unknown about HP

## Behavior
After selecting XMHF64 in GRUB, then Debian 11 Linux `5.10.0-9-amd64`, after
the Linux kernel is loaded, see (need to disable KASLR):
```
CPU(0x00): CS:EIP=0x0008:0x0009e059 MOV CR4, xx
MOV TO CR4 (flush TLB?), current=0x00002020, proposed=0x00000020
CPU(0x00): CS:EIP=0x0010:0x01000057 MOV CR4, xx
MOV TO CR4 (flush TLB?), current=0x00002020, proposed=0x000000a0
_vmx_handle_intercept_xsetbv: xcr_value=7
_vmx_handle_intercept_xsetbv: host cr4 value=00042020
_vmx_handle_intercept_xsetbv: xcr_value=7
_vmx_handle_intercept_xsetbv: host cr4 value=00042020
```

VGA shows
```
[   10.165055] smpboot: do_boot_cpu failed(-1) to wakeup CPU#1
[   20.169066] smpboot: do_boot_cpu failed(-1) to wakeup CPU#2
[   30.173034] smpboot: do_boot_cpu failed(-1) to wakeup CPU#3
/dev/sda1: clean ...
```

Then serial shows
```
_svm_and_vmx_getvcpu: fatal, unable to retrieve vcpu for id=0xff
```

GDB shows that the first CPU halts. The other CPUs are still at
`xmhf_smpguest_arch_initialize()` (busy waiting for `vcpu->sipireceived`)

## Debugging

### Analyze serial output

A normal serial output should look like (e.g. XMHF64 with x86 Debian QEMU):
```
CPU(0x00): CS:EIP=0x0010:0x010000e8 MOV CR4, xx
MOV TO CR4 (flush TLB?), current=0x00002000, proposed=0x00000000
_vmx_handle_intercept_xsetbv: xcr_value=7
_vmx_handle_intercept_xsetbv: host cr4 value=00042020
_vmx_handle_intercept_xsetbv: xcr_value=7
_vmx_handle_intercept_xsetbv: host cr4 value=00042020
0x0060:0xc104ebcf -> (ICR=0x00000300 write) INIT IPI detected and skipped, value=0x0000c500
0x0060:0xc104ebcf -> (ICR=0x00000300 write) INIT IPI detected and skipped, value=0x00008500
0x0060:0xc104ebcf -> (ICR=0x00000300 write) STARTUP IPI detected, value=0x0000069c
CPU(0x00): processSIPI, dest_lapic_id is 0x01
CPU(0x00): found AP to pass SIPI; id=0x01, vcpu=0x10249f98
CPU(0x00): Sent SIPI command to AP, should awaken it!
CPU(0x01): SIPI signal received, vector=0x9c
```

Call stack is:
```
xmhf_parteventhub_arch_x86_64vmx_intercept_handler() ->
	xmhf_smpguest_arch_x86_64_eventhandler_dbexception() ->
		xmhf_smpguest_arch_x86_64vmx_eventhandler_dbexception()
			INIT IPI -> print message, skip
			STARTUP IPI -> print message, call processSIPI()
```

Set a break point at
`xmhf_smpguest_arch_x86_64vmx_eventhandler_dbexception()`, see that
`g_vmx_lapic_op` is `LAPIC_OP_RSVD`, which is strange.

### Analyze how XMHF implements APIC read / write

`g_vmx_lapic_op` needs to be set as a result of EPT violation. So set another
break point at `xmhf_smpguest_arch_x86_64vmx_eventhandler_hwpgtblviolation()`

The GDB script looks like
```
# Start from gdb/x64_rt_pre.gdb
b xmhf_smpguest_arch_x86_64vmx_eventhandler_dbexception
b xmhf_smpguest_arch_x86_64vmx_eventhandler_hwpgtblviolation
c
```

Call stack for `xmhf_smpguest_arch_x86_64vmx_eventhandler_hwpgtblviolation()`:
```
xmhf_parteventhub_arch_x86_64vmx_intercept_handler() ->
	_vmx_handle_intercept_eptviolation() ->
		xmhf_smpguest_arch_x86_64_eventhandler_hwpgtblviolation() ->
			xmhf_smpguest_arch_x86_64vmx_eventhandler_hwpgtblviolation()
```

See that guest `0xffffffff81062a62` calls
`xmhf_smpguest_arch_x86_64vmx_eventhandler_hwpgtblviolation()`, then
guest `0xffffffff81062a68` calls
`xmhf_smpguest_arch_x86_64vmx_eventhandler_dbexception()`.
This repeats 4 times in total.

Actually `xmhf_smpguest_arch_x86_64vmx_eventhandler_hwpgtblviolation()` setups
up a #DB intercept. That's how
`xmhf_smpguest_arch_x86_64vmx_eventhandler_dbexception()` gets called.

The Linux code is
```
   0xffffffff81062a60 <native_apic_mem_read>:	mov    %edi,%edi
   0xffffffff81062a62 <native_apic_mem_read+2>:	mov    -0xa03000(%rdi),%eax
   0xffffffff81062a68 <native_apic_mem_read+8>:	ret    
```

By print debugging, Linux is reading LAPIC registers `0x20 <LAPIC_ID>` and
`0x30`. No read to interesting registers (`LAPIC_ICR_LOW` and `LAPIC_ICR_HIGH`).
In x86 Debian 11, a lot more accesses are made.

### Analyze how Linux wakes up CPUs

We want to debug the `do_boot_cpu()` function in Linux. First break at
`_vmx_handle_intercept_xsetbv`, 

```
# Start from gdb/x64_rt_pre.gdb
b _vmx_handle_intercept_xsetbv
c
d
# (gdb) x/3i 0x1037ffb
#    0x1037ffb:	xsetbv 
#    0x1037ffe:	mov    0x18346e3(%rip),%rax        # 0x286c6e8
#    0x1038005:	test   $0x8,%al
# (gdb) 
hb *(vcpu->vmcs.guest_RIP + vcpu->vmcs.info_vmexit_instruction_length + 7)
c
d
symbol-file
source gdb/x64_linux_sym.gdb
```

Call stack to `native_apic_mem_read()`:
```
secondary_startup_64
 start_kernel
  setup_arch
   early_acpi_boot_init
    early_acpi_process_madt
     early_acpi_parse_madt_lapic_addr_ovr
      register_lapic_address
       read_apic_id
        apic_read
         native_apic_mem_read
       apic_read
        native_apic_mem_read
   init_apic_mappings
    read_apic_id
     apic_read
      native_apic_mem_read
  x86_late_time_init
   apic_intr_mode_select
    __apic_intr_mode_select
     read_apic_id
      apic_read
       native_apic_mem_read
```

After this point, if debugging is enabled, will see the
`Assertion '!env->exception_has_payload' failed.` error.

After that, will call `do_boot_cpu()`, and see earlier error
```
ret_from_fork()
	kernel_init()
		kernel_init_freeable()
			smp_init()
				bringup_nonboot_cpus()
					cpu_up()
						_cpu_up()
							cpuhp_up_callbacks()
								cpuhp_invoke_callback()
									bringup_cpu()
										__cpu_up()
											native_cpu_up()
												do_boot_cpu()
													...
```

Two calls to `do_boot_cpu()` have 10 seconds interval. During this time can
pause QEMU and set a GDB break point at `do_boot_cpu()`. GDB script:
```
# Make sure RIP starts with 0xffffffff
source gdb/x64_linux_sym.gdb
b do_boot_cpu
c
b wakeup_cpu_via_init_nmi
c
```

Note that the `do_boot_cpu()` called is in `arch/x86/kernel/smpboot.c`, not
`arch/ia64/kernel/smpboot.c`

```
do_boot_cpu()
	wakeup_cpu_via_init_nmi()
		wakeup_secondary_cpu_via_init()
			apic_write()
				native_apic_msr_write()
			apic_read()
				native_apic_msr_read()
			apic_icr_write()
				native_x2apic_icr_write()
					wrmsr()
						...
```

# Temporary notes

TODO: compare how x86 and x64 Linux wakes up CPUs. Look different. See `do_boot_cpu()`
Looks like the APIC is accessed through MSRs, not memory.
* TODO: study x2apic, looks like need to edit wrmsr intercept.
It is strange why x86 and x64 have different behavior
* TODO: debug `do_boot_cpu()` in x86

```
# Make sure RIP starts with 0xffffffff
source gdb/x64_linux_sym.gdb
b do_boot_cpu
c
b wakeup_cpu_via_init_nmi
c
```

