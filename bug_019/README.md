# e820 missing the last entry

## Scope
* Debian 11 all configurations (QEMU and HP, x86 and x64 XMHF)
* Ubuntu not tested

## Behavior

Using the normal QEMU 512M configuration to boot Debian, dmesg reports memory
as (same for x86 and x86-64):
```
[    0.000000] BIOS-e820: [mem 0x0000000000000000-0x000000000009fbff] usable
[    0.000000] BIOS-e820: [mem 0x000000000009fc00-0x000000000009ffff] reserved
[    0.000000] BIOS-e820: [mem 0x00000000000f0000-0x00000000000fffff] reserved
[    0.000000] BIOS-e820: [mem 0x0000000000100000-0x000000001ffdffff] usable
[    0.000000] BIOS-e820: [mem 0x000000001ffe0000-0x000000001fffffff] reserved
[    0.000000] BIOS-e820: [mem 0x00000000feffc000-0x00000000feffffff] reserved
[    0.000000] BIOS-e820: [mem 0x00000000fffc0000-0x00000000ffffffff] reserved
```

If boot using XMHF, can see that XMHF detects memory correctly in serial:
```
original system E820 map follows:
0x0000000000000000, size=0x000000000009fc00 (1)
0x000000000009fc00, size=0x0000000000000400 (2)
0x00000000000f0000, size=0x0000000000010000 (2)
0x0000000000100000, size=0x000000001fee0000 (1)
0x000000001ffe0000, size=0x0000000000020000 (2)
0x00000000feffc000, size=0x0000000000004000 (2)
0x00000000fffc0000, size=0x0000000000040000 (2)

revised system E820 map follows:
0x0000000000000000, size=0x000000000009fc00 (1)
0x000000000009fc00, size=0x0000000000000400 (2)
0x00000000000f0000, size=0x0000000000010000 (2)
0x0000000000100000, size=0x000000000ff00000 (1)
0x0000000010000000, size=0x0000000009a00000 (2)
0x0000000019a00000, size=0x00000000065e0000 (1)
0x000000001ffe0000, size=0x0000000000020000 (2)
0x00000000feffc000, size=0x0000000000004000 (2)
0x00000000fffc0000, size=0x0000000000040000 (2)
```

However, after XMHF boots Debian, Debian's e820 misses the last entry
(`0xfffc0000-0xffffffff`):
```
[    0.000000] BIOS-e820: [mem 0x0000000000000000-0x000000000009fbff] usable
[    0.000000] BIOS-e820: [mem 0x000000000009fc00-0x000000000009ffff] reserved
[    0.000000] BIOS-e820: [mem 0x00000000000f0000-0x00000000000fffff] reserved
[    0.000000] BIOS-e820: [mem 0x0000000000100000-0x000000000fffffff] usable
[    0.000000] BIOS-e820: [mem 0x0000000010000000-0x00000000199fffff] reserved
[    0.000000] BIOS-e820: [mem 0x0000000019a00000-0x000000001ffdffff] usable
[    0.000000] BIOS-e820: [mem 0x000000001ffe0000-0x000000001fffffff] reserved
[    0.000000] BIOS-e820: [mem 0x00000000feffc000-0x00000000feffffff] reserved
```

## Debugging

The related logics are in `_vmx_int15_handleintercept()`. Set a break point:
```
source gdb/x64_vm_entry.gdb
b peh-x86_64vmx-main.c:130
c
```

...

Looks like the implementation in XMHF is not following the specification. The
specification is ACPI, can be retrieved at
<https://uefi.org/sites/default/files/resources/ACPI_4_Errata_A.pdf>.
Spec says on page 478 that CF means "Non-Carry - Indicates No Error", but the
implementation thinks that CF also indicates the last entry
```c
		// if (no error) {
			// ...
			{
				...
				if(r->ebx > (rpb->XtVmmE820NumEntries-1) ){
					//we have reached the last record, so set CF and make EBX=0
					r->ebx=0;
					guest_flags |= (u16)EFLAGS_CF;
					#ifndef __XMHF_VERIFICATION__
						gueststackregion[2] = guest_flags;
					#endif
				}else{
					//we still have more records, so clear CF
					guest_flags &= ~(u16)EFLAGS_CF;
					#ifndef __XMHF_VERIFICATION__
						gueststackregion[2] = guest_flags;
					#endif
				}

			}

		}else{	//invalid state specified during INT 15 E820, fail by
				//setting carry flag
				printf("\nCPU(0x%02x): INT15 (E820), invalid state specified by guest \
						Halting!", vcpu->id);
				HALT();
		}
```

OSDev says in <https://wiki.osdev.org/Detecting_Memory_(x86)>:
> For the subsequent calls to the function: increment DI by your list entry
  size, reset EAX to 0xE820, and ECX to 24. When you reach the end of the list,
  EBX may reset to 0. If you call the function again with EBX = 0, the list
  will start over. If EBX does not reset to 0, the function will return with
  Carry set when you try to access the entry after the last valid entry.

OSDev is consistent with the specfication.

The fix is straight forward. After fix, on QEMU Debian 11 can see 9 entries
correctly.

For HP, the memory mapping contains above 4G region, so need to change EPT.

## Fix

`d26673ba8..63a196411`
* Fix e820 not able to detect last entry error

