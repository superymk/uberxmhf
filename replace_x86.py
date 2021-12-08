# Auto-replace names in XMHF source code
# e.g. In `part-x86_64svm.c`
# From xmhf_partition_arch_x86svm_initializemonitor
# To   xmhf_partition_arch_x86_64svm_initializemonitor

import re, sys
from subprocess import Popen, check_output

def replace_word(old, new, dry_run=False):
	grep_output = check_output(['git', 'grep', '-Flw', old]).decode()
	names = list(filter(bool, grep_output.split('\n')))
	# Prepare
	change_list = []
	for name in names:
		assert name.endswith('.h') or name.endswith('.c')
		old_content = open(name).read()
		new_content, ns = re.subn(r'\b%s\b' % re.escape(old), new, old_content)
		assert old_content != new_content
		# Test whether we should make the change
		if 'x86_64' in name:
			# Yes, we should (e.g. part-x86.c)
			change_list.append((name, old_content, new_content, ns))
			print('%d\t%s' % (ns, name))
		elif 'x86' in name:
			# No, we should not (e.g. part-x86_64.c)
			print('%d\t\033[9m%s\033[0m' % (ns, name))
		else:
			# Need manual investigation (e.g. include/xmhf-partition.h)
			print('%d\t\033[41m%s\033[0m' % (ns, name))
	# Commit
	if dry_run:
		return
	for name, old_content, new_content, ns in change_list:
		with open(name, 'r') as f:
			assert f.read() == old_content
		with open(name, 'w') as f:
			f.write(new_content)

def main():
	old = sys.argv[1]
	if len(sys.argv) > 2:
		new = sys.argv[2]
	else:
		assert 'x86' in old
		new = old.replace('x86', 'x86_64')
	replace_word(old, new)

if __name__ == '__main__':
	main()

