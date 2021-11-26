# Stop at beginning of runtime

source ~/Documents/GreenBox/notes/gdb/x64_sl_end.gdb

b *$rdx
c
d
symbol-file
symbol-file ~/Documents/GreenBox/xmhf64/xmhf/src/xmhf-core/xmhf-runtime/runtime.exe

