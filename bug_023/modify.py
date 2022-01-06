import sys, re

def update(content, filename):
	lines = content.split('\n')
	skip = None
	output = []
	for i in lines:
		if skip:
			assert i == skip
			skip = None
			continue
		if '_full' in i:
			skip = 1
			name = re.fullmatch('  unsigned int  (\w+)_full;', i).groups()[0]
			skip = '  unsigned int  %s_high;' % name
			if 'x86_64' in filename:
				output.append('  unsigned long long  %s;' % name)
			else:	# x86
				output.append('  union {')
				output.append('    unsigned long long  %s;' % name)
				output.append('    struct {')
				output.append('      unsigned int  %s_full;' % name)
				output.append('      unsigned int  %s_high;' % name)
				output.append('    };')
				output.append('  };')
		else:
			output.append(i)
	return '\n'.join(output)

for i in sys.argv[1:]:
	content = open(i).read()
	updated = update(content, i)
	open(i, 'w').write(updated)

