
noinst_LTLIBRARIES = libcommon.la

libcommon_la_SOURCES = \
	eggaccelerators.c	\
	eggaccelerators.h	\
	msd-keygrab.c		\
	msd-keygrab.h		\
	msd-osd-window.c	\
	msd-osd-window.h

libcommon_la_CPPFLAGS = \
	$(AM_CPPFLAGS)

libcommon_la_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

libcommon_la_LDFLAGS = \
	$(MSD_PLUGIN_LDFLAGS) $(X11_LIBS)

libcommon_la_LIBADD  = \
	$(SETTINGS_PLUGIN_LIBS)
