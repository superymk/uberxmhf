import sys

table = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'

def char2int64(c):
	i = ord(c)
	if i > 0x60:	# a-z
		return i - 71	# i - 0x61 + 26
	elif i > 0x40:	# A-Z
		return i - 0x41
	elif i >= 0x30:	# 0-9
		return i + 4	# i - 0x30 + 26 * 2
	elif i == 0x2f:	# /
		return 63
	elif i == 0x2b:	# +
		return 62
	else:
		raise ValueError

if 'test':
	for index, i in enumerate(table):
		assert char2int64(i) == index

def process_serial_2(c, cpu_buf):
	i = char2int64(c)
	cpu = i // 16
	val = i % 16
	if cpu_buf[cpu] is None:
		cpu_buf[cpu] = val
		return None
	else:
		ans = (cpu_buf[cpu] << 4) | val
		cpu_buf[cpu] = None
		return cpu, ans

def process_serial_3(c, cpu_buf):
	i = char2int64(c)
	cpu = i >> 4
	rst = i & 0x8
	val = i & 0x7
	ans = None
	if rst:
		cpu_buf[cpu] = []
	if cpu_buf[cpu] is not None:
		cpu_buf[cpu].append(val)
		if len(cpu_buf[cpu]) == 3:
			_2, _1, _0 = cpu_buf[cpu]
			ans = cpu, (_2 << 6) | (_1 << 3) | _0
			cpu_buf[cpu] = []
	return ans

cpu_buf = [None, None, None, None]

while True:
	i = sys.stdin.read(1)
	if not i:
		break
	ans = process_serial_3(i, cpu_buf)
	# print(oct(char2int64(i)), ans, sep='\t')
	if ans is not None:
		cpu, c = ans
		if c == 0:
			print(end='\033[4%dm \033[0m' % (cpu + 1), flush=True)
		else:
			print(end='\033[3%dm%c\033[0m' % (cpu + 1, c), flush=True)

