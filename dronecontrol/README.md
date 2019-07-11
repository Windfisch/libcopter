Communication software
======================

`comm.py` will connect to your drone and launch it.

**Safety warning: this will take-off your drone without any confirmation.**
Be prepared to catch it mid-air in case it does not auto-land as expected within
a couple of seconds. Note that the `panic` command is sent after 8 or so seconds,
causing the drone to immediately stop its motors and fall down.
