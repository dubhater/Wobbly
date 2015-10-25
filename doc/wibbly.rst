Wibbly
######

Wibbly consists of one main window and several tool windows. The tool windows can either float on their own, or they can be docked inside the main window. To dock a tool window, you can double click on its title bar, or you can drag it over the main window near an edge. The main window will react when the tool window is hovering approximately in the right place.

Wibbly will remember which windows were shown, their sizes and locations, and whether they were docked or not. You should only have to arrange the windows once.


When to use Wibbly
==================

- When you want to work with only part(s) of a video file (check the "Trim" box).

- When you want to improve upon VFM and/or VDecimate's decisions, either manually or using Wobbly's pattern guessing features (check the "Field matching" and/or "Decimation" boxes).

- When you want to make sure all interlaced fades are taken care of, and no clean frames are touched (check the "Interlaced fades" box).

- When you want to filter per scene (check the "Scene changes" box).


Keyboard shortcuts
==================

+-------------------+---------------------------------------+
| Left              | Move 1 frame back.                    |
+-------------------+---------------------------------------+
| Right             | Move 1 frame forward.                 |
+-------------------+---------------------------------------+
| Ctrl+Left         | Move 5 frames back.                   |
+-------------------+---------------------------------------+
| Ctrl+Right        | Move 5 frames forward.                |
+-------------------+---------------------------------------+
| Alt+Left          | Move 50 frames back.                  |
+-------------------+---------------------------------------+
| Alt+Right         | Move 50 frames forward.               |
+-------------------+---------------------------------------+
| Ctrl+Home         | Jump to the first frame.              |
+-------------------+---------------------------------------+
| Ctrl+End          | Jump to the last frame.               |
+-------------------+---------------------------------------+
| Page Down         | Jump 20% back.                        |
+-------------------+---------------------------------------+
| Page Up           | Jump 20% forward.                     |
+-------------------+---------------------------------------+
| Ctrl+Up           | Move to the previous job.             |
+-------------------+---------------------------------------+
| Ctrl+Down         | Move to the next job.                 |
+-------------------+---------------------------------------+
| [                 | Start a trim.                         |
+-------------------+---------------------------------------+
| ]                 | End a trim.                           |
+-------------------+---------------------------------------+
| A                 | Add the trim to the list of trims.    |
+-------------------+---------------------------------------+

Generally, they work everywhere in Wibbly, but some of these shortcuts will not work when certain widgets have the keyboard focus, e.g. the left and right arrows in text input widgets.


Main window
===========

Here you can add metrics collection jobs, by selecting video files. d2vsource is used to open files with "d2v" extension, LibavSMASHSource to open files with "mp4", "m4v", and "mov" extension, and LWLibavSource to open everything else.

You may configure multiple jobs at the same time, by selecting them and changing stuff.

The names of the project files can be automatically numbered. To do this, select the desired jobs, insert the string "%1" into the destination name where the numbers need to go, and click the Autonumber button. For example, to obtain project files named "asdf1.json", "asdf2.json", etc. make their names "asdf%1.json". The numbers start at 1. They are padded with only enough zeroes so they all have the same number of digits, i.e. if you select fewer than 10 jobs, no padding is done.


Video output window
===================

YUV is always converted to RGB using the BT 601 matrix.


VFM window
==========

See http://www.vapoursynth.com/doc/plugins/vivtc.html for information about each parameter. A few parameters are hardcoded thusly: "field" is always the opposite of "order", "mode" is always 0, and "micout" is always 1. These values are required to collect useful metrics from VFM.


Random remarks
==============

You should probably crop away any black borders, so that any junk there doesn't distort the collected metrics. Cropping slightly more than necessary is fine. It's best to crop only by multiples of 4 at the top and bottom.

If you select the field matching step, make sure to pick the correct field order in the VFM window. (If the chosen field order is incorrect, VFM cannot do its job and you get the usual 2 interlaced frames out of every 5.)

