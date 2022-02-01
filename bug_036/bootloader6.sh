#!/bin/bash

set -xe
as bootloader6.s -o bootloader6.o
objcopy --dump-section .text=bootloader6.bin bootloader6.o
rm bootloader6.o

# Expand to 512 bytes, add bootable flag
truncate -s 512 bootloader6.bin
# TODO

objdump -D -b binary -m i8086 bootloader6.bin

