import time

def int32_to_bytes(i):
	a = (i    ) & 0xFF
	b = (i>> 8) & 0xFF
	c = (i>>16) & 0xFF
	d = (i>>24) & 0xFF

	return bytes([a,b,c,d])

# builds the two date messages which are sent to the drone by the app.
def make_date_messages( now = None ):
	if now is None:
		now = time.gmtime()
	
	# first message
	msg1 = bytes([0x26]) + b''.join( [int32_to_bytes(x) for x in [now.tm_year, now.tm_mon, now.tm_mday, now.tm_wday, now.tm_hour, now.tm_min, now.tm_sec]] )

	msg2 = bytes('date -s "%4d-%02d-%02d %02d:%02d:%02d"' % (now.tm_year, now.tm_mon, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec), 'ascii')

	return msg1, msg2

# creates a command packet and a valid checksum using the raw arguments. height needs to be within 00..FF, yaw, pitch, roll must be within 00..7F and the *_trims within 00..0x20
def make_raw_command(height, yaw, pitch, roll, *, launch=False, panic=False, land=False, recalibrate=False, auto_altitude=True, yaw_trim=0x10, pitch_trim=0x10, roll_trim=0x10, compass=False, percent_raw=0):
	command = [0x08, 0,0,0,0, 0,0,0,0] 
	command[1] = height & 0xFF
	command[2] = yaw & 0x7F
	command[3] = pitch & 0x7F
	command[4] = roll & 0x7F
	
	if auto_altitude: command[5] |= 0x80
	if recalibrate: command[5] |= 0x40

	command[5] |= yaw_trim & 0x3F
	command[6] = pitch_trim & 0x3F
	command[7] = roll_trim & 0x3F

	if launch: command[8] |= 0x40
	if panic:  command[8] |= 0x20
	if land:   command[8] |= 0x80
	if compass:command[8] |= 0x10

	command[8] |= percent_raw & 0x03

	command += [ 0xFF - (sum(command) & 0xFF) ]

	command = [0xFF] + command

	return bytes(command)

# creates a command message. Accepts height, yaw, pitch, roll as normalized floating point parameters within -1 .. +1
def make_command(height, yaw, pitch, roll, *, launch=False, panic=False, land=False, recalibrate=False):
	height = max(min(height, 1), -1)
	yaw =    max(min(yaw,    1), -1)
	pitch =  max(min(pitch,  1), -1)
	roll =   max(min(roll,   1), -1)

	return make_raw_command(
		int(height*0x7E + 0x7E),
		int(yaw   *0x3F + 0x3F),
		int(pitch *0x3F + 0x40),
		int(roll  *0x3F + 0x3F),
		launch=launch,
		panic=panic,
		land=land,
		recalibrate=recalibrate
	)



def expect(cmd, i, mask, val):
	if cmd[i] & mask != val:
		return 'cmd[%d]&%02X==%02X!=%02X. ' % (i,mask,cmd[i],val)
	else:
		return ''

def flagstr(flag, flagname):
	return flagname.upper() if flag else flagname.lower()

# decodes a command and returns a human-readable representation.
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
