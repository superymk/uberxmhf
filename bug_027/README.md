# XMHF cannot boot Windows XP on QEMU (and Windows 10, and HP)

## Scope
* bad: x64 Windows 10, x64 XMHF, HP and QEMU
* bad: x86 Windows 10, x86 XMHF, QEMU
* bad: x86 Windows XP SP3, x86 XMHF, QEMU
* bad: x86 Windows XP SP3, x64 XMHF, QEMU
* good: x64 Windows XP SP1, x64 XMHF, QEMU
* HP and QEMU (may be different root cause)

## Behavior
After selecting XMHF64, then Windows 10, see different behaviors:
* HP
	* Exception #GP on CPU 4, exception #MC on CPU 0 (`20220109205416`, 1 time)
	* No output on serial port (3 times)
* QEMU
	* Unhandled intercept: 0x00000002 (always same error)
		* Windows 10 x64: `20220109214700`
		* Windows 10 x86: `20220109223437`
		* Windows XP x86: `20220110101504`

## Debugging

First debug Windows XP in QEMU. After triple fault, connect GDB

```
(gdb) p/x vcpu->vmcs.guest_CR0
$3 = 0x8000003b
(gdb) p/x vcpu->vmcs.guest_CR3
$4 = 0x23c5000
(gdb) p/x vcpu->vmcs.guest_CR4
$5 = 0x2000
(gdb) 
```

Serial:
```
CPU(0x01): Unhandled intercept: 0x00000002
	CPU(0x01): EFLAGS=0x00010006
	SS:ESP =0x0000:0x00000000
	CS:EIP =0x1f00:0x000003a6
	IDTR base:limit=0xf78c4590:0x07ff
	GDTR base:limit=0xf78c4190:0x03ff
```

Since CS is very large, this is likely when jumping from real mode to protected
mode. The instructions can be accessed using CS * 16 + EIP. The start address
is around `0x1f350`.

Get disassembly at `real1.s.16`
```sh
as real1.s
objdump -m i8086 -d a.out > real1.s.16
```

After some debugging, found:
* CR0 is not intercepted at `0x1f3a3`
* Windows tries to set CR4.PAE in `0x1f361`. Serial:
```
MOV TO CR4 (flush TLB?), current=0x00002000, proposed=0x00000020
```

### PAE

Maybe better to try to disable PAE. Ref:
<https://docs.microsoft.com/en-us/windows/win32/memory/physical-address-extension>

For Windows XP, go to Explorer, enter `C:\boot.ini`, then in
`[operating systems]`, add `/NOPAE` to the end of the first entry.

For Windows 10, Microsoft website has another way to disable PAE.

However, still see the same behavior. Ignore for now. Also, enabling PAE

### CR4

I think the problem may be related to CR4. In `part-x86vmx.c`, VMXE in
`control_CR4_shadow` is set. This causes normal write to CR4 if VMXE = 0.
So should not set `control_CR4_shadow`.

```
464  	//trap access to CR4 fixed bits (this includes the VMXE bit)
465  	vcpu->vmcs.control_CR4_mask = vcpu->vmx_msrs[INDEX_IA32_VMX_CR4_FIXED0_MSR];
466  	vcpu->vmcs.control_CR4_shadow = CR4_VMXE; //let guest know we have VMX enabled
```

Also, I don't think this is the way to let the guest know VMX is enabled. Or
maybe there is no way to do so?

This becomes a large problem because `vmx_handle_intercept_cr4access_ug()`'s
implementation is essentially ignoring the guest's request. Probably Windows
sets up the page table using PAE, but due to incorrect handling of CR4 the CPU
accesses the page table using non-PAE, causing the problem.

To fix, should set CR4 shadow to 0. In Intel 3's Table 9-1, it says
CR0 = 60000010H, CR2 = CR3 = CR4 = 00000000H.

After fixing CR4, the triple fault is solved. However, in Windows XP x86,
see a lot of TrustVisor information. Is Windows trying to access VMCALL?
Git after fix is `069f2d1b9`

### VMCALL in Windows XP

Try to set a break point at `scode_register()`
```
waitamt /tmp/amt; gdb --ex 'target remote :::2198' -x gdb/x86_vm_entry.gdb
hb scode_register
c
```

