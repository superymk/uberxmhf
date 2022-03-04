# Reduce size of runtime.exe

## Scope
* x86 and x64
* All hardware
* Git `774b9dc34`

## Behavior
After building XMHF, `xmhf/src/xmhf-core/xmhf-runtime/runtime.exe` is larger
than 200MiB. This is too large. The build directory is a few hundred MiBs.

## Debugging
For example, when git `774b9dc34`, build x64 with DRT, size is 233M. readelf
shows
```
$ readelf -SW xmhf/src/xmhf-core/xmhf-runtime/runtime.exe
There are 14 section headers, starting at offset 0xe8c5e10:

Section Headers:
  [Nr] Name              Type            Address          Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            0000000000000000 000000 000000 00      0   0  0
  [ 1] .text             PROGBITS        0000000010200000 001000 049000 00  AX  0   0 32
  [ 2] .data             PROGBITS        0000000010249000 04a000 437000 00  WA  0   0 32
  [ 3] .palign_data      PROGBITS        0000000010680000 481000 e300000 00  WA  0   0 32
  [ 4] .stack            PROGBITS        000000001e980000 e781000 090000 00  WA  0   0 32
  [ 5] .debug_abbrev     PROGBITS        0000000000000000 e811000 011dea 00      0   0  1
  [ 6] .debug_aranges    PROGBITS        0000000000000000 e822df0 0029a0 00      0   0 16
  [ 7] .debug_info       PROGBITS        0000000000000000 e825790 05fe92 00      0   0  1
  [ 8] .debug_line       PROGBITS        0000000000000000 e885622 023614 00      0   0  1
  [ 9] .debug_ranges     PROGBITS        0000000000000000 e8a8c36 0001e0 00      0   0  1
  [10] .debug_str        PROGBITS        0000000000000000 e8a8e16 00de45 01  MS  0   0  1
  [11] .symtab           SYMTAB          0000000000000000 e8b6c60 009060 18     12 899  8
  [12] .strtab           STRTAB          0000000000000000 e8bfcc0 0060c2 00      0   0  1
  [13] .shstrtab         STRTAB          0000000000000000 e8c5d82 000089 00      0   0  1
Key to Flags:
  W (write), A (alloc), X (execute), M (merge), S (strings), I (info),
  L (link order), O (extra OS processing required), G (group), T (TLS),
  C (compressed), x (unknown), o (OS specific), E (exclude),
  D (mbind), l (large), p (processor specific)
$ 
```

`palign_data` is very large (227 MiB). Can see that it only contains 0s. Also
there is no .bss section. We need to change linker script.

However, looks like things are not that simple:
* I am not sure how to create a .bss section in ld script and make it consume
  no space in ELF.
* `runtime.exe` will be objcopy'ed to `runtime.bin`, and the bss section will
  be sha1 hashed.

Fixing the above issues carelessly may cause a problem in security. So we
use a workaround:
* When building the project, we allow the file size to explode
* After the project is built, it will sit in disk / memory for a long time.
  At this time we want to reduce the size.

This can be done by digging holes in `runtime.exe`. Linux supports creating
sparse file in place (ref <https://wiki.archlinux.org/title/sparse_file>):
> `fallocate -d copy.img`

Looks like it works very well:
```sh
$ md5sum xmhf/src/xmhf-core/xmhf-runtime/runtime.exe
af2ff3e3a55b24a456677583c229630a  xmhf/src/xmhf-core/xmhf-runtime/runtime.exe
$ du -hs xmhf/src/xmhf-core/xmhf-runtime/runtime.exe
229M	xmhf/src/xmhf-core/xmhf-runtime/runtime.exe
$ fallocate -d xmhf/src/xmhf-core/xmhf-runtime/runtime.exe
$ md5sum xmhf/src/xmhf-core/xmhf-runtime/runtime.exe
af2ff3e3a55b24a456677583c229630a  xmhf/src/xmhf-core/xmhf-runtime/runtime.exe
$ du -hs xmhf/src/xmhf-core/xmhf-runtime/runtime.exe
1.2M	xmhf/src/xmhf-core/xmhf-runtime/runtime.exe
$ 
```

After adding some `fallocate -d` on large files in Makefile, reduced build
directory of x64 to 23 MiB, reduced build directory of x86 to 12 MiB.

## Fix

`774b9dc34..4d6217bd5`
* Use sparse file to save build disk space

