# Support waking APs in DRT

## Scope
* HP, x64 XMHF, DRT
* Good: x86 XMHF
* Good: no DRT
* Git `92a9e94dc`

## Behavior
After fixing `bug_042`, DRT mode can enter runtime, but when BSP wakes up AP,
no AP shows up and BSP waits forever. Serial ends with following.
```
xmhf_xcphandler_arch_initialize: setting up runtime IDT...
xmhf_xcphandler_arch_initialize: IDT setup done.
Runtime: Re-initializing DMA protection...
Runtime: Protected SL+Runtime (10000000-1da07000) from DMA.
BSP: _mle_join_start = 0x1020bec0, _ap_bootstrap_start = 0x1020be70
BSP: joining RLPs to MLE with MONITOR wakeup
BSP: rlp_wakeup_addr = 0x0
Relinquishing BSP thread and moving to common...
BSP rallying APs...
BSP(0x00): My RSP is 0x000000001d987000
```

## Debugging

Note that when no DRT, BSP uses APIC to wake up APs. But now BSP uses something
else.

The difference can be seen in `xmhf_baseplatform_arch_x86_64vmx_wakeupAPs()`.
We need to compare behavior of x86 and x64 XMHF with DRT.

