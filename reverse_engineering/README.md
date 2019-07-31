# Reverse engineering the SG500 drone

First, use mon_on.sh to put your WiFi interface into monitor mode.
Use airodump-ng to find out the channel where the drone sends on,
and then use it to dump the traffic between the drone and the
smartphone.

Open the .cap file in wireshark and use the following filter:

tcp or (udp and not mdns and not dhcp and not dns)

This should give you two streams: One TCP stream on port 8080 and
a sequence of UDP packets on port 8888.

## UDP packets

We analyze the UDP stream more detailedly later. But note how it
starts with always the same prelude:

```
Smartphone                                      Drone

                        0x0F (reset)
            --------------------------------->



                        0x28 (request type)
            --------------------------------->
                                             | 
                        "UDP720P"            |
            <---------------------------------


                        0x42 (request version)
            --------------------------------->
                                             | 
                        "V2.4.8"             |
            <---------------------------------


              'date -s "2019-07-10 13:37:42'
            --------------------------------->

             [binary date package, see below]
            --------------------------------->

                        "timeok"
            <---------------------------------


the binary date package starts with 0x26, and then there follows as
four-byte, little endian encoded 32bit integers: the year, month, day,
day-of-week, hour, minute, second.

the "timeok" seems to be sent only sometimes.
```

We can verify this even better with the [fake access point](accesspoint).
See [its README](accesspoint/README.md) for details.

## The TCP stream

The TCP stream can be saved to a file using Wireshark's "follow stream"
functionality. After saving it, it can be analyzed further using
`fix_stream.py` which outputs statistics and writes a usable H264
video stream.

