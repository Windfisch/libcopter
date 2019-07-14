import socket
import command
import time

import cv2
import libcopter
from select import select

#IP = "131.188.24.11"
IP = "172.16.10.1"
PORT = 8080

# control socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# initialization sequence
print("sending initialization sequence to the drone")
sock.sendto(bytes([0x0f]), (IP, PORT))
sock.sendto(bytes([0x28]), (IP, PORT)) # request UDP720P message
print(sock.recvfrom(100))
sock.sendto(bytes([0x28]), (IP, PORT)) # request version message
print(sock.recvfrom(100))
sock.sendto(bytes([0x42]), (IP, PORT))

for i in range(100):
	msg1, msg2 = command.make_date_messages() # todo: check for 'timeok'
	sock.sendto(msg1, (IP, PORT))
	sock.sendto(msg2, (IP, PORT))
	time.sleep(0.01)

sock.sendto(bytes([0x2c]), (IP, PORT)) # causes "ok\x02"
#sock.sendto(bytes([0x27]), (IP, PORT)) # the app does this, but it seems unneeded
#sock.sendto(bytes([0x27]), (IP, PORT))
print(sock.recvfrom(100))

print("initialization complete, now opening the video+telemetry TCP socket")

# video and telemetry socket
tcpsock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
tcpsock.connect((IP, 8888))

tcpsock.send(bytes([0,1,2,3,4,5,6,7,8,9, 0x28, 0x28])) # this is required to start the video stream
#for i in range(100): # receive some video
#	print(len(tcpsock.recv(1024)))
print()

print("calibrating...")
for i in range(10):
	tcpsock.send(bytes([0,1,2,3,4,5,6,7,8,9, 0x28, 0x28]))
	sock.sendto( command.make_command(0,0,0,0, recalibrate=True), (IP, PORT) )
	time.sleep(0.1)

p = libcopter.PyVideoTelemetryParser()
height,yaw,pitch,roll = 0,0,0,0
land,launch,panic = False,False,False
i=0

streamdump = open('steamdump.bin', 'wb')

fourcc = cv2.VideoWriter_fourcc(*'X264')
videodump = cv2.VideoWriter('output.avi',fourcc, 25.0, (1280,720))

telemetry_alti = None
telemetry_batt = None

while True:
	i+=1
	while True:
		readable, _, _ = select([tcpsock], [], [], 0)
		if len(readable) == 0:
			break
		tcpdata = tcpsock.recv(1024)
		streamdump.write(tcpdata)
		result = p.consume_data(tcpdata)

		if len(result.video_frames) > 0:
			cv2.imshow("frame", result.video_frames[-1])

			for vf in result.video_frames:
				videodump.write(vf)

		if len(result.telemetry_batt):
			telemetry_batt = result.telemetry_batt[-1]
		if len(result.telemetry_alti):
			telemetry_alti = result.telemetry_alti[-1]
		if len(result.telemetry_other):
			print("\ngot unknown telemetry?!\n")

	if telemetry_alti is not None and telemetry_batt is not None:
		print("\r#%010d: batt = %10d @ %08x %08X, altitude = %10d @ %08x %08X" % (i, telemetry_batt.value, telemetry_batt.timestamp, telemetry_batt.maybe_timestamp_high, telemetry_alti.value, telemetry_alti.timestamp, telemetry_alti.maybe_timestamp_high), end='')

	tcpsock.send(bytes([0,1,2,3,4,5,6,7,8,9, 0x28, 0x28])) # this is required to start the video stream
	
	k = cv2.waitKey(30)
	if k == ord("s"):
		pitch = min(pitch+0.25, 1)
	elif k == ord("w"):
		pitch = max(pitch-0.25, -1)
	elif k == ord("d"):
		roll = min(roll+0.25, 1)
	elif k == ord("a"):
		roll = max(roll-0.25, -1)
	elif k == ord("e"):
		yaw = min(yaw+0.25, 1)
	elif k == ord("q"):
		yaw = max(yaw-0.25, -1)
	elif k == ord("r"):
		height = min(height+0.25, 1)
	elif k == ord("f"):
		height = max(height-0.25, -1)
	elif k == ord("x"):
		launch = True
	elif k == ord("z"):
		land = True
	elif k == ord("c"):
		panic = True
	elif k == ord("1") or k == ord("2") or k == ord("3"):
		height,yaw,pitch,roll = 0,0,0,0
	
	
	sock.sendto( command.make_command(height, yaw, pitch, roll, launch=launch, land=land, panic=panic), (IP, PORT) )

	land,launch,panic = False,False,False
		
exit(1)
print ("launching")
print ("sending, in that order: \n%s\n%s\n%s\n%s" % (command.make_command(0,0,0,0, recalibrate=True).hex(), command.make_command(0,0,0,0, launch=True).hex(), command.make_command(0,0,0,0, land=True).hex(), command.make_command(0,0,0,0, panic=True).hex()))

for i in range(10):
	tcpsock.send(bytes([0,1,2,3,4,5,6,7,8,9, 0x28, 0x28]))
	sock.sendto( command.make_command(0,0,0,0, recalibrate=True), (IP, PORT) )
	time.sleep(0.1)
	print("C ", end='')
print("launch in 5")
time.sleep(5)
for i in range(10):
	tcpsock.send(bytes([0,1,2,3,4,5,6,7,8,9, 0x25, 0x25]))
	sock.sendto( command.make_command(0,0,0,0, launch=True), (IP, PORT) )
	time.sleep(0.1)
	print("L ", end='')
print()
time.sleep(1)
print("land")
for i in range(50):
	sock.sendto( command.make_command(0,0,0,0, land=True), (IP, PORT) )
print("panic in 4")
time.sleep(4)
print("panic")
for i in range(50):
	sock.sendto( command.make_command(0,0,0,0, panic=True), (IP, PORT) )

print(sock.recvfrom(100))
while True:
	print(sock.recvfrom(100))
