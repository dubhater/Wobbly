Wobbly
######

Wobbly consists of one main window and several tool windows. The tool windows can either float on their own, or they can be docked inside the main window. To dock a tool window, you can double click on its title bar, or you can drag it over the main window near an edge. The main window will react when the tool window is hovering approximately in the right place.

Wobbly will remember which windows were shown, their sizes and locations, and whether they were docked or not. You should only have to arrange the windows once.


Main window
===========

Video files can be opened directly. Everything works pretty much the same as when opening Wobbly projects. d2vsource is used to open files with "d2v" extension, LibavSMASHSource to open files with "mp4", "m4v", and "mov" extension, and LWLibavSource to open everything else.

Zooming is done with nearest neighbour scaling. The maximum ratio is 8.

By default, YUV is converted to RGB using the BT 709 matrix. This can be changed in the Settings window.


Frame details window
====================

- "Frame": the current frame number, before and after decimation. Wobbly always knows exactly which frames will be decimated, so the frame numbers after decimation can be calculated accurately.

  When previewing, the second number is bold.

- "Time": the current frame's time, before decimation.

- "Matches": shows the matches for the current frame and for 10 frames before and after.

- "Section": shows the section the current frame belongs to.

- "Presets": shows the presets assigned to the current section.

- "Custom lists": shows the custom lists active on the current frame.

- "Frozen": a Disney film you should watch. The first two numbers indicate the range of frames that were replaced (frozen), and the third number indicates the replacement frame.

- "DMetric": the current frame's duplicate metric. A frame that is very similar to the previous one will have a low DMetric.

- "Mics": shows the interlacing metric for the five possible matches (P, C, N, B, U). 0 indicates that VFM found no interlacing in the match. 255 indicates that VFM considered the match very interlaced.
  
  The mic for the current match is bold.


Cropping, resizing, bitdepth window
===================================

If this window is visible, cropping is enabled, and you're not in preview mode, then coloured borders will show how much will be cropped.

It is possible to crop either right after the source filter (if "Crop early" is checked), or at the end of the filter chain, before the resizing. Cropping early can be useful when the video has large black borders (4/3 blurays), but in this case you must take care to crop only by multiples of 4 at the top and bottom.

A bit depth conversion filter can be inserted at the very end of the filter chain, after cropping and resizing.


Presets window
==============

Presets are Python functions which take one argument, called "clip", and return the same thing. Behind the scenes, every line you write in a preset will be indented with 4 spaces. The function definition and the return statement will also be added for you. The VapourSynth core object is called "c".

Example::

    # Blurrrrr.
    clip = c.rgvs.RemoveGrain(clip=clip, mode=19)
    clip = c.rgvs.RemoveGrain(clip=clip, mode=19)
    clip = c.rgvs.RemoveGrain(clip=clip, mode=19)


Pattern editor window
=====================

The valid letters for the match pattern are "p", "c", "n", "b", and "u".

The valid letters for the decimation pattern are "k" ("keep") and "d" ("drop").


Custom lists window
===================

A custom list applies a preset to arbitrary ranges of frames.

A custom list can be placed in 3 possible positions in the filter chain:

- Post source

  Right after the source filter and the early cropping (if used), and before field matching.

- Post field match

  Right after field matching, and before the sections.

- Post decimate

  Right after decimation, and before cropping (if cropping isn't early).

Custom lists that have the same position in the filter chain will be inserted in the order they appear in this window.

The frame numbers shown in this window are always from before decimation, even for the custom lists placed after decimation.


Pattern guessing window
=======================

Wobbly has two ways to guess the telecine patterns, one similar to Yatta's pattern guidance ("From mics"), and a new one meant for terrible DVDs with field blending ("From matches").


Random remarks
==============

The default keyboard shortcuts mirror Yatta's. Actions that don't exist in Yatta have no default keyboard shortcuts. The keyboard shortcuts can be edited in the Settings window.

When calculating the time for the current frame, Wobbly uses the video's or the project's frame rate. On the other hand, when creating the timecodes file and when calling AssumeFPS after decimation, Wobbly assumes the input video's frame rate is 30000/1001 fps.

Here is how everything appears in the VapourSynth scripts generated by Wobbly:

- Source filter

- Crop (if early)

- Trim and splice

- Custom lists (post source)

- Field matching

- Custom lists (post field match)

- Sections

- Frozen frames

- Decimation

- Custom lists (post decimate)

- Crop (if not early)

- Resize

- Bit depth

Decimation is done either with DeleteFrames or with SelectEvery, depending on which method generates fewer characters in the VapourSynth script.
