Description
===========

There are two executables, Wibbly and Wobbly.

Wibbly gathers metrics and creates project files that Wobbly can open. (Wobbly can also open video files directly.)

Wobbly can be used to filter a video per scene, and/or to improve upon VFM and VDecimate's decisions.


Compilation
===========

The usual steps work::
    ./autogen.sh
    ./configure
    make

Requirements:
    - A C++11 compiler.

    - Qt 5 (just qtbase). It is unclear which version of Qt 5 is the earliest that works.

    - VapourSynth.


License
=======

The code itself is available under the ISC license.