H264 streams consist of various packets which are indicated by a
0x00 0x00 0x01 0x?? sequence, where 0x?? denotes the NAL Unit type.
This type is a 7-bit value, where the 8th bit must be always zero
according to the H264 standard. For more details,
[Yumi Chan's Blog](https://yumichan.net/video-processing/video-compression/introduction-to-h264-nal-unit/)
has lots of information.

However, the stream returned by the drone contains lots of rather short
packages with that bit set, violating the H264 standard:

```
NALU statistics (0x00 0x00 0x01 0x?? sequences): 
	         0x??:   cnt,     avg pkt len
	a0 (10100000):  7840,  len =     44.8  <-- ???
	41 ( 1000001):  7840,  len =   6112.5  <-- non-IDR picture aka difference frame
	a1 (10100001):   160,  len =     44.3  <-- ???
	67 ( 1100111):   160,  len =     32.0
	68 ( 1101000):   160,  len =     16.0
	 6 (     110):   160,  len =     59.0
	65 ( 1100101):   160,  len =  25203.3  <-- IDR picture aka keyframe
	19 (   11001):    30,  len =     37.0
	c4 (11000100):     1,  len =     15.0
	ec (11101100):     1,  len =     15.0
	fb (11111011):     1,  len =     15.0
	43 ( 1000011):     1,  len =     15.0
	db (11011011):     2,  len =     15.0
	98 (10011000):     2,  len =     15.0
	53 ( 1010011):     2,  len =     15.0
	8b (10001011):     2,  len =     15.0
	a8 (10101000):     2,  len =     15.0
	d8 (11011000):     2,  len =     15.0
	b5 (10110101):     2,  len =     15.0
	23 (  100011):     2,  len =     15.0
	85 (10000101):     2,  len =     15.0
	34 (  110100):     2,  len =     15.0
```

As you can see, there's one 0xA0 packet for every difference frame, and a 0xA1
packet for every key frame. They're sent alternatingly:

```
cumulative counts of NALU kinds:
 id       6    19    23    34    41    43    53    65    67    68    85    8b    98    a0    a1    a8    b5    c4    d8    db    ec    fb
 35       0     0     0     0    18     0     0     0     0     0     0     0     0    18     0     0     0     0     0     0     0     0
 36       0     0     0     0    18     0     0     0     0     0     0     0     0    19     0     0     0     0     0     0     0     0
 37       0     0     0     0    19     0     0     0     0     0     0     0     0    19     0     0     0     0     0     0     0     0
 38       0     0     0     0    19     0     0     0     0     0     0     0     0    20     0     0     0     0     0     0     0     0
 39       0     0     0     0    20     0     0     0     0     0     0     0     0    20     0     0     0     0     0     0     0     0
 40       0     0     0     0    20     0     0     0     0     0     0     0     0    20     1     0     0     0     0     0     0     0
 41       0     0     0     0    20     0     0     0     1     0     0     0     0    20     1     0     0     0     0     0     0     0
 42       0     0     0     0    20     0     0     0     1     1     0     0     0    20     1     0     0     0     0     0     0     0
 43       1     0     0     0    20     0     0     0     1     1     0     0     0    20     1     0     0     0     0     0     0     0
 44       1     0     0     0    20     0     0     1     1     1     0     0     0    20     1     0     0     0     0     0     0     0
 45       1     0     0     0    20     0     0     1     1     1     0     0     0    21     1     0     0     0     0     0     0     0
 46       1     0     0     0    21     0     0     1     1     1     0     0     0    21     1     0     0     0     0     0     0     0
 47       1     0     0     0    21     0     0     1     1     1     0     0     0    22     1     0     0     0     0     0     0     0
 48       1     0     0     0    22     0     0     1     1     1     0     0     0    22     1     0     0     0     0     0     0     0
 49       1     0     0     0    22     0     0     1     1     1     0     0     0    23     1     0     0     0     0     0     0     0
 50       1     0     0     0    23     0     0     1     1     1     0     0     0    23     1     0     0     0     0     0     0     0
```

See how the a0 count increases immediately before the 41 count increases, and
how the a1 is preceding the 65,67,78 and 06 NAL headers?

Upon filtering these out, the filtered stream can be played back
using `ffplay` or `mpv`. The correct video stream consists (among
other types) mainly of key frames and difference frames.

The A0 and A1 packets have the same format:

```
0xa0 packet dump:
000001a0  00000000 0000d119  00001800 85190000  d0020005 00000000  00000000 9714ffe8  c58b0500 00000000  00
000001a0  00000000 0000d219  00001800 45230000  d0020005 00000000  00000000 df9cffe8  c58b0500 00000000  00
000001a0  00000000 0000d319  00001800 951c0000  d0020005 00000000  00000000 da2900e9  c58b0500 00000000  00
000001a0  00000000 0000d419  00001800 c51d0000  d0020005 00000000  00000000 4ee500e9  c58b0500 00000000  00
000001a0  00000000 0000d519  00001800 951b0000  d0020005 00000000  00000000 1e6e01e9  c58b0500 00000000  00
000001a0  00000000 0000d619  00001800 d51b0000  d0020005 00000000  00000000 e12902e9  c58b0500 00000000  00
000001a0  00000000 0000d719  00001800 e51c0000  d0020005 00000000  00000000 09bf02e9  c58b0500 00000000  00

0 1 2 3   4 5 6 7  8 9 a b   c d e f  0 1 2 3   4 5 6 7  8 9 a b   c d e f  0 1 2 3   4 5 6 7  8 9 a b   c
0                                     1                                     2
```

The following things are obvious:

 - The byte `packet[0xa]` seems to be a 8-bit counter that counts from 00 to FF, then resumes at 00 again
 - There uint32 at `packet[0x10]` looks like a payload data, while `packet[0x20]` seems to be monotonic increasing.

In fact, `packet[0x20]` increases by pretty much 1000000 per 25 frames, so it seems to be a timestamp in microseconds.
This is true for 0xA0 and 0xA1 type packets.

`packet[0x10]` seems to be some sort of telemetry data with different meaning for 0xa0 and 0xa1 packets.
For 0xA0 packets, it seems to indicate the height of the copter, while for 0xA1 it's a value that drops
whenever the motors are turned up. Maybe a battery voltage?

TODO: insert graphs here.

The client app sends the following data to the drone over the video tcp stream every second to keep the
stream going: `00 01 02 03 04 05 06 07 08 09 28 28` (hex)

Also, there's another tcp stream on the same port, that seems to do the same chat every 1.5 seconds:

```
  (to drone) 00 01 02 03 04 05 06 07 08 09 25 25
(from drone) 6e 6f 61 63 74 0d 0a                 "noact\r\n"
```

## Telnet server

There's a telnet server on port 23 running, offering a prompt of the "Finsh" shell of the chinese
*thread real time operating system* ([Github](https://github.com/RT-Thread),
[Wikipedia](https://en.wikipedia.org/wiki/RT-Thread))

TODO: find out what it can do.
