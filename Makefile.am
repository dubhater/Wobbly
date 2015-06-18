warningflags = -Wall -Wextra -Wno-unused-parameter -Wshadow
includeflags = -I$(srcdir)/src/shared
commoncflags = -fPIC -O2 $(warningflags) $(includeflags)
AM_CXXFLAGS = -std=c++11 $(commoncflags)
AM_CFLAGS = -std=c99 $(commoncflags)



moc_verbose = $(moc_verbose_$(V))
moc_verbose_ = $(moc_verbose_$(AM_DEFAULT_VERBOSITY))
moc_verbose_0 = @echo "  MOC     " $@;


%.moc : %.h
	$(moc_verbose)$(MOC) -i -o "$@" "$<"

bin_PROGRAMS = wobbly

moc_files = src/wobbly/PresetTextEdit.moc \
			src/wobbly/WobblyWindow.moc

$(wobbly_OBJECTS) : \
	$(moc_files)

MOSTLYCLEANFILES = $(moc_files)

wobbly_SOURCES = src/wobbly/PresetTextEdit.cpp \
				 src/wobbly/PresetTextEdit.h \
				 src/wobbly/Wobbly.cpp \
				 src/wobbly/WobblyWindow.cpp \
				 src/wobbly/WobblyWindow.h \
				 src/shared/WobblyProject.cpp \
				 src/shared/WobblyProject.h \
				 src/shared/WobblyException.h

wobbly_LDFLAGS = $(QT5WIDGETS_LIBS) $(VSScript_LIBS)

wobbly_CPPFLAGS = $(QT5WIDGETS_CFLAGS) $(VSScript_CFLAGS)


