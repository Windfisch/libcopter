import parsestream
from sys import argv
from sys import stderr

import cv2

# usage:
# mkdir build
# cd build
# cmake ..
# make
# cd ..
# cp build/parsestream.so ./
# python python_test.py TCPSTREAM.bin


p = parsestream.PyVideoTelemetryParser()

stream = open(argv[1], 'rb')

lastval = -1
while True:
	bs = stream.read(1024)

	result = p.consume_data(bs)

	for frame in result.video_frames:
		cv2.imshow("frame",frame)
		cv2.waitKey(10)

	if len(result.telemetry_alti):
		lastval = result.telemetry_alti[-1].value

	print("%d, %d" % (len(result.video_frames), lastval), file=stderr)
