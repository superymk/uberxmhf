#!/usr/bin/python3
# Inject NMI to QEMU automatically

import sys, time, socket, json

CMD_INIT = b'{ "execute": "qmp_capabilities" }\r\n'
CMD_NMI = b'{ "execute": "inject-nmi" }\r\n'
RETURN_VAL = b'{"return": {}}\r\n'

def recvline(s):
	# This function assumes that s will print one line at a time
	line = b''
	while b'\r\n' not in line:
		buf = s.recv(1000)
		assert buf
		line += buf
	return line

def initialize(port):
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
	s.connect(('127.0.0.1', port))
	assert recvline(s)
	s.send(CMD_INIT)
	assert recvline(s) == RETURN_VAL
	return s

def send_nmi(s):
	s.send(CMD_NMI)
	while True:
		resp = recvline(s)
		if resp == RETURN_VAL:
			break
		if 'event' in json.loads(resp):
			# Async message
			continue
		else:
			print('Received unexpected message:', repr(resp))
			import pdb; pdb.set_trace()

def main():
	port = int(sys.argv[1])
	interval = float(sys.argv[2])
	s = initialize(port)
	while True:
		send_nmi(s)
		time.sleep(interval)

if __name__ == '__main__':
	main()

