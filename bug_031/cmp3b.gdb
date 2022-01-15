set pagination off
set confirm off
hb *0x7c00
c
# Currently at GRUB
c
# currently at NTFS


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

