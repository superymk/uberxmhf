from subprocess import Popen, check_call
import argparse, subprocess, time, os, random, socket, threading

def parse_args():
	parser = argparse.ArgumentParser()
	parser.add_argument('--qemu-image', required=True)
	parser.add_argument('--nbd', type=int, required=True)
	parser.add_argument('--work-dir', required=True)
	parser.add_argument('--test-num', type=int, default=100)
	parser.add_argument('--no-display', action='store_true')
	args = parser.parse_args()
	return args

def update_grub(args):
	check_call('sudo modprobe nbd max_part=8'.split())
	dev_nbd = '/dev/nbd%d' % args.nbd
	check_call(['sudo', 'qemu-nbd', '--connect=' + dev_nbd, args.qemu_image])
	dir_mnt = os.path.join(args.work_dir, 'mnt')
	check_call(['mkdir', '-p', dir_mnt])
	check_call(['sudo', 'fdisk', dev_nbd, '-l'])
	check_call(['sudo', 'mount', dev_nbd + 'p1', dir_mnt])
	check_call(['sudo', 'chroot', dir_mnt] + 
				'grub-editenv /boot/grub/grubenv set next_entry=3'.split())
	check_call(['sudo', 'sync'])
	for i in range(5):
		try:
			check_call(['sudo', 'umount', dir_mnt])
		except subprocess.CalledProcessError:
			time.sleep(1)
			continue
		break
	else:
		raise Exception('Cannot umounts')
	check_call(['sudo', 'qemu-nbd', '--disconnect', dev_nbd])

def get_port():
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
	for i in range(10000):
		num = random.randrange(1024, 65536)
		try:
			s.bind(('127.0.0.1', num))
		except OSError:
			continue
		s.close()
		return num
	else:
		raise RuntimeError('Cannot get port')

def spawn_qemu(args):
	qemu_args = [
		'qemu-system-x86_64', '-m', '512M',
		'--drive', 'media=disk,file=%s,index=2' % args.qemu_image,
		'-device', 'e1000,netdev=net0',
		'-netdev', 'user,id=net0,hostfwd=tcp::%d-:22' % ssh_port,
		'-smp', '4', '-cpu', 'Haswell,vmx=yes', '--enable-kvm',
		'-serial', 'stdio'
	]
	if args.no_display:
		qemu_args.append('-display')
		qemu_args.append('none')
	p = Popen(qemu_args, stdin=-1, stdout=-1)
	return p

def send_ssh(ssh_port, test_num, status):
	'''
	status[0] = lock
	status[1] = 0: test not started yet
	status[1] = 1: test started
	status[1] = 2: test finished successfully
	status[2] = time of last line
	status[3] = number of successful tests
	'''
	# First echo may not be captured by p.stdout for unknown reason
	bash_script = ('echo TEST_START; '
					'echo TEST_START; '
					'for i in {1..%d}; do ./main $i $i; done; '
					'echo TEST_END; '
					'sudo init 0; '
					'true') % test_num
	while True:
		p = Popen(['ssh', '-p', str(ssh_port), '-o', 'ConnectTimeout=5', '-o',
					'StrictHostKeyChecking=no', '127.0.0.1',
					'bash', '-c', bash_script], stdin=-1, stdout=-1)
		read_test_start = False
		while True:
			line = p.stdout.readline().decode()
			if not line:
				p.wait()
				assert p.returncode is not None
				if p.returncode == 0:
					# Test pass, but should not use this branch
					with status[0]:
						status[1] = 2
						return
				break
			print('ssh:', line.rstrip())
			ls = line.strip()
			if ls == 'TEST_START':
				with status[0]:
					status[1] = 1
			elif ls == 'TEST_END':
				with status[0]:
					# Test pass, expected way
					status[1] = 2
					return
			else:
				if_pass_1 = line.strip() == 'With PAL:'
				with status[0]:
					status[1] = 1
					status[2] = time.time()
					status[3] += if_pass_1
		time.sleep(1)
		print('Retry SSH')

def watch_serial(p, serial_buffer):
	while True:
		# Will lose the last line without '\n'
		c = p.stdout.readline()
		if not c:
			break
		# print('serial:', c.decode().rstrip())
		serial_buffer.append(c.decode().rstrip())

if __name__ == '__main__':
	args = parse_args()
	update_grub(args)
	ssh_port = get_port()
	print('Use ssh port', ssh_port)
	p = spawn_qemu(args)
	serial_buffer = []
	ss = [threading.Lock(), 0, 0, 0]
	ts = threading.Thread(target=send_ssh, args=(ssh_port, args.test_num, ss),
							daemon=True)
	tw = threading.Thread(target=watch_serial, args=(p, serial_buffer))
	ts.start()
	tw.start()

	start_time = time.time()
	test_result = 'unknown'
	test_pass_count = -1
	while True:
		state = [None]
		with ss[0]:
			state[1:] = ss[1:]
		print('main: MET = %d;' % int(time.time() - start_time),
				'state =', state)
		if state[1] == 2:
			# test successful, give the OS 10 seconds to shutdown
			time.sleep(10)
			test_result = 'pass'
			test_pass_count = state[3]
			break
		elif state[1] == 1 and time.time() - state[2] > 10:
			# 10 seconds without new message, likely failed
			test_result = 'fail'
			test_pass_count = state[3]
			break
		time.sleep(1)

	p.kill()
	p.wait()
	tw.join()

	print(test_result, test_pass_count, len(serial_buffer))
	with open(os.path.join(args.work_dir, 'result'), 'w') as f:
		print(test_result, test_pass_count, *serial_buffer, sep='\n', file=f)

