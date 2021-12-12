# Stop at beginning of runtime

source gdb/x64_sl_end.gdb

b *$rdx
c
d
symbol-file
symbol-file xmhf/src/xmhf-core/xmhf-runtime/runtime.exe

