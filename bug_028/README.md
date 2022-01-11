# Should support booting Linux with PAE

## Scope
* All x86 Linux OS
* x86 and x64 XMHF

## Behavior
XMHF does not support running guest OS with PAE (I don't remember how it
crashes). But looks like in `bug_027` Windows XP can boot with PAE, so we
should at least make it Linux with x86 PAE boot

## Debugging
