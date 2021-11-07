# XMHF boot sequence
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
 without DRT: jump to SL + (*SL)
