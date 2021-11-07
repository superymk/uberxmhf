# XMHF code base structure

* `hypapps/trustvisor`: The hypapp we are focusing on
	* TODO
* `xmhf`
	* `src`
		* `libbaremetal`
			* TODO
		* `xmhf-core`
			* `include` (less interested)
			* `verification` (less interested)
			* `xmhf-bootloader`
				* `cmdline.c`
				* `header.S`: Entry for `init-x86.bin` (`init_start`), set stack
				* `init.c`: C entry for `init-x86.bin` (`cstartup`)
				* `init.lds.S`: Linker script for `init-x86.bin`
				* `initsup.S`
				* `Makefile`
				* `smp.c`: TODO
				* `txt_acmod.c`
				* `txt.c`: Functions for booting with Intel TXT
				* `txt_hash.c`
				* `txt_heap.c`
			* `xmhf-runtime`
				* TODO
			* `xmhf-secureloader`
				* TODO
	* `third-party`
		* TODO

