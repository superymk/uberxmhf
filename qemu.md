## Debugging using QEMU

Connect gdb: `target remote :1234`

### Breaking into bootloader
Entry point of bootloader (does not work)
* `rwatch *0x1E00000` (number comes from `init.lds.S`)
* Select XMHF in grub
* `x 0x1E00000 + 4 * 7` shows physical address of `init_start = 0x01e01000`
* `b *0x01e01000` does not work

Middle of bootloader
* When bootloader is printing things, hit Ctrl+C immediately in gdb
* Will stop at some instruction above `0x01e00000`

End of bootloader
* When DRT is disabled
* In `do_drtm()`, find instruction for `invokesl();` at `0x1e01d9e`
* `b *0x1e01d9e`; `c`
* `si` will go to secureloader

### Breaking into secureloader

Entry point of secure loader
* Break anywhere in bootloader (using Ctrl+C)
* Secure loader entry point is always `0x10003080` (empirically, see AMT)
* `b *0x10003080`; `c`
* `symbol-file -o 0x10000000 .../sl_syms.exe`: load symbols

