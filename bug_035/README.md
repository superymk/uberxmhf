# Windows 10 cannot detect all 4 CPUs

## Scope
* Windows 10, (maybe also Windows XP)

## Behavior
After booting Windows 10 in QEMU, Task manager and resource manager only show
2 CPUs. In gdb, CPU 0 and CPU 1 are running Windows. CPU 2 and CPU 3 are
waiting for SIPI.

## Debugging

Looks like this may be a problem with Windows and QEMU. When booting Windows 10
x64 without XMHF in QEMU, see the same behavior. In GDB it shows:
```
(gdb) info th
  Id   Target Id                    Frame 
* 1    Thread 1.1 (CPU#0 [halted ]) 0xfffff8006b20bfaf in ?? ()
  2    Thread 1.2 (CPU#1 [halted ]) 0xfffff8006b20bfaf in ?? ()
  3    Thread 1.3 (CPU#2 [halted ]) 0x00000000000fd0ad in ?? ()
  4    Thread 1.4 (CPU#3 [halted ]) 0x00000000000fd0ad in ?? ()
(gdb) 
```

After reading
<https://serverfault.com/questions/101434/why-does-my-windows-7-vm-running-under-linux-kvm-not-use-all-the-virtual-proces>,
maybe we need to change number of cores / sockets in QEMU's `-smp` config.

`info hotpluggable-cpus` in QEMU monitor shows:
```
(qemu) info hotpluggable-cpus 
Hotpluggable CPUs:
  type: "Haswell-x86_64-cpu"
  vcpus_count: "1"
  qom_path: "/machine/unattached/device[4]"
  CPUInstance Properties:
    socket-id: "3"
    core-id: "0"
    thread-id: "0"
  type: "Haswell-x86_64-cpu"
  vcpus_count: "1"
  qom_path: "/machine/unattached/device[3]"
  CPUInstance Properties:
    socket-id: "2"
    core-id: "0"
    thread-id: "0"
  type: "Haswell-x86_64-cpu"
  vcpus_count: "1"
  qom_path: "/machine/unattached/device[2]"
  CPUInstance Properties:
    socket-id: "1"
    core-id: "0"
    thread-id: "0"
  type: "Haswell-x86_64-cpu"
  vcpus_count: "1"
  qom_path: "/machine/unattached/device[0]"
  CPUInstance Properties:
    socket-id: "0"
    core-id: "0"
    thread-id: "0"
(qemu) 
```

Maybe Windows only supports 2 sockets. So we try `-smp cpus=4,sockets=2`.
See `man qemu-system-x86_64`, grep for `-smp`.

After the change, from Resource Manager and GDB can see that Windows uses all 4
CPUs. But QEMU becomes very slow.

## Result

This is a configuration problem in QEMU and a limit in Windows 10. Should use
`-smp cpus=4,sockets=2` instead of `-smp 4`.

