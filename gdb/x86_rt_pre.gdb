# Stop at beginning of runtime

source ~/Documents/GreenBox/notes/gdb/x86_sl_pre.gdb

symbol-file
symbol-file ~/Documents/GreenBox/build32/work/xmhf/src/xmhf-core/xmhf-runtime/runtime.exe
b xmhf_runtime_entry
c
d

