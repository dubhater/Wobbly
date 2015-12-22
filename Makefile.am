warningflags = -Wall -Wextra -Wshadow
includeflags = -I$(srcdir)/src/shared
commoncflags = $(FPIC) -O2 $(warningflags) $(includeflags)
AM_CXXFLAGS = -std=c++11 $(commoncflags)
AM_CFLAGS = -std=c99 $(commoncflags)
AM_CPPFLAGS = $(QT5PLATFORMSUPPORT_CFLAGS) $(QT5WIDGETS_CFLAGS) $(VSSCRIPT_CFLAGS)
AM_LDFLAGS = $(WINDOWS_SUBSYSTEM)



moc_verbose = $(moc_verbose_$(V))
moc_verbose_ = $(moc_verbose_$(AM_DEFAULT_VERBOSITY))
moc_verbose_0 = @echo "  MOC     " $@;


moc_%.cpp : %.h
	$(moc_verbose)$(MOC) -o "$@" "$<"

bin_PROGRAMS = wibbly wobbly

shared_moc_files = src/shared/moc_DockWidget.cpp \
				   src/shared/moc_ListWidget.cpp

wibbly_moc_files = src/wibbly/moc_WibblyWindow.cpp

wobbly_moc_files = src/wobbly/moc_ImportWindow.cpp \
				   src/wobbly/moc_PresetTextEdit.cpp \
				   src/wobbly/moc_TableWidget.cpp \
				   src/wobbly/moc_WobblyWindow.cpp


MOSTLYCLEANFILES = $(shared_moc_files) \
				   $(wibbly_moc_files) \
				   $(wobbly_moc_files)

shared_sources = src/shared/DockWidget.cpp \
				 src/shared/DockWidget.h \
				 src/shared/ListWidget.cpp \
				 src/shared/ListWidget.h \
				 src/shared/WobblyProject.cpp \
				 src/shared/WobblyProject.h \
				 src/shared/WobblyException.h

wobbly_SOURCES = $(shared_sources) \
				 src/wobbly/ImportWindow.cpp \
				 src/wobbly/ImportWindow.h \
				 src/wobbly/PresetTextEdit.cpp \
				 src/wobbly/PresetTextEdit.h \
				 src/wobbly/TableWidget.cpp \
				 src/wobbly/TableWidget.h \
				 src/wobbly/Wobbly.cpp \
				 src/wobbly/WobblyWindow.cpp \
				 src/wobbly/WobblyWindow.h \
				 $(shared_moc_files) \
				 $(wobbly_moc_files)

wibbly_SOURCES = $(shared_sources) \
				 src/wibbly/Wibbly.cpp \
				 src/wibbly/WibblyJob.cpp \
				 src/wibbly/WibblyJob.h \
				 src/wibbly/WibblyWindow.cpp \
				 src/wibbly/WibblyWindow.h \
				 $(shared_moc_files) \
				 $(wibbly_moc_files)


LDADD = $(QT5PLATFORMPLUGIN) $(QT5PLATFORMSUPPORT_LIBS) $(QT5WIDGETS_LIBS) $(VSSCRIPT_LIBS)
