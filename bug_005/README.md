# HP Ubuntu 12.04 x86 Regression

## Scope
* x86, Ubuntu 12.04, HP

## Behavior
After fixing `bug_004` (commit `d12b97c0c`), cannot launch x86 XMHF in HP
machine. AMT term shows:

```
CPU(0x00): VMCLEAR success.
CPU(0x00): VMPTRLD success.
CPU(0x00): VMWRITEs success.
CPU(0x00): VMLAUNCH error; code=0x7. HALT!
Guest State follows:
guest_CS_selector=0x0000
guest_DS_selector=0x0000
guest_ES_selector=0x0000
guest_FS_selector=0x0000
guest_GS_selector=0x0000
guest_SS_selector=0x0000
guest_TR_selector=0x0000
guest_LDTR_selector=0x0000
guest_CS_access_rights=0x00000093
guest_DS_access_rights=0x00000093
guest_ES_access_rights=0x00000093
guest_FS_access_rights=0x00000093
guest_GS_access_rights=0x00000093
guest_SS_access_rights=0x00000093
guest_TR_access_rights=0x00000083
guest_LDTR_access_rights=0x00010000
guest_CS_base/limit=0x00000000/0xffff
guest_DS_base/limit=0x00000000/0xffff
guest_ES_base/limit=0x00000000/0xffff
guest_FS_base/limit=0x00000000/0xffff
guest_GS_base/limit=0x00000000/0xffff
guest_SS_base/limit=0x00000000/0xffff
guest_GDTR_base/limit=0x00000000/0x0000
guest_IDTR_base/limit=0x00000000/0x03ff
guest_TR_base/limit=0x00000000/0x0000
guest_LDTR_base/limit=0x00000000/0x0000
guest_CR0=0x00000020, guest_CR4=0x00002000, guest_CR3=0x00000000
guest_RSP=0x00000000
guest_RIP=0x00007c00
guest_RFLAGS=0x00000202
```

## Debugging

### Git bisect
```
git bisect start
git bisect bad d12b97c0c
git bisect good eca643c3c
git bisect bad 21b1abf4b
git bisect bad 81a80d216
git bisect bad 3b8955c74
```

The problematic commit is enabling INVPCID (`bug_002`)

`bug_003` is similar, but does not cause this regression.

According to Intel volume 2, should check INVPCID's availability using
`CPUID.(EAX=07H, ECX=0H):EBX.INVPCID (bit 10)`.

For RDTSCP (`bug_003`), should also check `CPUID.80000001H:EDX.RDTSCP[bit 27]`

## Fix

`d12b97c0c..e74a28b83`
* Check INVPCID and RDTSCP before setting them in VMCS

