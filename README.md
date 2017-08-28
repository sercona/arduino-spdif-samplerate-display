# arduino-spdif-samplerate-display

![spdif display](https://c1.staticflickr.com/5/4234/35592124081_bfc3b191c6_z.jpg)

First commit; initial set of files that will build on an arduino nano
(atmega 328).

Depends on u8glib library (install that, first, before building this
image).

More details to follow, but connection simply requires 5v, ground and
a 5v level-shifted (ideally, buffered) i2s-based 'word-clock'
(aka LR clock) square wave signal sent to arduino pin-5.  The OLED display
is i2c based and connects to the usual arduino i2c pins.

