# Windows XP performs VMCALL

## Scope
* All x86 Linux OS
* x86 and x64 XMHF

## Behavior
See also: `bug_027`

XMHF does not support running guest OS with PAE (I don't remember how it
crashes). But looks like in `bug_027` Windows XP can boot with PAE, so we
should at least make it Linux with x86 PAE boot

## Debugging

Likely related to something called vAPIC / APICv, from Hyper-V? Resources:
* <https://listman.redhat.com/archives/vfio-users/2016-June/msg00055.html>
* <https://github.com/torvalds/linux/commit/b93463aa59d67b21b4921e30bd5c5dcc7c35ffbd>
* <https://lists.gnu.org/archive/html/qemu-devel/2012-02/msg00516.html>
* <https://lore.kernel.org/linux-hyperv/DM5PR21MB0137FCE28A16166207E08C7CD79E0@DM5PR21MB0137.namprd21.prod.outlook.com/>
