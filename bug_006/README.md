# HP cannot run tee-sdk demo

## Scope
* HP, Ubuntu 12.04, x86-64
* Maybe on QEMU

## Behavior
After booting, run
```
cd ~/uberxmhf/xmhf/hypapps/trustvisor/tee-sdk/examples/newlib
./hello
```

See that the process does not return; AMT term shows error.

Expected behavior: shell below, serial in `newlib32.txt`
```
$ ./hello 
warning, couldn't lock scode section 1 into physical memory
getting pages swapped in and hoping for the best...
$ 
```

Actual behavior: shell below, serial in `newlib64.txt`
```
$ ./hello 
warning, couldn't lock scode section 1 into physical memory
getting pages swapped in and hoping for the best...
```

Can also reproduce by running `pal_demo`. Compare `demo32.txt` and `demo64.txt`.

## Debugging

Note: maybe not going to support x86 appliaction in x64 XMHF

From AMT term output, 32-bit is 
```
TV[0]:scode.c:scode_marshall:806:                  parameter page base address is 0xb7f92010
```
But 64-bit is
```
TV[0]:scode.c:scode_marshall:806:                  parameter page base address is 0x1
```

So looks like `whitelist[curr].gpmp` is not initialized correctly.
`whitelist_new.gpmp` is initialized in `memsect_info_register()`.

Add a dead loop in `memsect_info_register` to set break point. See `bug_004`.

Then it is copied to `whitelist[0].gpmp` in line 678. At the time of copy,
value is good
```
memcpy(whitelist + i, &whitelist_new, sizeof(whitelist_entry_t));
```

In `scode_marshall()`, whitelist[0].gpmp becomes 1

So set a (data write) watch point in `&(whitelist[0].gpmp)` and continue
```
(at scode.c:691, a little bit after memcpy() above)
(gdb) p &whitelist->gpmp
$10 = (u32 *) 0x10277f54 <memory_pool+6580>
(gdb) watch * (u32 *) 0x10277f54
Hardware watchpoint 5: * (u32 *) 0x10277f54
(gdb) c
```

Then find that `gpmp` is written by
```
923    EU_CHKN( copy_from_current_guest(vcpu,
924                                     &whitelist[curr].return_v,
925                                     VCPU_grsp(vcpu),
926                                     sizeof(void*)));
```

`&whitelist[curr].return_v` is defined as `u32`. When compiled in x86-64,
`sizeof(void*)` at line 926 becomes 8, not 4. So `gpmp` gets overwritten.
```
104  typedef struct whitelist_entry{
...    ...
111    u32 entry_p; /* entry point physical address */
112    u32 return_v; /* return point virtual address */
113  
114    u32 gpmp;     /* guest parameter page address */
115    u32 gpm_size; /* guest parameter page number */
...    ...
148  } __attribute__ ((packed)) whitelist_entry_t;
```

To fix, simply change line 926 to 4 (or `sizeof` equivalent).

## Fix

`d0958afba..9b1777042`
* Change `copy_from_current_guest`'s size from 8 to 4

