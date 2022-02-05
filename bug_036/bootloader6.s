.code16

.altmacro
.macro print o a b c d
	// Print message (base is 0xB8000)
	movl	$&o&, %eax
	movb	$&a&, 0(%eax)
	movb	$&b&, 2(%eax)
	movb	$&c&, 4(%eax)
	movb	$&d&, 6(%eax)
.endm

	// Check segment registers
	movw	%cs, %ax
	testw	%ax, %ax
61:	je		6f
	print	0xB8500, 'E', 'R', 'R', '6'
	jmp		61b
6:
	movw	%ds, %ax
	testw	%ax, %ax
71:	je		7f
	print	0xB8500, 'E', 'R', 'R', '7'
	jmp		71b
7:
	movw	%es, %ax
	testw	%ax, %ax
81:	je		8f
	print	0xB8500, 'E', 'R', 'R', '8'
	jmp		81b
8:
	movw	%ss, %ax
	testw	%ax, %ax
91:	je		9f
	print	0xB8500, 'E', 'R', 'R', '9'
	jmp		91b
9:

	// Prepare call
	movl	$0xbb07, %eax
	movl	$0x41504354, %ebx
	movl	$0x200, %ecx
	movl	$0x9, %edx
	movl	$0x0, %esi
	movl	$0x7c00, %edi

	// Call
	int		$0x1a

	// Test CF
	jnb		1f
	print	0xB8500, 'E', 'R', 'R', '1'
2:	jmp		2b

1:
	cmpl	$0x1a, %eax
	je		3f
	print	0xB8500, 'E', 'R', 'R', '3'
4:	jmp		4b

3:
	print	0xB8500, 'G', 'O', 'O', 'D'

	// Loop
5:	jmp 5b

