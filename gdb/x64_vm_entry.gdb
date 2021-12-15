# Stop before VMENTRY

source gdb/x64_rt_pre.gdb

b __vmx_start_hvm
c
n 23

