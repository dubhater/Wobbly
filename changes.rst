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
