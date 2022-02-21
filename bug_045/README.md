# udelay() may overflow

## Scope
* All platforms
* Git `81563bccd`

## Behavior
A long time ago git commit `92e3e641c1` was made to fix overflow when
calculating `latchregval`. But there are still problems
* `xmhf_baseplatform_arch_x86_udelay()` is fixed, but not `udelay()` in `init.c`
* When `usecs` is too large, should throw an error

## Debugging

Git `1e0387a58` used to fix `udelay()` in `init.c`.

Can see that usecs should be less than or equal to 54925
```
(%i1) float(solve(1193182 * usecs / 1000000 = 65536, usecs));
(%o1)                     [usecs = 54925.40115422459]
(%i2) 
```

Git `72356c5a7` is used to implement check of overflow in `latchregval`.

## Fix

`81563bccd..72356c5a7`
* Fix unsigned overflow in udelay() in init.c
* Check for overflow of latchregval in udelay()

