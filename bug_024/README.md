# HP running `pal_demo` not stable, likely Linux stuck

## Scope
* HP only
* Reproducible x64, not tested on x86

## Behavior
When running `bug_018`'s test 2 on HP, sometimes the machine gets stuck

For example, serial output is `20220105212903`

```
for i in {1..1000..2}; do ./main $i $i; done & for i in {0..1000..2}; do ./main $i $i; done
```

## Debugging

There is exactly one drop NMI message printed: `CPU(0x04): drop NMI`. Does
Linux crash etc when NMI is not delivered correctly?

TODO
