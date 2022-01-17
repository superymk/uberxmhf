set pagination off
set confirm off
# source gdb/x86_vm_entry.gdb
hb arch/x86/vmx/peh-x86vmx-main.c:623
c
d

hb *0x7c00
c
d

p/x $eax = 0xbb00
p/x $ecx = 0x0
p/x $edx = 0x9a0
p/x $ebx = 0x0
p/x $esp = 0x7c00
p/x $ebp = 0x0
p/x $esi = 0x7be4
p/x $edi = 0x8010
p/x ((char*)$eip)[0] = 0xcd
p/x ((char*)$eip)[1] = 0x1a
p/x ((char*)$eip)[2] = 0x90
p/x ((char*)$eip)[3] = 0x90
p/x ((char*)$eip)[4] = 0x90
p/x ((char*)$eip)[5] = 0x90
c

