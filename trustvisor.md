# TrustVisor

## Reverse engineer its calling convention

Use gdb, break at `vmcall()`, see list of VMCALLs

* `vmcall (eax=1, ecx=3221222488, edx=0, esi=3221222308, edi=134529024)`
	* eax = `TV_HC_REG`
	* ecx = pageinfo (`struct tv_pal_sections *`)
		* `CODE`: 134529024, 10 pages
		* `DATA`: 134569984, 12 pages
		* `STACK`: 134643712, 1 page
		* `PARAM`: 134647808, 1 page
	* edx = 0
	* esi = params (`struct tv_pal_params *`)
		* `INTEGER`, size 1 (not used)
		* `INTEGER`, size 1 (not used)
		* `INTEGER`, size 1 (not used)
		* `POINTER`, size 1
	* edi = entry = 0x804c000 <hellopal>
* `vmcall (eax=6, ecx=134529024, edx=134668864, esi=134668880, edi=2)`
	* eax = `TV_HC_SHARE`
	* ecx = entry = 0x804c000 <hellopal>
	* edx = addrs (`u32[]`)
	* esx = lens (`u32[]`)
	* eix = count
* Call PAL function (134529024)
* `vmcall (eax=2, ecx=134529024, edx=0, esi=0, edi=0)`
	* eax = `TV_HC_UNREG`
	* ecx = entry = 0x804c000 <hellopal>
	* edx = 0
	* esx = 0
	* eix = 0

## Demo

Difficult to write a ld script, so use dynamic memory allocation instead.

Working demo in `hypapps/trustvisor/pal_demo/`, only works in x86.