However, this does not work. Will see the following error in QEMU (first found
in `bug_004`)
```
kvm_queue_exception: Assertion `!env->exception_has_payload' failed.
```

So use nop breakpoint instead. See `x86_nop.gdb`.

The break point hits on VMCALL intercept handler. It is still PAE paging.
```
(gdb) p/x vcpu->vmcs.guest_RIP
$5 = 0x800cb189
(gdb) p/x vcpu->vmcs.guest_CR4
$10 = 0x26b8
(gdb) p/x vcpu->vmcs.guest_CR3
$11 = 0x19e00020
(gdb) p/x vcpu->vmcs.guest_CR0
$12 = 0x80010031
(gdb) 
```

Maybe we can disable PAE using CPUID? For x86 guests it should be fine. However
disabling PAE should be temporary.

### GDB snippet for performing page walk in PAE paging

```gdb
set $a = vcpu->vmcs.guest_RIP
set $c = vcpu->vmcs.guest_CR3
set $p1 = ($c           ) & ~0x1f  | ((($a >> 30) & 0x1ff) << 3)
set $p2 = (*(long*)($p1)) & ~0xfff | ((($a >> 21) & 0x1ff) << 3)
set $p3 = (*(long*)($p2)) & ~0xfff | ((($a >> 12) & 0x1ff) << 3)
set $p4 = (*(long*)($p3)) & ~0xfff
set $b = $p4 | ($a & 0xfff)
```

The physical address for RIP is `0xcb189`. Looks like it is really a VMCALL
instruction.

Assembly:
```
   0xcb0c0:	pushf  
   0xcb0c1:	cli    
   0xcb0c2:	xchg   %ax,%ax
   0xcb0c4:	push   %ecx
   0xcb0c5:	movzbl %fs:0x51,%eax
   0xcb0cd:	mov    0x800cb03c,%ecx
   0xcb0d3:	shl    %cl,%eax
   0xcb0d5:	testb  $0x1,-0x7ff34cfc(%eax)
   0xcb0dc:	je     0xcb0e8
   0xcb0de:	movzbl -0x7ff34d00(%eax),%eax
   0xcb0e5:	pop    %ecx
   0xcb0e6:	popf   
   0xcb0e7:	ret    
   0xcb0e8:	mov    0x800cb040,%eax
   0xcb0ed:	mov    (%eax),%eax
   0xcb0ef:	jmp    0xcb0e5
   0xcb0f1:	mov    %eax,%ebx
   0xcb0f3:	call   0xcb0c0
   0xcb0f8:	xchg   %eax,%ebx
   0xcb0f9:	ret    
   0xcb0fa:	mov    %eax,%ecx
   0xcb0fc:	call   0xcb0c0
   0xcb101:	xchg   %eax,%ecx
   0xcb102:	ret    
   0xcb103:	mov    %eax,%edx
   0xcb105:	call   0xcb0c0
   0xcb10a:	xchg   %eax,%edx
   0xcb10b:	ret    
   0xcb10c:	mov    %eax,%esi
   0xcb10e:	call   0xcb0c0
   0xcb113:	xchg   %eax,%esi
   0xcb114:	ret    
   0xcb115:	mov    %eax,%edi
   0xcb117:	call   0xcb115
   0xcb11c:	xchg   %eax,%edi
   0xcb11d:	ret    
   0xcb11e:	mov    %eax,%ebp
   0xcb120:	call   0xcb0c0
   0xcb125:	xchg   %eax,%ebp
   0xcb126:	ret    
   0xcb127:	call   0xcb0c0
   0xcb12c:	xchg   %eax,0x4(%esp)
   0xcb130:	ret    
   0xcb131:	push   %eax
   0xcb132:	call   0xcb138
   0xcb137:	ret    
   0xcb138:	pushf  
   0xcb139:	push   %eax
   0xcb13a:	push   %ecx
   0xcb13b:	push   %edx
   0xcb13c:	push   %ebx
   0xcb13d:	cli    
   0xcb13e:	xchg   %ax,%ax
   0xcb140:	movzbl %fs:0x51,%edx
   0xcb148:	mov    0x800cb03c,%ecx
   0xcb14e:	shl    %cl,%edx
   0xcb150:	testb  $0x1,-0x7ff34cfc(%edx)
   0xcb157:	je     0xcb18e
   0xcb159:	mov    -0x7ff34d00(%edx),%eax
   0xcb15f:	mov    %eax,%ebx
   0xcb161:	mov    0x18(%esp),%bl
   0xcb165:	lock cmpxchg %ebx,-0x7ff34d00(%edx)
   0xcb16d:	jne    0xcb140
   0xcb16f:	cmp    %bh,%bl
   0xcb171:	jae    0xcb175
   0xcb173:	mov    %bh,%bl
   0xcb175:	rol    $0x8,%ebx
   0xcb178:	cmp    %bh,%bl
   0xcb17a:	ja     0xcb184
   0xcb17c:	pop    %ebx
   0xcb17d:	pop    %edx
   0xcb17e:	pop    %ecx
   0xcb17f:	pop    %eax
   0xcb180:	popf   
   0xcb181:	ret    $0x4
   0xcb184:	mov    $0x1,%eax
   0xcb189:	vmcall 
   0xcb18c:	jmp    0xcb17c
   0xcb18e:	mov    0x18(%esp),%ecx
   0xcb192:	mov    0x800cb040,%eax
   0xcb197:	mov    %ecx,(%eax)
   0xcb199:	jmp    0xcb17c
   0xcb19b:	out    %al,$0x7e
   0xcb19d:	movzbl 0x800cb300,%eax
   0xcb1a4:	ret    
   0xcb1a5:	out    %al,$0x7e
   0xcb1a7:	movzbl 0x800cb300,%ebx
   0xcb1ae:	ret  
   ...  
