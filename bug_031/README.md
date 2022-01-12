# XMHF cannot boot Windows 10 on QEMU due to triple fault

## Scope
* bad: x64 and x86 Windows 10, x64 and x86 XMHF, QEMU
* bad: Windows 10, HP (but another bug)

## Behavior
See `bug_027` "Behavior" section for logs

After selecting XMHF and then Windows 10, see triple fault.
x86 and x64 behavior are the same.
```
CPU(0x00): Unhandled intercept: 0x00000002
	CPU(0x00): EFLAGS=0x00000046
	SS:ESP =0xda00:0x000e97d4
	CS:EIP =0xf000:0x000f7d0b
	IDTR base:limit=0x000f615e:0x0000
	GDTR base:limit=0x000f6120:0x0037
```
