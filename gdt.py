'Convert binary to human-readable GDT'

def int2bits(i, n):
	bits = []
	for j in range(n):
		bits.append((i >> j) & 1)
	return bits

def bits2int(b, n):
	assert n == len(b)
	ans = 0
	for i in reversed(b):
		ans <<= 1
		ans |= i
	return ans

def print_gdt(gdt):
	'gdt is a 64-bit integer'
	bits = int2bits(gdt, 64)
	if 0:
		for i in reversed(range(64)):
			print(i // 10 if i % 10 == 0 else ' ', end='')
		print()
		for i in reversed(range(64)):
			print(i % 10, end='')
		print()
		print(''.join(map(str, reversed(bits))))
		print()
	sl = bits2int(bits[0:16] + bits[48:52], 20)
	print('Segment Limit', hex(sl), end='\t')
	ba = bits2int(bits[16:40] + bits[56:64], 32)
	print('Base Address', hex(ba), end='\t')
	t = bits2int(bits[40:44], 4)
	print('Type', hex(t), sep='=', end=' ')
	s = bits2int(bits[44:45], 1)
	print('S', (s), sep='=', end=' ')
	dpl = bits2int(bits[45:47], 2)
	print('DPL', (dpl), sep='=', end=' ')
	p = bits2int(bits[47:48], 1)
	print('P', (p), sep='=', end=' ')
	avl = bits2int(bits[52:53], 1)
	print('AVL', (avl), sep='=', end=' ')
	l = bits2int(bits[53:54], 1)
	print('L', (l), sep='=', end=' ')
	db = bits2int(bits[54:55], 1)
	print('DB', (db), sep='=', end=' ')
	g = bits2int(bits[55:56], 1)
	print('G', (g), sep='=', end='\n')

# https://github.com/torvalds/linux/blob/v4.16/arch/x86/boot/compressed/head_64.S

print('\nLinux 64')
print_gdt(0x00cf9a000000ffff)
print_gdt(0x00af9a000000ffff)
print_gdt(0x00cf92000000ffff)
print_gdt(0x0080890000000000)

print('\nXMHF 32')
print_gdt(0x00cf9a000000ffff)
print_gdt(0x00cf92000000ffff)

