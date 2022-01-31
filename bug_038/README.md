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

