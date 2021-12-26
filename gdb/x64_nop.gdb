# Jump out of NOP loop
# C: asm volatile("1: nop; jmp 1b; nop; nop; nop; nop; nop; nop; nop; nop");
# Need to manually remove break point

b *($rip + 5)
p $rip += 3
c

