## Debian

### Download ISO
<https://cdimage.debian.org/debian-cd/current/i386/iso-cd/debian-11.1.0-i386-netinst.iso>

### QEMU
```sh
qemu-img create -f qcow2 a.qcow2 4G
qemu-system-i386 -m 512M \
	--drive media=cdrom,file=debian-11.1.0-i386-netinst.iso,index=1 \
	--drive media=disk,file=a.qcow2,index=2 \
	--enable-kvm \
	-device e1000,netdev=net0 -netdev user,id=net0,hostfwd=tcp::1126-:22 \
	-smp 4 -serial stdio -cpu Haswell,pae=no,vmx=yes
```

Disable PAE: `--cpu=qemu32,pae=no`

### Install packages
```sh
apt-get install -y sudo    (others same as Ubuntu)
```

### Install non-PAE kernel
```sh
apt-get install -y linux-image-686
```

### Compile XMHF
Warnings generated because high gcc version. Manually fix the code

### Serial port
```sh
$ sudo dmesg | grep ttyS
[    1.472013] 00:04: ttyS0 at I/O 0x3f8 (irq = 4, base_baud = 115200) is a 16550A
```

Change grub line to the following, then can see XMHF output from serial port
```
	multiboot ${kernel} serial=115200,8n1,0x3f8
```

### Changing environment
* Normal TrustVisor
	* Receive error `FATAL ERROR: TXT not suppported`
* Disable DRT and use helloworld (a simpler hypapp)
	* QEMU CPU does not support virtualization
		* Receive error `CPU(0x01) does not support VT. Halting!`
	* QEMU CPU does supports virtualization
		* Receive error `CPU(0x00): VMWRITE failed. HALT!`

### Conclusion on QEMU
Ref:
<https://stackoverflow.com/questions/39154850/how-do-i-emulate-the-vmx-feature-with-qemu>

Looks like `vmx` support in QEMU is implemented by nested vmx (the Ben-Yehuda
paper), and QEMU does not support emulating vmx. Also since XMHF does not work
out of the box, using QEMU is deprecated.

### Debian 11 on HP

Get unhandled exception when OS is booting in XMHF
```
[00]: unhandled exception d, halting!
[00]: state dump follows...
[00] CS:EIP 0x0008:0x10206c01 with EFLAGS=0x00010016
[00]: VCPU at 0x10244aa0
[00] EAX=0x00000491 EBX=0xcab88360 ECX=0x00000491 EDX=0xffffc0b0
[00] ESI=0x10244aa0 EDI=0x0000007f EBP=0x1994bf1c ESP=0x1994bf0c
[00] CS=0x0008, DS=0x0010, ES=0x0010, SS=0x0010
[00] FS=0x0010, GS=0x0010
[00] TR=0x0018
[00]-----stack dump-----
...
```

