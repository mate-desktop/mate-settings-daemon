plugin_LTLIBRARIES = libmouse.la

libmouse_la_SOURCES = 		\
	msd-mouse-plugin.h	\
	msd-mouse-plugin.c	\
	msd-mouse-manager.h	\
	msd-mouse-manager.c

libmouse_la_CPPFLAGS = \
	-I$(top_srcdir)/mate-settings-daemon		\
	-I$(top_srcdir)/plugins/common/			\
	-DMATE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	$(AM_CPPFLAGS)

libmouse_la_CFLAGS =			\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

libmouse_la_LDFLAGS = 		\
	$(MSD_PLUGIN_LDFLAGS)

libmouse_la_LIBADD  =							\
	$(top_builddir)/plugins/common/libcommon.la			\
	$(SETTINGS_PLUGIN_LIBS)						\
	$(X11_LIBS)							\
	$(XINPUT_LIBS)

plugin_in_files = mouse.mate-settings-plugin.desktop.in

plugin_DATA = $(plugin_in_files:.mate-settings-plugin.desktop.in=.mate-settings-plugin)

libexec_PROGRAMS = msd-locate-pointer

msd_locate_pointer_SOURCES = 	\
	msd-locate-pointer.h	\
	msd-locate-pointer.c	\
	msd-timeline.h		\
	msd-timeline.c

msd_locate_pointer_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

msd_locate_pointer_LDADD  = 		\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(X11_LIBS)			\
	-lm

EXTRA_DIST = $(plugin_in_files)
CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@
