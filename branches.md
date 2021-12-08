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
		* <del>`xmhf64-long`: set up long mode in secureloader</del>
		* <del>`xmhf64-acpi`: get into x64 runtime</del>
		* `xmhf64-ap`

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
`7e142744b..7bf294603`
* Transition from protected mode to long mode in beginning of secure loader
	* <https://www.gnu.org/software/grub/manual/multiboot/multiboot.html>
	* <https://wiki.osdev.org/Setting_Up_Long_Mode>
	* <https://0xax.gitbooks.io/linux-insides/content/Booting/linux-bootstrap-4.html>
	* <https://wiki.osdev.org/Creating_a_64-bit_kernel#Loading>
	* Intel v3 9.8.5 Initializing IA-32e Mode
	* Paging: Intel v3 chapter 4
	* MSR (cr0, cr3, cr4): Intel v3 2.5 CONTROL REGISTERS
	* MSR (EFER): Intel v3 2.2.1 Extended Feature Enable Register
	* GDT entries: Intel v3 3.4.5 Segment Descriptors
	* In x64 segment base=0: Intel v1 3.4.2.1 Segment Registers in 64-Bit Mode
* x86 use GDT to create VA to PA offset -> x86_64 use page table
* Implement spinlock in x64
* For SLPB (passing parameter from x86 bootloader to x86_64 secure loader), use
  32-bit data types, and cast in secure loader
* Fix stdarg in x64 (so that printf can work correctly)

### `xmhf64-acpi`
`7bf294603..4e1ddca98`
* Support accessing low physical memory (e.g. 0x40e)
	* e.g. ACPI problem in x64: `xmhf_baseplatform_arch_x86_acpi_getRSDP`
	* Solution: map these memory at high end of the page table
* Change run time GDT TSS to 64-bits in `xmhf_sl_arch_xfer_control_to_runtime()`
	* TSS ref: Intel v3 7.2.3 TSS Descriptor in 64-bit mode
* 4-level identity paging in `xmhf_sl_arch_x86_64_setup_runtime_paging()`
* Port exception handler (IDT) to 64 bits
	* Intel v3 6.3 SOURCES OF INTERRUPTS
	* Intel v3 6.10 INTERRUPT DESCRIPTOR TABLE (IDT)
	* Intel v3 6.14 EXCEPTION AND INTERRUPT HANDLING IN 64-BIT MODE
	* Intel v3 6.12.1 Exception- or Interrupt-Handler Procedures (stack content)
	* Find out CPU's LAPIC ID: <https://wiki.osdev.org/APIC>
* Other Reference
	* E820: <https://wiki.osdev.org/Detecting_Memory_(x86)>, or ACPI standard
* EPT: similar to 4-level paging, but some fields differ
	* Intel v3 27.2 THE EXTENDED PAGE TABLE MECHANISM (EPT)

### `xmhf64-ap`
`386d791fd..`
* AP bootstrap
	* <https://wiki.osdev.org/Symmetric_Multiprocessing#AP_startup>
	* Intel v3 8.4.4 MP Initialization Example
	* Intel v3 10.6.1 Interrupt Command Register (ICR)
	* Intel v3 10.5.3 Error Handling
	* <https://stackoverflow.com/questions/70147401/>
	* Use `objdump -m i8086 -Sd runtime.exe` to view real mode code
* Rename functions from `*x86*` to `*x86_64*`, using `replace_x86.py` for help

### TODO
* Review unaligned structs caused by `__attribute__((packed))`
* Decide a coding format.

