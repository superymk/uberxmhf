# XMHF boot sequence
(Currently this document is not technically markdown)

## `xmhf-bootloader`
init_start (entry) ->
cstartup() (early init)
 Print banner (AMT 3-4)
 Detect Intel / AMD CPU (AMT 7)
 Intel CPU -> find SINIT module (AMT 7-8)
 Call dealwithMP(): find number of cores (AMT 10-34)
  Find ACPI RSDP -> RSDT -> MADT -> APIC record -> find processor records
  Store to global variable `pcpus` and `pcpus_numentries`
 dealwithE820(): arrange memory
  Get Grub's memory map to `grube820list`, print them (AMT 35-53)
  Split a region starting at `__TARGET_BASE_SL`, update table, print (AMT 54-76)
 Move hypervisor module to `__TARGET_BASE_SL`
 Print hash of runtime and SL (AMT 77-90)
 Print stats (AMT 92-94)
 Fill parameter block `SL_PARAMETER_BLOCK *slpb` (address fixed)
  modified memmap, detected CPUs, location of grub (for recursive loading)
 Switch to MP mode
  Set up CPU stack (AMT 95-100)
  Entry is address `0x10000`, code is `_ap_bootstrap_start`
  Wake all CPUs (AMT 101-107)
  Jump to `init_core_lowlevel_setup`
AP -> _ap_bootstrap_start
 Jump to `init_core_lowlevel_setup`
AP and BSP -> init_core_lowlevel_setup
 Search for this CPU in midtables, jump to `mp_cstartup()`
mp_cstartup
 AP: wait for DRTM establishment, halt (AMT 103-105)
 BSP: wait for all APs to be ready, jump to `do_drtm()` (AMT 108-109)
do_drtm()
 If MP: call `send_init_ipi_to_all_APs()` (AMT 110-113)
 with DRT: call `txt_do_senter()`
  `txt_verify_platform()`
   `txt_support_txt()`: check hardware support (AMT 119-123)
   `verify_bios_data()`: check bios (AMT 124-130)
  `txt_prepare_cpu()`: set registers for CPU (AMT 131-133)
  jump to `txt_launch_environment()`
   `check_sinit_module()`: (AMT 134-138)
   `copy_sinit()`: (AMT 139)
   `verify_acmod()`: (AMT 140-183)
   `print_file_info()`: (AMT 184-185)
   `print_mle_hdr()`: (AMT 186-198)
   `build_mle_pagetable()`: (AMT 199-234)
   `init_txt_heap`: (AMT 235-258)
   `save_mtrrs()`: (AMT 259-269)
   `set_mtrrs_for_acmod()`: (AMT 270)
   Print `executing GETSEC[SENTER]...`: (AMT 271)
   jump to `__getsec_senter()` (never returns)
 without DRT: jump to SL + (*SL) (`_sl_start`)

## `xmhf-secureloader`
_sl_start
 test CPU vendor
 modify GDT and load GDT
 jump to `xmhf_sl_main`
xmhf_sl_main()
 Print `slpb` from `cstartup()` (AMT 273-283)
 Set up rbp (AMT 286-287)
 `xmhf_baseplatform_initialize()`: check PCI and ACPI (AMT 288-290)
 `xmhf_sl_arch_sanitize_post_launch()`: ? (AMT 291-293)
 `xmhf_sl_arch_xfer_control_to_runtime()`: (AMT 294 - 298)
  construct page table
  `xmhf_sl_arch_x86_invoke_runtime_entrypoint`: enable paging, jump to runtime

## `xmhf-runtime`
xmhf_runtime_entry()
 Dump info (AMT 299-327)
 `xmhf_xcphandler_initialize()`: set up exception handler (AMT 328-329)
 Set up DMA protection (AMT 330-331)
 Jump to `xmhf_baseplatform_smpinitialize()`
xmhf_baseplatform_smpinitialize()
 Set up Master-ID table
 `xmhf_baseplatform_arch_x86vmx_allocandsetupvcpus()`: Set up VCPU table
 `xmhf_baseplatform_arch_x86vmx_wakeupAPs()`: Wake up APs (AMT 332-334)
  Performs MLE
  AP will start at `_ap_bootstrap_start`
 Jump to `_ap_pmode_entry_with_paging`
AP -> `_ap_bootstrap_start`
 Load GDT, enter protected mode
AP and BSP -> `_ap_pmode_entry_with_paging`
 Call `xmhf_baseplatform_arch_x86_smpinitialize_commonstart()` with vcpu ptr
xmhf_baseplatform_arch_x86_smpinitialize_commonstart() (AMT 336-339, 358-359)
 BSP -> Wait for all APs to wake, then start all APs
 AP -> send wake to BSP, then wait for start
 Jump to `xmhf_runtime_main`
xmhf_runtime_main()
 `xmhf_baseplatform_cpuinitialize()`: initialize CPU (AMT 340-343, 360-363)
 `xmhf_partition_initializemonitor()`: initialize hypervisor, VMXON
                                       (AMT 344-353, 355, 357, 364-373)
 `xmhf_partition_setupguestOSstate()`: setup VMCS (AMT 378)
  BSP will jump to `XtGuestOSBootModuleBase` when VMLAUNCH
 `xmhf_memprot_initialize()`: setup EPT
                              (AMT 354, 356, 374-377, 379-381, 384, 386-387)
 Call `xmhf_app_main()` (e.g. TrustVisor) (AMT 381-487)
  See next section
 Wait for all cores to exit `xmhf_app_main()` (AMT 383, 389, 391, 489-490)
 `xmhf_smpguest_initialize()`: init SMP? (AMT 491)
  BSP -> continue
  AP -> wait until `processSIPI()` in `After VMLAUNCH`, then continue
 `xmhf_partition_start()`: start HVM, VMLAUNCH (AMT 493-495, 637, 639, 641, 643,
                                                652, 656-658, 667-670)
 // NewBluePill page 31 figure 3.4 is helpful
 // Never returns

## `hypapps/trustvisor`
xmhf_app_main()
 Print useless info (AMT 381, 384, 387, 389, 391, 393)
 `parse_boot_cmdline()` (AMT 394-397)
 `init_scode()`
  Allocate space (AMT 398-401)
  `trustvisor_master_crypto_init()`
   `xmhf_tpm_open_locality()`: (AMT 403-411)
   `master_prng_init()`: (AMT 412-413)
   `trustvisor_long_term_secret_init()`: (AMT 414-454)
   `validate_trustvisor_nv_region()`: (AMT 455-484)
   `xmhf_tpm_deactivate_all_localities()` (AMT 486)

## After VMLAUNCH
_vmx_int15_handleintercept(): E820? (AMT 496-628)
xmhf_parteventhub_arch_x86vmx_entry
 xmhf_parteventhub_arch_x86vmx_intercept_handler()
  xmhf_smpguest_arch_x86_eventhandler_dbexception
   xmhf_smpguest_arch_x86vmx_eventhandler_dbexception():
    Send SIPI to other cores (AMT 629-631, 636, 644-646, 651, 659-661)
    processSIPI(): software IPI
                   (AMT 632-635, 638, 640, 642, 647-650, 653-655, 662-665)
    Remove SIPI (AMT 666)

