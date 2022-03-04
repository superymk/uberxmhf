# Possible regression in PAL execution speed

## Scope
* HP, x64 XMHF, x64 Debian, x64 PAL, no DRT
* Not sure about other configurations
* Git `4d6217bd5`

## Behavior

When running PAL in Debian, enter `./test_args64 7 7 7`, then GUI freezes.
After the test completes, the GUI becomes normal. For an earlier version
I think GUI does not freeze. Note that the TrustVisor serial debug output is
on (so debugger machine see a lot of logs).

## Debugging

```sh
git bisect start
git bisect bad 4d6217bd5
git bisect bad 6733ca433
git bisect bad 3b199dbe0
```

I think this is not regression. Earlier I have been testing over SSH, and I can
see the dots come out smoothly. Then I don't remember that I have not tested
on GUI. When I test on GUI, things are not smooth and I thought it was a
regression.

## Result

Not a regression. No need to fix.

