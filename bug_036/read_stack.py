import re, sys, bisect
from subprocess import check_output

def read_symbols(exe):
	ans = []
	for line in check_output(['nm', exe]).decode().split('\n'):
		if not line:
			continue
		addr, name = re.fullmatch('([0-9a-f]{16}) \w ([\w\.]+)', line).groups()
		addr = int(addr, 16)
		ans.append((addr, 0, name))
	ans.sort()
	return ans

def lookup_symbol(symbols, addr):
	'''
		Return print_str, func_name, func_offset.
		e.g. ('<main+10>', 'main', 10)
	'''
	index = bisect.bisect(symbols, (addr, 1))
	if index > 0:
		offset = addr - symbols[index - 1][0]
		if offset < 0x10000000:
			name = symbols[index - 1][2]
			return '<%s+%d>' % (name, offset), name, offset
	return '???', None, None

def process_cpu(symbols):
	ans = None
	valid_state = False
	while True:
		line = yield ans
		ans = line
		if line.startswith(': unhandled exception'):
			# Reset state
			valid_state = True
			has_error_code = False
			rsp = None
			rbp = None
			frame_no = 0
			intercept_rsp = None
			continue
		if not valid_state:
			continue
		if line.startswith(': error code:'):
			has_error_code = True
			continue
		if 'RBP=' in line:
			rbp, = re.search('RBP=(0x[0-9a-f]+)', line).groups()
			rbp = int(rbp, 16)
			old_rbp = rbp
		if line.startswith('  Stack('):
			matched = re.fullmatch('Stack\((0x[0-9a-f]+)\) \-\> (0x[0-9a-f]+)',
									line.strip())
			addr, value = matched.groups()
			addr = int(addr, 16)
			value = int(value, 16)
			if rsp is None:
				rsp = addr - has_error_code * 8
			offset = addr - rsp
			exception_dict = {
				-8: 'Error code',
				0x08: 'CS', 0x10: 'RFLAGS', 0x18: 'RSP', 0x20: 'SS',
			}
			if offset == 0:
				ans += ' RIP #0\t'
			elif offset in exception_dict:
				ans += ' %s\t' % exception_dict[offset]
			elif addr == rbp:
				ans += ' RBP #%d\t' % frame_no
				old_rbp = rbp
				rbp = value
				frame_no += 1
			elif addr == old_rbp + 8:
				func_ptn, func_name, func_offset = lookup_symbol(symbols, value)
				ans += ' RIP #%d\t' % frame_no
				if func_name == 'xmhf_parteventhub_arch_x86_64vmx_entry':
					intercept_rsp = addr + 0x8
			elif intercept_rsp is not None:
				regs = ['rdi','rsi','rbp','rsp','rbx','rdx','rcx','rax','r8',
						'r9','r10','r11','r12','r13','r14','r15']
				ans += ' guest %s\t' % (regs[(addr - intercept_rsp) // 0x8])
			else:
				ans += '\t\t'
			func_ptn, _, _ = lookup_symbol(symbols, value)
			ans += '%s' % func_ptn

def main():
	cpus = []
	symbols = read_symbols(sys.argv[2])
	# import pdb; pdb.set_trace()
	for line in open(sys.argv[1]).read().split('\n'):
		matched = re.match('\[(\d\d)\](.+)', line)
		if matched:
			cpu, content = matched.groups()
			cpu = int(cpu)
			while len(cpus) <= cpu:
				cpus.append(process_cpu(symbols))
				cpus[-1].send(None)
			new_content = cpus[cpu].send(content)
			print('[%02d]%s' % (cpu, new_content))
		else:
			print(line)

if __name__ == '__main__':
	main()

