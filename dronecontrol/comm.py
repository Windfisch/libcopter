import socket
import command
import time

#IP = "131.188.24.11"
IP = "172.16.10.1"
PORT = 8080

# control socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# initialization sequence
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


# video and telemetry socket
tcpsock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
tcpsock.connect((IP, 8888))

tcpsock.send(bytes([0,1,2,3,4,5,6,7,8,9, 0x28, 0x28])) # this is required to start the video stream
for i in range(100): # receive some video
	print(len(tcpsock.recv(1024)), end=' ')
print()



#exit(1)
print ("launching")
print ("sending, in that order: \n%s\n%s\n%s\n%s" % (command.make_command(0,0,0,0, recalibrate=True).hex(), command.make_command(0,0,0,0, launch=True).hex(), command.make_command(0,0,0,0, land=True).hex(), command.make_command(0,0,0,0, panic=True).hex()))

for i in range(10):
	tcpsock.send(bytes([0,1,2,3,4,5,6,7,8,9, 0x25, 0x25]))
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
