# Branches in <https://github.com/lxylxy123456/uberxmhf>

* `master`: Forked from <https://github.com/uberspark/uberxmhf>
	* `lxy-compile`: Fix compile errors on Debian 11
* `notes`: Notes when studying XMHF; does not contain code
* `xmhf-latest`: XMHF code of `lxy-compile`, but v0.2.2 directory structure
	* `xmhf64`: Main development branch
		* <del>`xmhf64-no-drt` (ea33d732f): Make trustvisor runnable with
		  `--disable-drt`</del>
		* `xmhf64-comp`: Make trustvisor compilable in 64 bits

## Change Log

### `xmhf64-no-drt`
* Disable call to `trustvisor_master_crypto_init()`
* Disable call to `utpm_extend()`

### `xmhf64-comp`
* Add `__X86_64__` and `__X86__` macros
* Port `xmhf/src/libbaremetal/libxmhfc/include/sys/ia64_*.h`, similar to `i386*`
* When casting to/from pointers and uints, change from uint32 to `uintptr_t`
  or equivalent
* Remove assembly instructions that do not compile (e.g. `pusha`)
* Remove some function definitions when `__DRT__` is not defined
* Add uint definitions for pointers: `hva_t`, `spa_t`, `gva_t`, `gpa_t`, `sla_t`

