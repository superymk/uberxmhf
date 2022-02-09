# Run TrustVisor PALs on Windows

## Scope
* Windows, all platforms
	* QEMU
	* HP if HP can boot Windows

## Behavior
Currently TrustVisor PALs can only be built and run on Linux. Since XMHF needs
to support Windows, we should be able to run PALs on Windows.

## Debugging
The key work is to build the executable for Windows, and follow Linux calling
conventions. A few possibilities:
* Write wrapper functions to translate Windows calling convention to Linux
* Use WSL to run Linux binaries on Windows
* Use MinGW to build the exe file on Linux
* Use Wine to run exe on Linux for testing

### MinGW
* Fedora
	* Install: `mingw32-gcc mingw64-gcc` (not tested)
* Debian
	* Install: `gcc-mingw-w64`
	* GCC: `/usr/bin/{i686,x86_64}-w64-mingw32-gcc`

We can try to build PAL demo using:
```sh
cd hypapps/trustvisor/pal_demo/
CC=/usr/bin/x86_64-w64-mingw32-gcc LD=/usr/bin/x86_64-w64-mingw32-ld make WINDOWS=y
CC=/usr/bin/i686-w64-mingw32-gcc LD=/usr/bin/i686-w64-mingw32-ld make WINDOWS=y
```

A few problems need to be fixed
* `sys/mman.h` is not available in Windows, cannot use `mmap()` and `mlock()`.
  See <https://github.com/msys2/MINGW-packages/issues/6002>
* `RAND_MAX` is only `0x7fff` in MinGW, see
  `/usr/i686-w64-mingw32/include/stdlib.h`. Need to change `test_args.c`.

The memory management APIs in Windows are:
* VirtualAlloc(): replace anonymous mmap()
* VirtualProtect(): can replace mprotect()
* VirtualLock(): replace mlock()

### Wine

We can likely install wine on Debian and test first. 

Install
```sh
sudo dpkg --add-architecture i386
sudo apt-get update
sudo aptitude install wine wine32 wine64
```

Git `a7fa77062`, branch `xmhf64-win`, test result is
* `test.exe`
	* Runs well on x64 and x86
* `test_args.exe`
	* Runs well on x86
	* VMEXIT-EXCEPTION in x64 (did not translate calling convention)
* `main.exe`
	* Did not test

### Testing x86
Later tested x86 PAL x86 Windows 10 {x86,x64} XMHF on HP, everything runs well.

### Calling convention

## Fix

`a2fe5a973..a7fa77062`
* Allow RAND_MAX to be 0x7fff (to support MinGW)
* Update Makefile to set Windows macro
* Use Windows memory APIs

