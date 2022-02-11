# Better support for PAE

## Scope
* All x86 OS, Linux and Windows
* x86 and x64 XMHF

## Behavior
Currently the commit `ee1e4c976` is cherry-picked to fill `guest_PDPTE*` with
correct values. But there is a security problem with this code. The two
long-term solutions are:
* Walk EPT in software
* Re-run the MOV CR0 instruction with only CR0.PG changed, let hardware fill
  `guest_PDPTE*` in guest mode

## Debugging

Can see that the operation on CR0 made by x86 Windows 10 is:
```
[cr0-01] MOV TO, old=0x00000030, new=0x80010033, shadow=0x60000010
```

Our idea is to retry with `old=0x00010032, new=0x80010033`, or maybe
`old=0x00010033`.

This idea is implemented in git `833120dc6` (on `xmhf64-dev` branch).

This idea is implemented cleanly in git `fd4534a29` (for x64 XMHF).
Commit `0efebfe87` adds documentation for x86 XMHF.

### History

Before this bug is fixed, `ee1e4c976` can be cherry-picked to unsafely populate
`guest_PDPTE*`. To run Windows XP SP3, another solution is to run revert
`9c0f9491a`, so that change to CR0.PG will not also change other masked bits
in the same instruction.

## Fix

`5324ae8e8..0efebfe87`
* Retry MOV CR0 interceptions when CR0.PG changes

