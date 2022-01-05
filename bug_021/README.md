# Interrupting PAL with NMI results in unhandled exception intercept 

## Scope
* Debian 11 all configurations (QEMU and HP, x86 and x64 XMHF)
* Ubuntu not tested

## Behavior
See also: "Failure A" and "Failure C" in `bug_018`

When running a lot of PALs sequentially and send NMIs to the guest, see
```
VMEXIT-EXCEPTION:
control_exception_bitmap=0xffffffff
interruption information=0x80000b0e
errorcode=0x00000000
```

Sample way to reproduce: boot QEMU x64 XMHF x64 Debian. In one terminal,
connect to Debian and run
```sh
for i in {0..1000}; do ./main $i $i; done
```

In another terminal, run
```sh
python3 bug_018/qemu_inject_nmi.py 4444 1
```

## Debugging

My guess is that when PAL is running, and an NMI is delivered to the PAL, this
problem will happen. First declutter TrustVisor output by changing `EU_LOG_LVL`
to 2. There are 3 files need to be changed.

After decluttering, code is `34c409bf8`, can still reproduce this bug.

Now print all events in `d0797f892`. Can see that the problem happens when the
CPU receives an NMI.
```
CPU(0x00): call,    0x00000000
CPU(0x00): endcall, 0x00000000
CPU(0x00): ept,     0x00000000
CPU(0x00): endept,  0xffffffff
CPU(0x00): VMEXIT-EXCEPTION:
control_exception_bitmap=0xffffffff
interruption information=0x80000b0e
errorcode=0x00000000
```

Can decode "interruption information" using
Intel volume 3 "23.9.2 Information for VM Exits Due to Vectored Events".
* Vector of interrupt or exception = 0x0e (Page fault)
* Interruption type = 3: Hardware exception
* Error code valid = 1 = valid
* NMI unblocking due to IRET = 0
* Valid = 1

My guess should be correct. However, I am not sure why the guest will generate
a page fault (expecting to see EPT violation in host). Also, in `scode.c` can
see that the TrustVisor wants to disable interrupts during sensitive code.

### Experiment

Modify `pal_demo` to busy loop for some time in PAL. Code is `76da0244b`, saved
as `main2`. Then in Debian, use
```
while sleep 0.1; do ./main2 10000 100000; done
```

And observer serial output. After seeing `CPU(0x00): endept,  0xffffffff`,
inject NMI using QEMU. The problem is that NMI seems to get dropped when CPU 0
is running PAL. When other CPUs are running PAL, can see NMI's effect on Linux.
Note that QEMU's injection of NMI is on CPU 0.

Actually this behavior can be explained. In
`xmhf_smpguest_arch_x86_64vmx_eventhandler_nmiexception()`, the code is:
```
	if (quiesce) {
		// process quiesce
		...
	}else{
		//we are not in quiesce, inject the NMI to guest

		if(vcpu->vmcs.control_exception_bitmap & CPU_EXCEPTION_NMI){
			//TODO: hypapp has chosen to intercept NMI so callback
		}else{
			// set NMI windowing
			...
		}
```

Actually the check for NMI being intercepted should happen at NMI windowing
handler.

Race condition analysis:

|CPU 0 Guest|CPU 0 Hypervisor|CPU 0 Exception|Other|
|-|-|-|-|
||||(Example of entering and exiting scode)|
|Call PAL||||
||EPT violation interception|||
||Quiesce|||
||Enter sensitive code|||
||End quiesce|||
||Return from interception|||
|Run PAL||||
||EPT violation interception|||
||Quiesce|||
||Enter normal code|||
||End quiesce|||
|Run normal code||||
|...|...|...|...|
||||(Example of handling NMI injection)|
||||NMI injected|
||NMI exit interception|||
||Set NMI windowing|||
||Return from interception|||
||NMI windowing interception|||
||Inject NMI to guest|||
||Return from interception|||
|Handle NMI||||
|Iret||||
|...|...|...|...|
||||(Race condition)|
|Call PAL||||
||EPT violation interception|||
||||NMI injected|
|||NMI exception||
|||Set NMI windowing||
|||Iret||
||Quiesce|||
||Enter sensitive code|||
||End quiesce|||
||Return from interception|||
||NMI windowing interception|||
||Inject NMI to guest|||
||Return from interception|||
|Handle NMI, but should not||||

As mentioned earlier, this can be fixed by moving the check to NMI windowing
handler. It is implemented in `76da0244b..cdb86cceb` in `xmhf64-nmi` branch.

Checkout to `xmhf64` branch and run `git cherry-pick cdb86cceb` can move the
change from `xmhf64-nmi` branch to `xmhf64` branch.

## Fix

`582a35cd5..8d0fa2d1b`
* Check NMI interception in NMI window handler to prevent injecting NMI to PAL

