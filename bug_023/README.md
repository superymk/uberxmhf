# Change accessing VMCS 64-bit fields using full and high

## Scope
* x86 and x64, static

## Behavior
In previous x86 code, 64-bit fields are accessed in two VMREADs. For example,
guest physical address are accessed through `guest_paddr_full` and
`guest_paddr_high`. In x64 code, this variable is still accessed in two
VMREADs. This can be changed
* In x64, use only one VMREAD
* Use a union with name `guest_paddr` to access, avoid using bitwise operations

## Debugging
Use a script `modify.py` to modify header files

Also change all C files, by grepping for lines containing `_full` that are next
to lines containing `_high`.

...

## Fix

`16095a3e7..716d95b93`
* Optimize VMCS-related code for dealing with 64 bit fields

