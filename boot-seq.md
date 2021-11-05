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
 Different code for with / without MP / DRT
 
