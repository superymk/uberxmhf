set pagination off
set confirm off
hb *0x7c00
c
# Currently at GRUB

p/x $eax = 0xbb00
p/x $ecx = 0x0
p/x $edx = 0x9a0
p/x $ebx = 0x0
p/x $esp = 0x7c00
p/x $ebp = 0x0
p/x $esi = 0x7be4
p/x $edi = 0x8010

#    7cd9:	cd 1a                	int    $0x1a
#    7cdb:	66 23 c0             	and    %eax,%eax
#    7d1b:	90                   	nop

p/x ((char*)$eip)[0] = 0xcd
p/x ((char*)$eip)[1] = 0x1a
p/x ((char*)$eip)[2] = 0x90
p/x ((char*)$eip)[3] = 0x90
p/x ((char*)$eip)[4] = 0x90
p/x ((char*)$eip)[5] = 0x90

hb *0x7c03

hb *0x7d06
hb *0xf7d06
c
d

