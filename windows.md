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
