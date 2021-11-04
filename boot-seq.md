# XMHF boot sequence
init_start (entry) ->
cstartup()
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

