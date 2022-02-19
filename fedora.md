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

## Booting XMHF on Fedora installed in GPT partition

Looks like Fedora installed in GPT partition uses EFI GRUB. Need to do
something different

Refs:
* <https://www.dedoimedo.com/computers/grub2-fedora-command-not-found.html>
* <https://bugzilla.redhat.com/show_bug.cgi?id=1896848>

```sh
sudo dnf install grub2-efi-x64-modules
sudo cp /usr/lib/grub/x86_64-efi/* /boot/efi/EFI/fedora/x86_64-efi/
sudo grub2-mkconfig -o /boot/grub2/grub.cfg
sudo grub2-mkconfig -o /boot/efi/EFI/fedora/grub.cfg
```

However, unable to boot XMHF. Will need to look into UEFI support in the future.

### Install GRUB on USB

We try to use Debian to install GRUB on another disk formatted with MBR

```sh
sudo fdisk /dev/sdx
	# add a partition
sudo mke2fs /dev/sdxy
sudo grub-install /dev/sdx
sudo tune2fs /dev/sdxy -U <UUID-of-disk-containing-boot-directory>
sudo mount /dev/sdxy /mnt
sudo mkdir /mnt/boot
sudo cp -r /boot/grub /mnt/boot/
sudo umount /mnt
sync
```

