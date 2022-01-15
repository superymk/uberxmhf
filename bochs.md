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

## Resources
* <https://wiki.osdev.org/Bochs>
* <https://bochs.sourceforge.io/doc/docbook/user/index.html>
* <https://www.cnblogs.com/oasisyang/p/15358137.html>

