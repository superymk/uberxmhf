# From gdb/x64_vm_entry.gdb
d
hb arch/x86_64/vmx/peh-x86_64vmx-main.c:300
c
c
d
hb *0x1000
hb *0x1000 + 0x07c0 * 16
c
d

# enter e83
hb *0xe83
hb *0xe83 + 0x7c00
c
d

# exit e83
hb *0xea7
hb *0xea7 + 0x7c00
# c

# break point 13 and 14
hb *0xe9d
hb *0xe9d + 0x7c00

define helper
  info reg
  disable 13
  disable 14
  si
  enable 13
  enable 14
  c
end

while ($rip != 0xea7)
  helper
end

