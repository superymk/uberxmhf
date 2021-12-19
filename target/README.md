# Files used to configure target (machine that runs XMHF)

* `install86.sh`: goes to `~`, install x86 XMHF from `/tmp` to `/boot`
* `install64.sh`: goes to `~`, install x86-64 XMHF from `/tmp` to `/boot`
* `42_xmhf`: goes to `/etc/grub.d/`, configure GRUB for x86 XMHF
* `43_xmhf64`: goes to `/etc/grub.d/`, configure GRUB for x86-64 XMHF
	* Make sure to change serial configuration
* `bash_aliases`: goes to `~/.bash_aliases`
