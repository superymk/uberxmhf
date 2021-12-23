# QEMU Debian 11 cannot reboot

## Scope
* x86 and x86-64 XMHF, x86 Debian 11, QEMU

## Behavior
After booting Debian 11 with XMHF, run `init 6`, see hypervisor error.
```
VM-ENTRY error: reason=0x80000021, qualification=0x0000000000000000
Guest State follows:
guest_CS_selector=0x0008
guest_DS_selector=0x0010
guest_ES_selector=0x0010
guest_FS_selector=0x0010
guest_GS_selector=0x0010
guest_SS_selector=0x0010
guest_TR_selector=0x0080
guest_LDTR_selector=0x0000
guest_CS_access_rights=0x0000009b
guest_DS_access_rights=0x00000093
guest_ES_access_rights=0x00000093
guest_FS_access_rights=0x00000093
guest_GS_access_rights=0x00000093
guest_SS_access_rights=0x00000093
guest_TR_access_rights=0x0000008b
guest_LDTR_access_rights=0x0001c000
guest_CS_base/limit=0x0009b000/0xffff
guest_DS_base/limit=0x00000100/0xffff
guest_ES_base/limit=0x00000100/0xffff
guest_FS_base/limit=0x00000100/0xffff
guest_GS_base/limit=0x00000100/0xffff
guest_SS_base/limit=0x00000100/0xffff
guest_GDTR_base/limit=0x0009b040/0xffff
guest_IDTR_base/limit=0x00000000/0xffff
guest_TR_base/limit=0xff406000/0x407b
guest_LDTR_base/limit=0x00000000/0xffff
guest_CR0=0x00000011, guest_CR4=0x001526d0, guest_CR3=0x01c84000
guest_RSP=0xd9b19e1c
guest_RIP=0x00001044
guest_RFLAGS=0x00200006
```

Shutting down (`init 0`) works well. Rebooting works well in HP.

### Ubuntu 12.04 in QEMU

x86 QEMU and x86 Ubuntu 12.04 also cannot reboot, with a different error
(intercept of triple fault). Shutting down also works well on Ubuntu.

## Debugging

### Debugging in Linux

By searching on the web, Linux is likely to call `kernel_restart()` when
rebooting. So set a breakpoint there.

Not so easy to trace through kernel's calls

Difficult to debug because there are all kinds of timer interrupts? Cannot do
`step` in gdb, need to explicitly set break point and continue.

Call stack:
```
kernel_restart() ->
	machine_restart() ->
		machine_ops.restart = native_machine_restart() ->
			machine_shutdown() ->
				machine_ops.shutdown = native_machine_shutdown() ->
					...
			__machine_emergency_restart()
				machine_ops.emergency_restart = native_machine_emergency_restart() ->
					acpi_reboot() ->
						Not successful because acpi_gbl_FADT.header.revision < 2
					mach_reboot_fixups()
					acpi_reboot() (retry)
					mach_reboot_fixups() (retry)
					efi_reboot()
					machine_real_restart() ->
						Executes strange code
```

Call stack without XMHF:
```
native_machine_emergency_restart() ->
	acpi_reboot() ->
		Not successful because acpi_gbl_FADT.header.revision < 2
	mach_reboot_fixups()
	In BOOT_KBD, reboot successful (PC jumps to 0xfff0)
```

### Debugging in XMHF

On XMHF's side, looks like `xmhf_baseplatform_reboot()` function is important

By setting a print in `xmhf_parteventhub_arch_x86vmx_intercept_handler()`,
looks like the last intercept is `VMX_VMEXIT_CRX_ACCESS`.

Moving from EDX to CR0, `EDX = 0x60000011 = [CD NW ET PE]`,
`control_CR0_shadow = guest_CR0 = 0x80050033 = [PG ? ? NE ET MP PE]`,
`control_CR0_mask = 0x60000020 = [CD NW NE]`
`guest_RIP = 0x1041`. By setting later break points looks like this causes
the VM-ENTRY error.

```py
MASK = 0x60000020
HP = 'hp-'
DEB = 'deb'
BAD = 'bad'
DATA = [
	# typ, pre_cr0,   proposed,   pre_shadow, post_cr0,   post_shadow
	[HP,  0x80050033, 0xc0050033, 0x00000020, 0x80050033, 0xc0050033],
	[HP,  0x80050033, 0x80050033, 0xc0050033, 0x80050033, 0x80050033],
	[HP,  0x8005003b, 0xc005003b, 0x80050033, 0x8005003b, 0xc005003b],
	[HP,  0x8005003b, 0x8005003b, 0xc005003b, 0x8005003b, 0x8005003b],
	[HP,  0x8005003b, 0xc005003b, 0x00000020, 0x8005003b, 0xc005003b],
	[HP,  0x8005003b, 0x8005003b, 0xc005003b, 0x8005003b, 0x8005003b],
	[HP,  0x8005003b, 0xc005003b, 0x8005003b, 0x8005003b, 0xc005003b],
	[HP,  0x8005003b, 0x8005003b, 0xc005003b, 0x8005003b, 0x8005003b],
	[DEB, 0x80050033, 0xc0050033, 0x80050033, 0x80050033, 0xc0050033],
	[DEB, 0x80050033, 0x80050033, 0xc0050033, 0x80050033, 0x80050033],
	[DEB, 0x80050033, 0xc0050033, 0x00000020, 0x80050033, 0xc0050033],
	[DEB, 0x80050033, 0x80050033, 0xc0050033, 0x80050033, 0x80050033],
	[BAD, 0x80050033, 0x60000011, 0x80050033, 0x00000011, 0x60000011],
]
for typ, pre_cr0, proposed, pre_shadow, post_cr0, post_shadow in DATA:
	assert (post_cr0 & ~MASK) == (post_shadow & ~MASK)
	print('%s 0x%08x 0x%08x 0x%08x' % (typ, proposed, (post_cr0 & MASK), (post_shadow & MASK)))
```

Try to set `post_cr0` to random values and see whether 
```
0x00000011: (bad)
0x80000011: bad
0x80050033: good
0x00050033: good
0x00000033: good
0x00000013: bad
0x00000031: good
```

The problem should be bit 5 of CR0 (NE), but this is strange.

### ACPI version
In `smp.c` `smp_getinfo()`, can print `rsdp->revision`. On QEMU revision is 0,
on HP revision is 2. Looks like QEMU is too old.
e.g. <https://stackoverflow.com/questions/60137506/qemu-support-for-acpi-2-0>

In HP, Linux will likely to reboot using ACPI. Not sure what will happen after
that, because there are almost no serial output.

### Attachment

`git.diff`: temporary change to `xmhf64` branch, on top of commit `9b1777042`.

## Result

Not fixed now, fixed in `bug_008`

