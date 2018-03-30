v5 (20200614)
=============

Wibbly
------

* Make the Compact Projects checkbox work. Its value wasn't actually
  used and projects were always saved not compact. (Bug introduced in
  v1.)

* Show overall average processing speed and estimated time remaining.

Wobbly
------

* Fix crash when attempting to preview nothing, i.e. before opening a
  project or video. (Bug introduced in v1.)
  
* Fix crash when decimating all five frames in a cycle by not allowing
  to decimate all five frames in a cycle.

* Fix crash when opening a project which was saved when the crop
  assistant was visible. (Bug introduced in v3.)

* Fix previewing when the crop assistant is visible. Don't show what's
  getting cropped when previewing the final script, because the final
  script already includes the cropping. (Bug introduced in v2.)

* Fix switching to preview and back too quickly. Previously, if you
  switched to preview and back very quickly, before the preview frame
  appeared on screen, then you could end up with the preview frame
  shown instead of the source frame.

* Fix opening projects with bad cropping, like odd numbers
  incompatible with the video's subsampling (or other problems that
  make it generate unusable scripts). When opening such a project,
  script evaluation fails, but that doesn't have to leave the
  application in a semi-invalid state. Instead of closing the
  previously opened project and refusing to open the new one, open the
  new one in the hopes that the user can fix the problem. In the case
  of bad cropping, the user can fix it easily.

* Fix opening videos that can't be opened, e.g. files that aren't
  actually videos. Anything that causes a script evaluation error.
  Don't close the previously opened project when opening such a video.

* Fix the default resize values in the crop assistant. If the project
  didn't have resizing enabled, use the project's width and height as
  default values.

* Fix shortcuts that require the Shift key. Shortcuts like "{" (typed
  with Shift+[) or ">" (Shift+.) were not usable. Shortcuts that
  involve keys from A to Z were fine.

* Don't allow resizing to less than 16x16. The resizer (zimg) has some
  trouble resizing to very small sizes. This will be fixed at some
  point, probably, but work around the problem anyway. Surely no one
  needs to resize to less than 16x16.

* Use a tab bar to switch from source to preview.

* Print the frame details on top of the video, much like Yatta does.
  The frame details window can still be used instead of or along with
  this feature.

* Add action to disable/enable all freezeframes (only in the source
  tab). No default shortcut.
  
* Add window displaying the list of combed frames

* Add a button to redetect all the combed frames. It runs the final
  script with all the filtering set up by the user through
  tdm.IsCombed from TDeintMod.

* Add the possibility to bookmark frames.

* Make the number of thumbnails shown under the video customisable.
  The minimum is none, the maximum 21.

* Make the thumbnail size customisable.

* When quitting only ask to save the current project if it was
  modified.

* Ask to save the current project when opening another project or a
  video (if the current project was modified).

* Change the mouse cursor during potentially slow operations like
  opening a video or project, saving a project, guessing patterns, and
  jumping to another frame.
  
* Make Home, End, Shift+Home, Shift+End keys behave better in tables.
  Make them move to the first/last row, not the first/last column of
  the current row.

* Make the Preview tab update automatically. Most of the actions that
  affect the final script will now update the preview immediately if
  the Preview tab is active. One action that doesn't do this is
  editing a preset. It's too easy to accidentally create an unusable
  preset, which would make an error message appear. So when editing a
  preset switch to the Source tab and back to the Preview tab to make
  it update.

Both
----

* Fix compatibility with VapourSynth R41 and newer. Older VapourSynth
  versions should still work.

* Fix gibberish error messages or crash when VapourSynth returns an
  error message instead of a frame. (Bug introduced in Wobbly in v3,
  and in Wibbly in v1.)

* Use native file selector dialogs. If the application ever froze when
  it should have displayed a file selector dialog, this should fix it.

* Fix the file name passed to vsscript_evaluateScript, so that a more
  correct file name appears in error messages from VapourSynth.

* Fix opening videos with single quotes in the file name.

* Make it possible to focus the scroll areas, so that the user can
  click on the scroll area to move the keyboard focus away from
  widgets like tables, lists, text editors, etc so that the shortcut
  keys defined by the user can be used again.
  
* Give the scroll areas a frame. At least some widget styles will make
  the frame look different when the scroll area has focus, so that's
  possibly useful.
    
* To make the Windows binaries fully portable, store the settings in
  ini files alongside the executables instead of using the Windows
  registry. Settings stored in the registry by previous versions will
  be automatically migrated to the ini files and deleted from the
  registry.
  
* Give the progress dialogs a Minimise button and make the main window
  minimise/restore when the progress dialog is minimised/restored.
  Also make the main window's taskbar entry flash when the progress
  dialog disappears, whether the window was minimised or not.

* Add option to write projects with relative paths. If enabled, the
  project's input_file key will only contain the input file's name,
  without the path to it. It's disabled by default.



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
