
noinst_LTLIBRARIES = libcommon.la

libcommon_la_SOURCES = \
	eggaccelerators.c	\
	eggaccelerators.h	\
	msd-keygrab.c		\
	msd-keygrab.h		\
	msd-input-helper.c	\
	msd-input-helper.h	\
	msd-osd-window.c	\
	msd-osd-window.h

libcommon_la_CPPFLAGS = \
	$(AM_CPPFLAGS)

libcommon_la_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(XINPUT_CFLAGS)		\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

libcommon_la_LDFLAGS = \
	$(MSD_PLUGIN_LDFLAGS) $(XINPUT_LIBS) $(X11_LIBS)

libcommon_la_LIBADD  = \
	$(SETTINGS_PLUGIN_LIBS)		\
	$(XINPUT_LIBS)
