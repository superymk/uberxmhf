# Triple fault in QEMU x64 Debian 11 after set CR0 PG

## Scope
* x64, Debian 11, QEMU
* Unknown about HP

## Behavior
After selecting XMHF64 in GRUB, then Debian 11 Linux `5.10.0-9-amd64`, see:

```
CPU(0x00): Unhandled intercept: 0x00000002
	CPU(0x00): EFLAGS=0x00010046
	SS:ESP =0x0018:0x01694238
	CS:EIP =0x0010:0x01000146
	IDTR base:limit=0x00000000:0x0000
	GDTR base:limit=0x0167ff60:0x002f
```

## Debugging

`System.map-5.10.0-9-amd64` shows that EIP is at `phys_startup_64`,
instructions around it are:

```
   0x10000de:	lea    0x6a0000(%rbx),%eax
   0x10000e4:	mov    %rax,%cr3
   0x10000e7:	mov    $0xc0000080,%ecx
   0x10000ec:	rdmsr  
   0x10000ee:	bts    $0x8,%eax
   0x10000f2:	wrmsr  
   0x10000f4:	xor    %eax,%eax
   0x10000f6:	lldt   %ax
...
   0x1000129:	sub    $0x28,%esp
   0x100012c:	lea    0x67ca89(%rbp),%eax
   0x1000132:	mov    %edi,%ecx
   0x1000134:	mov    %esi,%edx
   0x1000136:	call   0x1000410
   0x100013b:	push   $0x10
   0x100013d:	push   %rax
   0x100013e:	mov    $0x80000001,%eax
   0x1000143:	mov    %rax,%cr0
   0x1000146:	lret   
   0x1000147:	add    %al,(%rax)
   0x1000149:	add    %al,(%rax)
```

Looks like this is the end of `startup_32` in
`arch/x86/boot/compressed/head_64.S`. I think the name is confusing, because
it looks like relocation of `startup_64`. But the content of `phys_startup_64`
and `startup_64` are different.

### Triple fault and lret
The triple fault shows that EIP points to the lret instruction. We need to know
whether lret causes the problem, or CR0.

First set a break point at intercept for MOV CR0 (0x1000143), then change
`guest_RIP` to another instruction. The gdb script is:
```
# Start from gdb/x64_rt_pre.gdb
b vmx_handle_intercept_cr0access_ug
c
b *0x100013e
hb *0x100013e
b *0x1000143
hb *0x1000143
p vcpu->vmcs.guest_RIP = 0x100013b - vcpu->vmcs.info_vmexit_instruction_length
c
```

Then the same triple fault happens at `CS:EIP =0x0010:0x0100013b`. So we know
that this bug has nothing to do with lret.

### Bit in CR0
By clearing CR0.PG bit in debugger, we can see that the guest is able to
execute some instructions. So the triple fault should be caused by trying to
enable paging. i.e. `bug_008` is fixed.

GDB script:
```
# Start from gdb/x64_rt_pre.gdb
b vmx_handle_intercept_cr0access_ug
c
b *0x100013e
p vcpu->vmcs.guest_RIP = 0x100013b - vcpu->vmcs.info_vmexit_instruction_length
p/x r->rax
# 0x80000001
p r->rax = 1
c
```

### Checking page state

In `vmx_handle_intercept_cr0access_ug()` break point, check CR4.PAE and EFER.LME
```
# Start from gdb/x64_rt_pre.gdb
b vmx_handle_intercept_cr0access_ug
c

(gdb) p/x vcpu->vmcs.guest_CR4 & (1 << 5)
$12 = 0x20
(gdb) p/x ((msr_entry_t *)vcpu->vmx_vaddr_msr_area_guest)[0]
$13 = {index = 0xc0000080, reserved = 0x0, data = 0x0}
(gdb) 

b *0x100013e
p vcpu->vmcs.guest_RIP = 0x100013b - vcpu->vmcs.info_vmexit_instruction_length
p/x r->rax
# 0x80000001
p r->rax = 1
c

(gdb) p/x $efer
$6 = 0x0
(gdb) p $rip
$7 = (void (*)()) 0x100013e
(gdb) 
```

