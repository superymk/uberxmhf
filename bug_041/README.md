# PAL exception in Fedora test

## Scope
* HP, x64 Fedora, x64 XMHF, x86/x64 PAL
* good: HP, Debian
* (should be good): QEMU, x64 Fedora

## Behavior
When running `./test_args32 7 7 7` or `./test_args64 7 7 7`, see (serial
`20220211220934`):
```
VMEXIT-EXCEPTION:
control_exception_bitmap=0xffffffff
interruption information=0x80000b0e
errorcode=0x00000004
```

Looks like reproducible every time.

## Debugging

TODO
