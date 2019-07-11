# libcopter, or rather libSG500 for now

This aims to be a library that supports various similar cheap
china copters that are controlled via WiFi and have a camera on
board.

Currently, it is developed for the **SG500** drone, but it seems
that at least the **blue crab** drone works the very same way.

The goal of this library will be to support controlling the drone
as well as retrieving telemetry information and the video stream
for further use in the OpenCV library.

I aim to support both C++ and Python 3 with numpy.

## Current state

Currently, I'm still reverse engineering the drone. Most I've
already figured out, but there is still some way to go. Find
my reverse engineering attempts [here](reverse_engineering).

There's [a python script](dronecontrol) that can generate
steering packets and can successfully take-off and land
the copter.

Also, there's [a video stream parser](parse_tcpstream) written
in C++ that splits the stream into telemetry data and a video
stream and parses both. I am currently working on python bindings.

**This project is in a _very_ early state, do not expect much
from it yet.**
