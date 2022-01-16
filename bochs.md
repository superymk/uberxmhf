# Bochs

## Logistics

Install
```sh
sudo dnf install bochs
```

Run
```sh
bochs
```

Compile dependencies
```
sudo dnf install libXrandr-devel
```

Compile
```sh
# https://sourceforge.net/projects/bochs/files/bochs/2.7/bochs-2.7.tar.gz/download
wget https://iweb.dl.sourceforge.net/project/bochs/bochs/2.7/bochs-2.7.tar.gz
tar xf bochs-2.7.tar.gz
cd bochs-2.7/
# First part: from .conf.linux
# Second part: from osdev
./configure --enable-sb16 \
            --enable-ne2000 \
            --enable-all-optimizations \
            --enable-cpu-level=6 \
            --enable-x86-64 \
            --enable-vmx=2 \
            --enable-pci \
            --enable-clgd54xx \
            --enable-voodoo \
            --enable-usb \
            --enable-usb-ohci \
            --enable-usb-ehci \
            --enable-usb-xhci \
            --enable-busmouse \
            --enable-es1370 \
            --enable-e1000 \
            --enable-show-ips \
            \
            --enable-smp \
            --enable-debugger \
            --enable-debugger-gui \
            --enable-logging \
            --enable-fpu \
            --enable-cdrom \
            --enable-x86-debugger \
            --enable-iodebug \
            --disable-plugins \
            --disable-docbook \
            --with-x --with-x11 --with-term --with-sdl2 \
            \
            --prefix=USR_LOCAL
make -j `nproc`
make install
```

Configurations in bochsrc:
```
display_library: sdl2
#sound: ...
```

Boot CDROM: in bochsrc:
```
ata0-master: type=cdrom, path="debian-11.1.0-i386-netinst.iso", status=inserted
boot: cdrom, disk
```

Set up disk: use `bximage` (interactive), then in bochsrc:
```
ata1-master: type=disk, mode=flat, path="c.img", cylinders=0
```

Looks like dnf installed version is faster than self-compiled version.

## Resources
* <https://wiki.osdev.org/Bochs>
* <https://bochs.sourceforge.io/doc/docbook/user/index.html>
	* Boot CDROM error codes
	  <https://bochs.sourceforge.io/doc/docbook/user/bios-tips.html>
	* Pre-defined CPUs
	  <https://bochs.sourceforge.io/doc/docbook/user/cpu-models.html>
* <https://www.cnblogs.com/oasisyang/p/15358137.html>
* Slow
	* <https://sourceforge.net/p/bochs/discussion/39592/thread/45d675556a/>
	* <https://forum.osdev.org/viewtopic.php?f=1&t=31702>
	* <https://forum.osdev.org/viewtopic.php?f=1&t=31358>

