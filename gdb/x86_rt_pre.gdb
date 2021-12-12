# Stop at beginning of runtime

source gdb/x86_sl_pre.gdb

symbol-file
symbol-file xmhf/src/xmhf-core/xmhf-runtime/runtime.exe
b xmhf_runtime_entry
c
d

