# HP Regression when running PAL

## Scope
* bad: HP, x64 XMHF, x64 Debian, x86 PAL
* good: HP, x86 XMHF, x86 Debian, x86 PAL
* good: QEMU

## Behavior
On commit `8d0fa2d1b`, HP x64 cannot run `pal_demo`. Running `./main 1 2` will
show
```
...
TV[0]:pages.c:pagelist_get_page:72:                num_used:4 num_alocd:127
TV[0]:pages.c:pagelist_get_page:72:                num_used:5 num_alocd:127
TV[0]:appmain.c:tv_app_handleintercept_hwpgtblviolation:584: CPU(0x01): gva=0xf7f61006, gpa=0x317af006, code=0x4
TV[0]:scode.c:hpt_scode_npf:1219:                  CPU(01): nested page fault!(rip 0xf7f61006, gcr3 0x102385804, gpaddr 0x317af006, errorcode 4)
TV[0]:scode.c:scode_in_list:196:                   find gvaddr 0xf7f61006 in scode 0 section No.0
TV[3]:scode.c:hpt_scode_npf:1230:                  EU_CHK( ((whitelist[*curr].entry_v == rip) && (whitelist[*curr].entry_p == gpaddr))) failed
TV[3]:scode.c:hpt_scode_npf:1230:                  Invalid entry point
TV[0]:appmain.c:tv_app_handleintercept_hwpgtblviolation:587: FATAL ERROR: Unexpected return value from page fault handling
```

## Debugging

From testing result in `bug_018`, likely `b04ca351a` is good. The git histories
are:
```
* 8d0fa2d1b Check NMI interception in NMI window handler to prevent injecting NMI to PAL
* 582a35cd5 Extend EPT to 8GB
* 9c5f688f3 Add error when E820 has entry exceed MAX_PHYS_ADDR
* 850af783b Extend runtime paging to 8GB
* 63a196411 Fix e820 not able to detect last entry error
* d26673ba8 Optimize __vmx_vmread and __vmx_vmwrite by giving compiler more freedom
* 7715f3f03 Print error code in exception handler
* fc92858ff Fix typo in NMI injection code
* b04ca351a Add gcc -mno-red-zone to prevent vmread fail
```

I suspect that the EPT commits are causing the problem. Try reverting them.
`63a196411`, `850af783b`, `9c5f688f3`, `582a35cd5`.

After reverting, looks good.
* This also confirms that test 2 in x64 XMHF, x64 Debian passes. So `bug_021`
  is fixed well by `8d0fa2d1b`

Now try to revert only `63a196411`.
* If good, then problem is likely accessing >= 4GB memory (problem not fixed
  completely)
* If bad, then there is a problem with constructing the page tables (the
  other 3 commits have a bug)

Test result is good, so `bug_020` is not fixing all the problems.

### Reproducing on QEMU

Before this bug I always / almost always give QEMU 512MB memory, using
`-m 512M`. If change to `-m 5G`, can reproduce this bug easily.

Can debug using gdb:
```
source gdb/runtime.gdb
hb tv_app_handleintercept_hwpgtblviolation
c
```

GDB shows that the `gpaddr` argument to `hpt_scode_npf` is truncated to
`u32`, which causes the mismatch.
```
(gdb) l
1224	
1225	  index = scode_in_list(gcr3, rip);
1226	  if ((*curr == -1) && (index >= 0)) {
1227	    /* regular code to sensitive code */
1228	
1229	    *curr = index;
1230	    EU_CHK( ((whitelist[*curr].entry_v == rip)
1231	             && (whitelist[*curr].entry_p == gpaddr)),
1232	            eu_err_e("Invalid entry point"));
1233	
(gdb) 
1234	    /* valid entry point, switch from regular code to sensitive code */
1235	    EU_CHKN( hpt_scode_switch_scode(vcpu));
1236	
1237	  } else if ((*curr >=0) && (index < 0)) {
1238	    /* sensitive code to regular code */
1239	
1240	    EU_CHK( RETURN_FROM_PAL_ADDRESS == rip,
1241	            eu_err_e("SECURITY: invalid scode return point!"));
1242	
1243	    /* valid return point, switch from sensitive code to regular code */
(gdb) p/x whitelist[0].entry_p
$22 = 0x11331c006
(gdb) p/x whitelist[0].entry_v
$23 = 0xf7fa8006
(gdb) p/x rip
$24 = 0xf7fa8006
(gdb) p/x gpaddr
$25 = 0x1331c006
(gdb) 
```

GDB allows tracing the stack easily. The problem originates in XMHF code for
`_vmx_handle_intercept_eptviolation()`:
```
421  //---intercept handler (EPT voilation)----------------------------------
422  static void _vmx_handle_intercept_eptviolation(VCPU *vcpu, struct regs *r){
423  	u32 errorcode, gpa, gva;
424  	errorcode = (u32)vcpu->vmcs.info_exit_qualification;
425  	gpa = (u32) vcpu->vmcs.guest_paddr_full;
426  	gva = (u32) vcpu->vmcs.info_guest_linear_address;
427  
428  	//check if EPT violation is due to LAPIC interception
429  	if(vcpu->isbsp && (gpa >= g_vmx_lapic_base) && (gpa < (g_vmx_lapic_base + PAGE_SIZE_4K)) ){
430  		xmhf_smpguest_arch_x86_64_eventhandler_hwpgtblviolation(vcpu, gpa, errorcode);
431  	}else{ //no, pass it to hypapp
432  		xmhf_smpguest_arch_x86_64vmx_quiesce(vcpu);
433  		xmhf_app_handleintercept_hwpgtblviolation(vcpu, r, gpa, gva,
434  				(errorcode & 7));
435  		xmhf_smpguest_arch_x86_64vmx_endquiesce(vcpu);
436  	}
437  }
```

In line 425, gpa is casted to u32. This is incorrect. Also should not only read
`vcpu->vmcs.guest_paddr_full`, but also read `vcpu->vmcs.guest_paddr_high`.

Maybe better to change VMCS definitions?

After change, test 2 (from `bug_018`) works well on QEMU and HP

However, test 3 on HP breaks? (will another bug)

## Fix

`8d0fa2d1b..1c7ec32a2`
* Fix gpa truncated bug in `_vmx_handle_intercept_eptviolation`

