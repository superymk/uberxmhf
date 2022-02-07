import os, sys, operator

def read_file_5(pathname):
	ans = []
	# print(pathname, file=sys.stderr)
	with open(pathname, 'rb') as f:
		ans.append(f.read(0x1000))
		ans.append(f.read(0x1000))
		ans.append(f.read(0x1000))
		ans.append(f.read(0x1000))
		ans.append(f.read(0x1000))
	return ans

def compare_file(a, b):
	ans = []
	for i, j in zip(a, b):
		if len(i) != 0x1000 or len(j) != 0x1000:
			return [0x1000] * 5
		ans.append(sum(map(operator.ne, i, j)))
	return ans

def walk_path(path):
	for p, d, f in os.walk(path):
		for i in f:
			pn = os.path.join(p, i)
			if os.path.isfile(pn):
				yield pn

if __name__ == '__main__':
	base = read_file_5(sys.argv[1])
	for i in walk_path(sys.argv[2]):
		print(*compare_file(base, read_file_5(i)), i, sep='\t')

