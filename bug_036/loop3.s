.code16

start:
	xorl	%eax, %eax
	xorl	%ebx, %ebx
	xorl	%ecx, %ecx
	xorl	%edx, %edx
back:
	incw	%ax
	jnz		loop
	incw	%bx
	jnz		loop
	incw	%cx
	jnz		loop
	incw	%dx
	jnz		loop
loop:
.rept 10
	nop
.endr
	jmp		back


.section ".trampoline"
	push   $0x7c0
	push   $0x0
	lret   

