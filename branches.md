# Branches in <https://github.com/lxylxy123456/uberxmhf>

* `master`: Forked from <https://github.com/uberspark/uberxmhf>
	* `lxy-compile`: Fix compile errors on Debian 11
* `notes`: Notes when studying XMHF; does not contain code
* `xmhf-latest`: XMHF code of `lxy-compile`, but v0.2.2 directory structure
	* `xmhf64`: Main development branch
		* <del>`xmhf64-no-drt` (ea33d732f): Make trustvisor runnable with
		  `--disable-drt`</del>
		* <del>`xmhf64-comp` (62c6eb8c6): Make trustvisor compilable in 64
		  bits</del>
		* `xmhf64-long`: set up long mode in bootloader 

## Change Log

### `xmhf64-no-drt`
`bc29c25e4..ea33d732f`
* Disable call to `trustvisor_master_crypto_init()`
* Disable call to `utpm_extend()`

### `xmhf64-comp`
`0f069fa78..62c6eb8c6`
* Add `__X86__`, `__X86_64__`, `__XMHF_X86__`, and `__XMHF_X86_64__` and macros
* Port `xmhf/src/libbaremetal/libxmhfc/include/sys/ia64_*.h`, similar to `i386*`
* When casting to/from pointers and uints, change from uint32 to `uintptr_t`
  or equivalent
* Remove assembly instructions that do not compile (e.g. `pusha`)
* Remove some function definitions when `__DRT__` is not defined
* Add uint definitions for pointers: `hva_t`, `spa_t`, `gva_t`, `gpa_t`, `sla_t`
* Modify Makefile to compile bootloader in x86, others in x86_64
* Compile some object files twice to support x86 bootloader

### `xmhf64-long`
Reason for entering long mode in secure loader:
in bootloader addresses are temporary; may be overwritten later.
* References
	* <https://wiki.osdev.org/Setting_Up_Long_Mode>
	* Intel v3 9.8.5 Initializing IA-32e Mode
	* Paging: Intel v3 chapter 4
	* MSR (cr0, cr3, cr4): Intel v3 2.5 CONTROL REGISTERS
	* MSR (EFER): Intel v3 2.2.1 Extended Feature Enable Register
	* GDT entries: Intel v3 3.4.5 Segment Descriptors

### TODO
* !!! `hva_t` in `_sl_parameter_block` causes problems.
  Likely to split header file? Or add `__BOOTLOADER__`.
* Review unaligned structs caused by `__attribute__((packed))`
* Decide a coding format.

