set pagination off
set confirm off
# source gdb/x86_vm_entry.gdb
hb arch/x86/vmx/peh-x86vmx-main.c:623
c
d

hb *0x7c00
c
d
x/s $eip

hb *0x7cd6
hb *0xd6
c
d

x/wx 0x1a * 4
# should be 0x68:	0xf000fe6e

hb *0xffe6e
hb *0xfe6e
c
d

display /i $cs * 16 + $eip

