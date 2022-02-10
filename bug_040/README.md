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

TODO: try to change cr0 handling code to support PAE safely

