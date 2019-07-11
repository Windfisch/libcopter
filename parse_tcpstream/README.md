# Parsing the video and telemetry TCP stream

This C++ tool can read a tcp stream dump. It will split it into
valid H264 video packets and telemetry packets, feed the former
into libavcodec to obtain RGB images (which are saved frame-by-frame
to the current working directory) and parse the latter according
to [this documentation](../reverse_engineering/README.md).

## Building

Prerequisites: FFMPEG and its libraries

Just run `make`

