# Review drive number given to BIOS

## Scope
* At least affecting Linux

## Behavior
1. `__vmx_start_hvm` in `part-x86_64vmx-sup.S` should also set r8 - r15 to 0
2. For `__vmx_start_hvm` in `part-x86vmx-sup.S` and `part-x86_64vmx-sup.S`,
   maybe EDX should not be hardcoded as 0x80. If boot from 2nd drive, should
   likely be 0x81.

## Debugging

### Literature review

Reference 1:
<https://en.wikipedia.org/wiki/BIOS#Boot_environment> says
> DL may contain the drive number, as used with INT 13h, of the boot device.

Reference 2:
<https://en.wikipedia.org/wiki/INT_13H#INT_13h_AH=42h:_Extended_Read_Sectors_From_Drive>
says
> DL ... drive index (e.g. 1st HDD = 80h) 

For example, when HP has a HDD and a USB, normally HDD will be 0x80 and USB
will be 0x81. But when selecting boot from USB in BIOS, USB will become 0x80
and HDD will become 0x81. This will make chain loading HDD's GRUB impossible.

### Empirical study

TODO: test the behavior on QEMU
TODO: likely accept an argument in XMHF bootloader to override DX

