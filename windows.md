# Running XMHF and Windows on QEMU

Download Windows 10 installer from Microsoft website.

Install Windows using:
```
qemu-img create -f qcow2 win10x64.qcow2 32G
qemu-system-x86_64 -m 1G \
	--drive media=cdrom,file=Win10_21H2_English_x64.iso,index=1 \
	--drive media=disk,file=win10x64.qcow2,index=2 \
	--enable-kvm \
	-smp 2 -cpu Haswell
qemu-img create -f qcow2 -b win10x64.qcow2 -F qcow2 win10x64-a.qcow2
```

Then use QEMU to load two disks, one for Linux and one for Windows:
```
qemu-system-x86_64 -m 1G \
	--drive media=disk,file=/home/lxy/Documents/GreenBox/tmp/qemu/c4.qcow2,index=1 \
	--drive media=disk,file=win10x64.qcow2,index=2 \
	--enable-kvm \
	-smp 2 -cpu Haswell,vmx=yes -serial stdio
```

Boot into Linux, and run `sudo grub-update`, should detect Windows at
`/dev/sdb1`. Then in GRUB can boot Windows.

A problem is that GRUB will try to search disk / partition with a (likely)
UUID. If we change `/dev/sdb` into another Windows installation, will see
a warning. The solution is to remove the search.

Can write a custom GRUB menu entry at `/etc/grub.d/40_windows`, then disable
`/etc/grub.d/30_os-prober`
```
#!/bin/sh
exec tail -n +3 $0
menuentry 'Windows' --class windows --class os $menuentry_id_option 'windows' {
	insmod part_msdos
	insmod ntfs
	set root='hd1,msdos1'
	parttool ${root} hidden-
	drivemap -s (hd0) ${root}
	chainloader +1
}
```

### WinDbg

Using tutorial from <https://www.cnblogs.com/csnd/p/11800535.html>
Downloaded zip file "Windbg XP 安装.zip"

Install `dbg_x86.msi`

Install `WindowsXP*symbols-full-ENU.exe` (to `C:\WINDOWS\Symbols`)

May need to copy `C:\WINDOWS\Symbols\*\*.pdb` to `C:\WINDOWS\Symbols\`

In WinDbg, select `File -> Kernel debug`, debug local kernel (for convenience)

```
!sym noisy
.sympath C:\WINDOWS\Symbols
.reload
```

Want to set break point, need to debug remote machine.

Refs
* <https://stackoverflow.com/questions/12513205/remote-debugging-using-windbg>
* <https://web.archive.org/web/20130605113813/http://blogs.msdn.com/b/ntdebugging/archive/2009/02/09/remote-debugging-connecting-to-a-remote-stub-using-the-microsoft-debugging-tools-for-windows.aspx>
* <https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/process-server-examples>

In kernel debug, if select COM / serial port, on QEMU can see it writes
to serial port a lot of "i"s. This should be run on the debugger.

Need to run `dbgsrv.exe` on the debugged machine. This is installed with windbg.

In cmd, syntax should be: `kdsrv.exe -t com:com1,baud=115200`

#### Serial port

In Linux, serial port is `/dev/ttyS{0..3}`. In Windows, is `COM{1..4}`.

In QEMU, can specify multiple `-serial` arguments. For example,
`-serial file:/tmp/a -serial stdio` means `/dev/ttyS0` is pipe and `/dev/ttyS1`
is stdio.

QEMU's `-serial file:` does not allow input, so we can use UDP. Should be able
to connect two computers together using `-serial udp::1152@:5211` on one
machine and `-serial udp::5211@:1152` on another. Or use TCP (as in tutorials):
`-serial tcp::11520,server,nowait` (debugger) and `-serial tcp:localhost:11520`
(debugee)

In extreme case, can use Wireshark to packet capture communication.

Refs:
* <https://resources.infosecinstitute.com/topic/kernel-debugging-qemu-windbg/>
* <http://mangoprojects.info/virtualization/virtual-serial-connection-between-two-qemu-vm/>
* <https://stackoverflow.com/questions/30049300/windbg-first-connect-then-stuck-on-debuggee-not-connected-message-during-kern>

First start debugger, and WinDbg, then start debugee.

Will see some information on WinDbg, and "Debugee not connected"

If hit "Debug -> Break", can stop the debugee

