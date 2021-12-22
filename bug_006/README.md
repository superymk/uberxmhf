# HP cannot run tee-sdk demo (IN PROGRESS)

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

## Debugging

TODO

Maybe not going to support x86 appliaction in x64 XMHF

