Communication software
======================

`comm.py` will connect to your drone and allow you to fly it. It will
display the video stream.

**Safety warning: this will take-off your drone if you press `x`.**
Be prepared to catch it mid-air in case the control program crashes
or you lose control over the drone. Controlling it is very difficult
using only a keyboard.

Preparation
-----------

This is very fiddly for now. Build the `parsestream.so` in
[parse\_tcpstream](../parse_tcpstream). Then move the `.so`-file that was
generated in the `build/` directory to this directory.

Usage
-----

Launch the software with `python3 comm.py`. It should first output the following:

```
sending initialization sequence to the drone
(b'UDP720P', ('172.16.10.1', 8080))      # <-- this is the ID sent by the drone
(b'V2.4.8', ('172.16.10.1', 8080))       # <-- this is the firmware version sent by the drone
(b'V2.4.8', ('172.16.10.1', 8080))
initialization complete, now opening the video+telemetry TCP socket
```

Then, after a while, a OpenCV window should pop up that displays a live
camera view. You can use the following keys in the window:

  - W/A/S/D: pitch/roll the drone forward/backward/sideway. This is cumulative.
    **Caution: releasing the key does not stop / level the drone.** Instead, when pressing
    "W" two times, the drone will fly forward until you press "A" two times.
  - Q/E: rotate (yaw) the drone. The same warning applies here.
  - R/F: rise/descend. The same warning applies here.
  - 1/2/3: cancel any previous movement, i.e. level the drone out
  - X: launch
  - Z: land
  - C: panic. **Caution: your drone will fall down immediately.**

During flight, the telemetry data is being written to stdout.

Files
-----

This tool will create two files:

  - a raw tcp stream dump (`streamdump.bin`). You can analyze it further using
    [the fix\_stream.py tool](../reverse_engineering/).
  - a video dump (`output.avi`, re-encoded using OpenCV's VideoWriter)
