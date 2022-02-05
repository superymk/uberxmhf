#!/bin/bash

set -xe
as bootloader6.s -o bootloader6.o
objcopy --dump-section .text=bootloader6.bin bootloader6.o
rm bootloader6.o

# Expand to 512 bytes, add bootable flag
echo [ "`stat -c"%s" bootloader6.bin`" -le "510" ]
echo -n $'\x55\xaa' | \
	dd of=bootloader6.bin seek=510 oflag=seek_bytes conv=notrunc
# truncate -s 512 bootloader6.bin

objdump -D -b binary -m i8086 bootloader6.bin

