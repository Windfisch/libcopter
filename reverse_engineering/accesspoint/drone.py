import socket

def expect(cmd, i, mask, val):
	if cmd[i] & mask != val:
		return 'cmd[%d]&%02X==%02X!=%02X. ' % (i,mask,cmd[i],val)
	else:
		return ''

def flagstr(flag, flagname):
	return flagname.upper() if flag else flagname.lower()

def decode_cmd(cmd):
	strange = ''
	strange += expect(cmd, 0, 0xFF, 0x08)
	strange += expect(cmd, 2, 0x80, 0x00)
	strange += expect(cmd, 3, 0x80, 0x00)
	strange += expect(cmd, 4, 0x80, 0x00)
	strange += expect(cmd, 6, 0xC0, 0x00)
	strange += expect(cmd, 7, 0xC0, 0x00)
	strange += expect(cmd, 8, 0x0C, 0x00)

	if strange != '': strange = "strange: "+strange

	height = cmd[1] & 0xFF
	yaw = cmd[2] & 0x7F
	pitch = cmd[3] & 0x7F
	roll = cmd[4] & 0x7F

	auto_altitude = bool(cmd[5] & 0x80)
	recalibrate =   bool(cmd[5] & 0x40)

	yaw_trim =   cmd[5] & 0x3F
	pitch_trim = cmd[6] & 0x3F
	roll_trim  = cmd[7] & 0x3F

	launch = cmd[8] & 0x40
	panic  = cmd[8] & 0x20
	land   = cmd[8] & 0x80
	compass= cmd[8] & 0x10 # dunno what this button does
	
	percent_raw = cmd[8] & 0x03
	if percent_raw == 0:
		percent = 30
	elif percent_raw == 1:
		percent = 60
	elif percent_raw == 2:
		percent = 100
	else:
		percent = -1 # invalid
		strange += 'illegal percent value. '

	pulses = []
	if launch: pulses += ['LAUNCH']
	if panic: pulses += ['PANIC']
	if land: pulses += ['LAND']
	if recalibrate: pulses += ['RECALIBRATE']

	return 'height %02X, yaw %02X%+2d, pitch %02X%+2d, roll %02X%+2d, %2d%%, %8s, %7s, %s %s' % (height, yaw, yaw_trim-16, pitch, pitch_trim-16, roll, roll_trim-16, percent, flagstr(auto_altitude, 'auto_alt'), flagstr(compass, 'compass'), ','.join(pulses), strange)


def prettify(s):
	if type(s) is bytes:
		try:
			string = s.decode('ascii')
		except UnicodeDecodeError:
			string = None
		s = s.hex()
	
	rem = s[2:]
	result = s[0:2]
	for i in range(0, len(rem), 8):
		if i % 16 == 0:
			result += " "
		result += " " + rem[i : i+8]
	
	if string is not None and string.isprintable:
		result = result + "[ " + string + "]"
	
	return result

IP = "172.16.10.1"
PORT = 8080

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((IP,PORT))


req1_served = False
req2_served = False

while True:
	data, addr = sock.recvfrom(1024)
	print(prettify(data)+"   ", end='')

	if data == bytes([0x0f]):
		print("(reset)")
		req1_served = False
		req2_served = False

	elif data == bytes([0x28]):
		print("(request1)")
		if not req1_served:
			sock.sendto(bytes("UDP720P", 'ascii'), addr)
			req1_served = True
	elif data == bytes([0x42]):
		print("(request2)")
		
		if not req2_served:
			#sock.sendto(bytes("V2.your string here.even more fun with strings", 'ascii'), addr)
			sock.sendto(bytes("V2.4.8", 'ascii'), addr)
			req2_served=True
	
	elif data[0] == 0x26:
		print("(date1)")
		#sock.sendto(bytes("timeok", 'ascii'), addr) # if we don't send this, we're getting that date command forever
	elif data[0:4] == bytes('date', 'ascii'):
		print("(date2)")

		sock.sendto(bytes("timeok", 'ascii'), addr) # if we don't send this, we're getting that date command forever
	elif data[0] == 0xff:
		print("(command)", end='')

		cmd = data[1:-1]
		checksum = data[-1]

		expected_checksum = 0xff - (sum(cmd) & 0xff)

		if checksum != expected_checksum:
			print("[CHECKSUM ERROR]", end='')
		else:
			print("[ok]", end='')

		print('   '+decode_cmd(cmd))

	else:
		print("(???)")


sock.sendto(bytes([0x0f]), (IP, PORT))
sock.sendto(bytes([0x28]), (IP, PORT)) # request UDP720P message
print(sock.recvfrom(100))
sock.sendto(bytes([0x28]), (IP, PORT)) # request version message
print(sock.recvfrom(100))
sock.sendto(bytes([0x42]), (IP, PORT))
sock.sendto(bytes([0x26, 0xe3, 0x07, 0,0, 6,0,0,0, 0x14,0,0,0, 0x04,0,0,0, 0x12,0,0,0, 0x38,0,0,0, 0x1d,0,0,0]), (IP, PORT))
sock.sendto(bytes('date -s "2019-06-20 18:56:29"', 'ascii'), (IP, PORT))
print(sock.recvfrom(100))
print(sock.recvfrom(100))
print(sock.recvfrom(100))
