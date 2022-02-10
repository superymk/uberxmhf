# Debugging with Fedora

## Grub

By default Fedora's GRUB will be hidden. Use the following to show:
```sh
sudo grub2-editenv - unset menu_auto_hide
```

Ref: <https://fedoraproject.org/wiki/Changes/HiddenGrubMenu>

Editing next GRUB entry becomes:
```sh
sudo grub2-editenv /boot/grubenv set next_entry=XMHF
```

Update grub is
```sh
grub2-mkconfig -o /boot/grub2/grub.cfg
```

## Running x86 PAL on x64 Fedora

Need to have `/lib/ld-linux.so.2`

```sh
sudo dnf install glibc.i686
```

