import libcopter
from sys import argv
from sys import stderr

# usage:
# mkdir build
# cd build
# cmake ..
# make
# cd ..
# cp build/libcopter.so ./
# python python_test.py TCPSTREAM.bin


p = libcopter.PyVideoTelemetryParser()

stream = open(argv[1], 'rb')

lastval = -1
while True:
	bs = stream.read(1024)

	result = p.consume_data(bs)

	if len(result.telemetry_alti):
		lastval = result.telemetry_alti[-1].value

	print("%d, %d" % (len(result.video_frames), lastval), file=stderr)
