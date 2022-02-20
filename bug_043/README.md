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

QEMU supports `-boot menu=on` to select a hard disk to boot.

We use the following GDB script to break at 0x7c00

```
hb *0x7c00
hb *0x0
```

Whichever hard disk I select, after hitting the break point, RDX = 0x80.
Actually QEMU / SeaBIOS will also change the booting hard drive to `(hd0)`.

Currently, we have a few cases. The XMHF files are stored in HDD; USB only
contains GRUB files.
* HDD is the first hardware disk and USB is the second hardware disk
	* Boot into HDD (1)
		* rdx=0x80, `(hd0)` is HDD, `(hd1)` is USB
	* Boot into USB (2)
		* rdx=0x80, `(hd0)` is USB, `(hd1)` is HDD
		* `set root='(hd1,msdos1)'` will load XMHF correctly
		* `module --nounzip (hd1)+1` will load USB's GRUB in guest
		  (intended to load HDD)
* USB is the first hardware disk and HDD is the second hardware disk
	* Boot into USB (1)
		* rdx=0x80, `(hd0)` is USB, `(hd1)` is HDD
		* `set root='(hd1,msdos1)'` will load XMHF correctly
		* `module --nounzip (hd1)+1` will load USB's GRUB in guest
		  (intended to load HDD)
	* Boot into HDD (2)
		* rdx=0x80, `(hd0)` is HDD, `(hd1)` is USB

We cannot confirm our hypothesis with only QEMU. Try directly changing EDX on
XMHF.

### Adding argument

We can add a command line argument for XMHF, following `cmdline.c` and
`get_tboot_serial()` will be good enough. Then pass the argument to SLPB and
RPB.

`9afbe1130` implements the change. Looks good on QEMU. In QEMU, need to add
`boot_drive=0x81` to XMHF arguments to boot:
* SeaBIOS -> USB GRUB -> XMHF -> HDD GRUB -> Linux

After test, can also boot to another GRUB on HP.

Also tested on a computer that uses EFI to boot. The computer does not have
serial port (so can only use VGA to see output). There are 2 problems to be
solved in the future:
* EFI GRUB cannot load XMHF correctly (see nothing from VGA)
* XMHF cannot chain load EFI GRUB

## Fix

`6733ca433..9afbe1130`
* Support booting a grub located other than `(hd0)`
* Remove some unnecessary `__attribute__((packed))`

