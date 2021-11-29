# Stop at beginning of SL (before translating address)
set confirm off
set pagination off
b *0x10003080
c
symbol-file -o 0x10000000 ~/Documents/GreenBox/xmhf64_32/xmhf/src/xmhf-core/xmhf-secureloader/sl_syms.exe
