#!/usr/bin/env python3

import sys
import struct

NAL_UNIT = 3

from collections import defaultdict

nal_unit_statistics = defaultdict(lambda : 0)
nal_unit_statistics2 = defaultdict(lambda : 0)

nal_unit_len = defaultdict(lambda : 0)

header = bytes([0x00, 0x00, 0x01])
data = open(sys.argv[1], "rb").read()
packets = [header + x for x in data.split(header) if len(x) > 0]

# statistics
for p in packets:
	nal_unit = p[NAL_UNIT]
	nal_unit_statistics[nal_unit] += 1
	nal_unit_len[nal_unit] += len(p)

print("NALU statistics (0x00 0x00 0x01 0x?? sequences): ")
print("\t%13s: %5s,  %14s" % ("0x??", "cnt", "avg pkt len"))
for key,value in nal_unit_statistics.items():
	print("\t%2x (%8s): %5d,  len = %8.1f" % (key, bin(key)[2:], value, nal_unit_len[key]/value))

print("")
print("-"*80)
print("")

nal_units = sorted(nal_unit_statistics.keys())


print("cumulative counts of NALU kinds:")
print(("%3s   " + " ".join(len(nal_unit_statistics)*["%5x"])) % (("id",) + tuple(nal_units)))
cnt = 0
for p in packets:
	nal_unit = p[NAL_UNIT]

	nal_unit_statistics2[nal_unit] += 1
	print(("%3s   " + " ".join(len(nal_unit_statistics)*["%5d"])) % ((cnt,) + tuple([nal_unit_statistics2[nal_unit] for nal_unit in nal_units])))

	cnt += 1



print("")
print("-"*80)
print("")

def prettify(s):
	rem = s[8:]
	result = s[0:8]
	for i in range(0, len(rem), 8):
		if i % 16 == 0:
			result += " "
		result += " " + rem[i : i+8]
	return result


for nalu in nal_units:
	if nalu & 0x80 and nalu != 0xa000:
		print("0x%02x packet dump:"%nalu)
		for p in packets:
			nal_unit = p[NAL_UNIT]
			if nal_unit == nalu:
				print( prettify(p[0:100].hex()) )
	print("")


print("")
print("-"*80)
print("")

for nalu in [0xa0, 0xa1]:
	print("0x%02x packet decoding:" % nalu)

	decode_bytes = [6, 13]

	print("%5s " % "count", end="")
	for i in range(0, 44-7, 4):
		print("%10s %11s " % ("uint@%d"%i, "float@%d"%i), end="")
	for b in decode_bytes:
		print("%4s " % ("b%d"%b), end="")
	print("")

	count = 0
	for p in packets:
		if p[NAL_UNIT] == nalu and len(p)==45:
			count = count + 1
			print("%5d "%count, end="")
			payload = p[NAL_UNIT+1 : -1]
			for i in range(0, len(payload)-3, 4):
				chunk = payload[i : i+4]
				as_uint32 = (chunk[3]<<24) + (chunk[2]<<16) + (chunk[1]<<8) + chunk[0]
				[as_float]  = struct.unpack('f', chunk)
				
				print("%10d %11.4e " % (as_uint32, as_float), end="")

			for b in decode_bytes:
				print("%4d " % (payload[b] if b<len(payload) else -1), end="")
			
			print("")
	print("")


# filter out all packages with the forbidden bit set
packets = [p for p in packets if p[NAL_UNIT] & 0x80 == 0]
#packets = [p for p in packets if p[NAL_UNIT] in [0x41, 0x65, 0x67, 0x68]]

open("out.bin", "wb").write(bytes().join(packets))


