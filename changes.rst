v4 (20160718)
=============

Wobbly
------

* Fix crash when jumping to an interlaced fade (bug introduced in v1).

* Fix off-by-one error in the interlaced fades window (bug introduced
  in v1).

Both
----

* Correctly create projects that lack decimation metrics, mics, or
  matches. Such projects created by previous versions would cause an
  error when loaded in Wobbly. (Bug introduced in v2).

* Fix JSON keys made of four words. The last two words were not
  separated. (Bug introduced in v2.)



v3 (20160408)
=============

Wibbly
------

* Use std.PlaneStats instead of std.PlaneAverage, because VapourSynth
  r32 doesn't have std.PlaneAverage anymore. Wibbly will continue to
  work with older VapourSynth if the interlaced fades thing isn't used.

Wobbly
------

* Make the interface more responsive by requesting frames from
  VapourSynth asynchronously.

* Show thumbnails of frames n-1, n, n+1 under the video frame.

Both
----

* Work around an error message with interlaced videos by deleting the
  ``_FieldBased`` frame property when converting to RGB for display
  purposes. This means interlaced video with vertical subsampling will
  look slightly wrong in Wibbly and Wobbly.



v2 (20160128)
=============

Wibbly
------

* Fix crash when cancelling the metrics collection jobs, caused by
  freeing the VapourSynth node while there were unfinished frame
  requests from it.

* Fix intermittent crashes during metrics collection, caused by
  accessing the GUI from worker threads.

* Avoid script evaluation error messages when deleting jobs.

Wobbly
------

* Fix crash when creating the first preset.

* Fix wrong decimation when guessing the pattern from mics.

* Allow importing presets, custom lists, etc. from other projects.

* Add submenu with the last ten opened projects or videos.

Both
----

* Don't quit when required VapourSynth plugins are missing. Instead
  there will be lots of error messages when attempting to use missing
  plugins.

* Require VapourSynth r29 or newer instead of zimg.

* Add message handlers for messages from VapourSynth. Fatal errors
  detected by VapourSynth's core will now be caught and the user's work
  will be saved before crashing.

* Allow scrolling the video by clicking and dragging.

* Greatly speed up project saving by using RapidJSON instead of QJson.
  Time to save a typical project file (45000 frames) before: ~10
  seconds; after: unnoticeable.

* Remember the last folder visited with a file selector dialog.



v1 (20150818)
=============

* Initial release, fairly buggy.
