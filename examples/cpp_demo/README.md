libcopter C++ example
=====================

This aims to provide a minimal demonstration example for controlling a SG500
drone using libcopter.

Building
--------

In this directory, perform the following steps:

```
mkdir build
cd build
cmake ..
make
```

This should build both the static library *libcopter_static.a* and the *demo*
program.

Usage
-----

Connect to your drone's WiFi. You might or might not need to disconnect your
wired network first.

Then run the `demo` program located in the `build/` directory.

The video stream should be displayed.

The controls are as follows:

  - **wasd**: go forward/backward/left/right
  - **rf**: go up/down
  - **qe**: rotate left/right
  - **x**: takeoff
  - **c**: panic
  - **z**: land
  - **123**: reset all speeds to zero

Note that all commands **add** up to the current speed. Do **not**
press-and-hold the keys. Instead, pressing **w** once goes forward slowly until
**s** is pressed. Pressing **w** three times and **d** once goes forward with a
high speed and a bit to the right. Then pressing **1** is equivalent to pressing
**s** three times and **a** once.

Troubleshooting
---------------

*No video stream*: retry, maybe power-cycle your copter. It sometimes does not
work on the first try.

*Infinite loop when connecting*: Probably time for some wireshark. Sorry.
