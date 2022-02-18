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

Using <https://en.wikipedia.org/wiki/X86_calling_conventions>, can see
Windows's calling conventions

| Register name | Windows | Linux   |
|---------------|---------|---------|
| RAX           | return  | return  |
| RBX           | callee  | callee  |
| RCX           | arg1    | arg4    |
| RDX           | arg2    | arg3    |
| RSI           | callee  | arg2    |
| RDI           | callee  | arg1    |
| RBP           | callee  | callee  |
| RSP           | callee  | callee  |
| R8            | arg3    | arg5    |
| R9            | arg4    | arg6    |
| R10           | caller  | caller  |
| R11           | caller  | caller  |
| R12           | callee  | callee  |
| R13           | callee  | callee  |
| R14           | callee  | callee  |
| R15           | callee  | callee  |

We can write assembly functions to translate between Windows and Linux calling
conventions.

### WineDbg

From
<https://wiki.winehq.org/Wine_Developer%27s_Guide/Debugging_Wine#Other_debuggers>,
looks like we can debug Windows executables in Wine.

```sh
winedbg --gdb --no-start test_args.exe 7 7 7
```

The command will output a line like `target remote localhost:12345`. This
indicates the port for GDB to connect. Then can break at main. Debug symbols
are automatically loaded.

Debian gdb may need
```
set architecture i386:x86-64
symbol-file test_args.exe
```

Fixed some bugs in `221e7b84b..057309e3f`. Now using x64 Debian to test,
`wine test_args.exe 4 1 1` will cause test failure. Looks like there will be a
problem if and only if `args_i[1] != 0`.

The cause is that in x64 Windows `long` is 4 bytes, should use `uintptr_t`
(8 bytes).

After that, looks like problem fixed.

## Fix

`a2fe5a973..a7fa77062`, `221e7b84b..5b5cd7931`
* Allow RAND_MAX to be 0x7fff (to support MinGW)
* Update Makefile to set Windows macro
* Use Windows memory APIs
* Write assembly code to translate x64 Windows calling convention to Linux
* Fix bug in hpt_scode_switch_regular() that cause unaligned RSP in x64
* Use `uintptr_t` instead of `long` to get correct size
* Support munmap memory in caller.c

