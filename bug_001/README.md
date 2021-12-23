# Triple fault when guest OS tries to enable paging

## Scope
* x86-64 only

## Behavior
When guest OS tries to enable paging by setting `CR0.PG`, the VMM sees an
`Unhandled intercept: 0x00000002`. This is a triple fault

## Debugging
* Add `nokaslr` to Linux boot line in grub, keep kernel address the same.
* Print instructions near guest RIP (`0x100012e`):
	```
	mov    $0x80050033,%eax
	mov    %rax,%cr0
	```
* Print VMCS after triple fault intercepted by VMM, compare x86 with x86-64.
	* (not very effective)
* Dump kernel memory space using gdb to a file, compare x86 with x86-64.
	* (not very effective)
* Compare register state of x86 and x86-64 just after VMLAUNCH using QEMU.
	* Found that `IA32_EFER.LME` is different.

### Attachments
* `a.gdb`: stop at move to CR0 instruction
* `b.gdb`: print register info (execute after `x??_vm_entry.gdb`)

## Fix
`19f921c3c..c2d641da3`
* When copying `MSR_EFER` from host to guest, clear `LME` and `LMA` flags

