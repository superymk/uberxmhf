#!/bin/bash
set -xe
as loop3.s
objcopy --dump-section .text=loop3a.bin       a.out
objcopy --dump-section .trampoline=loop3b.bin a.out
objdump -m i8086 -d -j .text -j .trampoline a.out
xxd --include loop3a.bin | tee    loop3.h
xxd --include loop3b.bin | tee -a loop3.h
rm a.out
rm loop3a.bin
rm loop3b.bin
cat loop3.h