```

From the stack, can see that PA 0xcb0c0 is function entry point. It is called
by PA 0x6e6422, VA 0x806e6422.

Try to inject #UD to guest (pretend not in VMX), git is `f015462c2`, get blue
screen.

If set a break point using `hb *0x800cb189`, without XMHF will still hit this
break point.

```
(gdb) p $eax
$9 = 1
(gdb) p $ebx
$10 = 209
(gdb) p $ecx
$11 = 7
(gdb) p $edx
$12 = 0
(gdb) p $esi
$13 = 0
(gdb) 
```

From <https://www.kernel.org/doc/html/latest/virt/kvm/hypercalls.html> and
<https://elixir.bootlin.com/linux/v5.10.84/source/include/uapi/linux/kvm_para.h#L21>,
the called function is `KVM_HC_VAPIC_POLL_IRQ`.

Looks like just need to return with EAX = 0:
<https://elixir.bootlin.com/linux/v5.10.84/source/arch/x86/kvm/x86.c#L8202>
However, how does Windows XP know we are running KVM?

In <https://www.kernel.org/doc/html/latest/virt/kvm/cpuid.html>, can see that
the legitimate way to determine whether running in KVM is CPUID 0x40000000.
Likely Windows XP uses this way to determine whether running in KVM.

It is surprising that Windows XP SP3 (released in Apr 2008) already tries to
detect KVM features (released in Feb 2007).

Now, we print all accesses to CPUID instruction. But it does not look like
CPUID 0x40000000 is accessed. Git is `4aede88dd`.

Then after changing code (not sure what exactly happend), see a lot more
VMCALLs.

If remove printing during VMCALLs, can boot Windows XP in x86 XMHF. Git is
`d58f13c60`. After cleaning up git is `ed2714bab`, but still have a few
questions.

1. Why does Windows XP call VMCALL frequently?
2. Has Windows XP detected the KVM? How to prevent it from calling VMCALL?
3. CR4 Intercept problem
	* In Windows XP x86, 0x6a29be causes CR4 intercept, but edx is wrong.
	  Probably CR4 read at 0x6a29b8 is incorrect.
4. We should probably support booting Linux with PAE

### Untested ideas

* How does Linux PAE fail?
* Try to forward VMCALL to real KVM
* Windows XP symbols: try <https://www.cnblogs.com/csnd/p/11800535.html>
* Good info web page about virtualization:
  <https://sites.cs.ucsb.edu/~rich/class/old.cs290/notes/Virtualization/virtualization.html>

## Fix

`3b199dbe0..069f2d1b9`
* Clear VMXE from CR4 shadow, update CR4 intercept handler