Looks like EFER.LME is not set. Maybe there are some implementation problems
in `_vmx_handle_intercept_wrmsr()` and `_vmx_handle_intercept_rdmsr()`. First
enable printf logging in these functions.

### After first fix

After fixing `_vmx_handle_intercept_wrmsr()` and
`_vmx_handle_intercept_rdmsr()` in `801fe353f..5cae3acd8`, the problem changes
to:
1. RDMSR intercept at RIP = 0x10000ec
2. WRMSR intercept at RIP = 0x10000f2
3. Write CR0 intercept at RIP = 0x1000143
4. VM-ENTRY error: reason=0x80000022, qualification=0x0000000000000001
	* RIP = 0x1000146
	* Qualification means that the first MSR entry is invalid
		* Intel v3 "25.8 VM-ENTRY FAILURES DURING OR AFTER LOADING GUEST STATE"

In Intel v3 "25.4 LOADING MSRS", looks like the problem is still the triple
fault. The load fails because
> An attempt to write bits 127:64 to the MSR indexed by bits 31:0 of the entry
  would cause a general-protection exception if executed via WRMSR with CPL = 0.

Also foot note:
> If CR0.PG = 1, WRMSR to the IA32_EFER MSR causes a general-protection
  exception if it would modify the LME bit. If VM entry has established
  CR0.PG = 1, the IA32_EFER MSR should not be included in the VM-entry MSR-load
  area for the purpose of modifying the LME bit.

It is strange, because in 3 EFER.LME is not changed.

### Understanding MSRs

After reading "25.3.2 Loading Guest State", it looks like my understanding
about MSRs in guest machines should be refreshed.

After a VMEXIT, the MSRs in the guest will be gone, if not explicitly saved.

Currently, XMHF will intercept all RDMSR and WRMSR instructions.

During a VMENTRY, for most MSRs, the guest MSRs = the host MSRs. The difference
are achieved using `vmx_vaddr_msr_area_guest`. Intel advices to keep the number
of MSRs below 512.

Some special MSRs will be modified during VMENTRY. For example, EFER.LME and
EFER.LMA depend on vmentry control -> "IA-32e mode guest".

So after my fix, after WRMSR, "IA-32e mode guest" is 0, but in MSR load area
EFER.LME = 1. This does not matter before CR0.PG is set, but after CR0.PG is
set, during VMENTRY, the following happens
* parallel
	* EFER.LME = 0 due to "IA-32e mode guest" setting
	* CR0.PG = 1 due to `vmcs.guest_CR0` setting
* EFER loaded from MSR load area, where LME = 1

Then a #GP exception happens, which is exactly described in "25.4 LOADING MSRS"

> An attempt to write bits 127:64 to the MSR indexed by bits 31:0 of the entry
  would cause a general-protection exception if executed via WRMSR with CPL = 0.

The solution is to set "IA-32e mode guest" etc correctly.

### Verification of fix

After `9223b099e`, while working on `bug_010`, the guest is able to jump to
`0xffffffff81000077` in `0x1000075`. So I believe the transition to long mode
is successful.

```
   0x100006a:	pop    %rsi
   0x100006b:	mov    %rax,%cr3
   0x100006e:	mov    $0xffffffff81000077,%rax
=> 0x1000075:	jmp    *%rax
   0x1000077:	lgdt   0x1612f82(%rip)        # 0x2613000
   0x100007e:	xor    %eax,%eax
   0x1000080:	mov    %eax,%ds
   0x1000082:	mov    %eax,%ss
   0x1000084:	mov    %eax,%es
```

## Fix

`801fe353f..9223b099e`
* If guest reads / writes `vmx_msr_area_msrs`, do not `rdmsr` / `wrmsr`
* Set "IA-32e mode guest" in `control_VM_entry_controls` according to EFER.LMA

