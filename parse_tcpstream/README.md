# Parsing the video and telemetry TCP stream

This C++ tool can read a tcp stream dump. It will split it into
valid H264 video packets and telemetry packets, feed the former
into libavcodec to obtain RGB images (which are saved frame-by-frame
to the current working directory) and parse the latter according
to [this documentation](../reverse_engineering/README.md).

Also, there are python bindings that use *boost::python*.

## Building `decode_video`

Prerequisites: FFMPEG and its libraries

Just run `make`

## Building the python bindings

```
mkdir build
cd build
cmake ..
make
```

This will create `libcopter.so`. Move it to this directory, and
then run `python_test.py` to test it.
