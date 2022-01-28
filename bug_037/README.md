# Enable DRT (DTRM)

## Scope
* HP
* Maybe not applicable to QEMU, because need TXT support

## Behavior
Currently DRTM is turned off using `--disable-drt`. Hopefully after enabling
DRT XMHF should still work in x86. However this has not been tested for a long
time. Also supporting DRT in x64 should not be too complicated, because it
looks like most of the work happens in `bootloader -> secureloader`.

## Debugging

Some background materials related to DRTM, SRTM, TXT:
* <https://trustedcomputinggroup.org/wp-content/uploads/DRTM-Specification-Overview_June2013.pdf>
* <https://trustedcomputinggroup.org/resource/d-rtm-architecture-specification/>
* <https://security.stackexchange.com/questions/53258/>
* <https://security.stackexchange.com/questions/39329/>

### Compile errors

First fixed some compile errors due to data types. Now git at `8ed67ee`,
x86 XMHF serial `20220127145105`, x64 XMHF serial `20220127145132`.

In x64, HP automatically reboots (looks like triple fault).

The following line numbers are for
<https://github.com/lxylxy123456/uberxmhf/blob/d4ef00c7acc1a14b2e57c2cfe9672065c9265153/xmhf/src/xmhf-core/xmhf-secureloader/arch/x86_64/sl-x86_64-entry.S>

* If we add an infinite loop after `_sl_start` (line 110), reboot no longer
  happens.
* Add infinite loop before `enable paging` (line 189), no reboot
* Add infinite loop in x64 mode (line 201), reboot
* Add infinite loop after move to CR0 (line 194), reboot

From our previos experience, we are basically certain that this is a triple
fault when enabling long mode. Unfortunately it looks like QEMU cannot run TXT.

### Check CS

Start branch `xmhf64-drt` to store debugging code.

Git `bde35cb55` can print simple things in sl entry code. HP without DRTM
shows:
```
CS=0x00000010
DS=0x00000018
ES=0x00000018
FS=0x00000018
EBP=0x10000000
GDT0=0xb8a0002f	GDT1=0x00001000
GDT2=0x00000000	GDT3=0x00000000
GDT4=0x00000000	GDT5=0x00000000
GDT6=0x0000ffff	GDT7=0x00cf9a00
GDT8=0x0000ffff	GDT9=0x00af9a00
GDTa=0x0000ffff	GDTb=0x00cf9200
GDTc=0x00000000	GDTd=0x00000000
GDTe=0x00000000	GDTf=0x00000000
```

HP with DRTM shows:
```
CS=0x00000008
DS=0x00000010
ES=0x00000018
FS=0x00000018
EBP=0x10000000
GDT0=0x00000000	GDT1=0x30040004
GDT2=0x0004f04b	GDT3=0xc08b0078
GDT4=0x52494350	GDT5=0x00000000
GDT6=0x020004f0	GDT7=0x30303030
GDT8=0x00000080	GDT9=0x00000800
GDTa=0x64300400	GDTb=0x42493030
GDTc=0x00000000	GDTd=0x0004f048
GDTe=0xf0496430	GDTf=0x656c6269
```

We can see that CS and DS are different. This also causes GDT to be garbled.
However it may not be easy to determine CS and DS offset.

For simplicity in development we can adapt the code in `sl-x86-entry.S` to
first establish a GDT where all base addresses are 0. Then jump to long mode.

TODO: check whether CS becomes unexpected value
TODO: check whether gdt data is loaded correctly
TODO: make SL position independent
TODO: Can reuse some x86 SL code

## Fix

`835d832f6..` (d4ef00c7a)
* Fix compile errors due to data types

