# XMHF cannot boot Windows 10

## Scope
* bad: x64 Windows 10, x64 XMHF
* bad: x86 Windows 10, x86 XMHF
* At most all Windows configurations
* HP and QEMU (may be different root cause)

## Behavior
After selecting XMHF64, then Windows 10, see different behaviors:
* HP
	* Exception #GP on CPU 4, exception #MC on CPU 0 (`20220109205416`, 1 time)
	* No output on serial port (3 times)
* QEMU
	* Unhandled intercept: 0x00000002 (always same error)
		* x64: `20220109214700`; x86: `20220109223437`

TODO: test older OSes
TODO: for HP, print all intercepts
TODO: for QEMU, try x86
TODO: for QEMU, get assembly
