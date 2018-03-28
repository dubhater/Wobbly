warningflags = -Wall -Wextra -Wshadow
includeflags = -I$(srcdir) -I$(srcdir)/src/shared
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

shared_moc_files = src/shared/moc_CombedFramesModel.cpp \
				   src/shared/moc_DockWidget.cpp \
				   src/shared/moc_ListWidget.cpp \
				   src/shared/moc_ScrollArea.cpp \
				   src/shared/moc_WobblyProject.cpp

wibbly_moc_files = src/wibbly/moc_WibblyWindow.cpp

wobbly_moc_files = src/wobbly/moc_CombedFramesCollector.cpp \
				   src/wobbly/moc_FrameLabel.cpp \
				   src/wobbly/moc_ImportWindow.cpp \
				   src/wobbly/moc_OverlayLabel.cpp \
				   src/wobbly/moc_PresetTextEdit.cpp \
				   src/wobbly/moc_SpinBox.cpp \
				   src/wobbly/moc_TableView.cpp \
				   src/wobbly/moc_TableWidget.cpp \
				   src/wobbly/moc_WobblyWindow.cpp


MOSTLYCLEANFILES = $(shared_moc_files) \
				   $(wibbly_moc_files) \
				   $(wobbly_moc_files)

rapidjson_sources = rapidjson/allocators.h \
					rapidjson/document.h \
					rapidjson/encodedstream.h \
					rapidjson/encodings.h \
					rapidjson/filereadstream.h \
					rapidjson/filewritestream.h \
					rapidjson/memorybuffer.h \
					rapidjson/memorystream.h \
					rapidjson/pointer.h \
					rapidjson/prettywriter.h \
					rapidjson/rapidjson.h \
					rapidjson/reader.h \
					rapidjson/stringbuffer.h \
					rapidjson/writer.h \
					rapidjson/error/en.h \
					rapidjson/error/error.h \
					rapidjson/internal/biginteger.h \
					rapidjson/internal/diyfp.h \
					rapidjson/internal/dtoa.h \
					rapidjson/internal/ieee754.h \
					rapidjson/internal/itoa.h \
					rapidjson/internal/meta.h \
					rapidjson/internal/pow10.h \
					rapidjson/internal/stack.h \
					rapidjson/internal/strfunc.h \
					rapidjson/internal/strtod.h \
					rapidjson/internal/swap.h \
					rapidjson/msinttypes/inttypes.h \
					rapidjson/msinttypes/stdint.h

shared_sources = $(rapidjson_sources) \
				 src/shared/CombedFramesModel.cpp \
				 src/shared/CombedFramesModel.h \
				 src/shared/DockWidget.cpp \
				 src/shared/DockWidget.h \
				 src/shared/ListWidget.cpp \
				 src/shared/ListWidget.h \
				 src/shared/ScrollArea.cpp \
				 src/shared/ScrollArea.h \
				 src/shared/WobblyProject.cpp \
				 src/shared/WobblyProject.h \
				 src/shared/WobblyException.h \
				 src/shared/WobblyTypes.h

wobbly_SOURCES = $(shared_sources) \
				 src/wobbly/CombedFramesCollector.cpp \
				 src/wobbly/CombedFramesCollector.h \
				 src/wobbly/FrameLabel.cpp \
				 src/wobbly/FrameLabel.h \
				 src/wobbly/ImportWindow.cpp \
				 src/wobbly/ImportWindow.h \
				 src/wobbly/OverlayLabel.cpp \
				 src/wobbly/OverlayLabel.h \
				 src/wobbly/PresetTextEdit.cpp \
				 src/wobbly/PresetTextEdit.h \
				 src/wobbly/SpinBox.cpp \
				 src/wobbly/SpinBox.h \
				 src/wobbly/TableView.cpp \
				 src/wobbly/TableView.h \
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
