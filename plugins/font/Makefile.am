NULL =

plugin_LTLIBRARIES = \
	libfont.la		\
	$(NULL)

libfont_la_SOURCES = 		\
	msd-font-plugin.h	\
	msd-font-plugin.c	\
	msd-font-manager.h	\
	msd-font-manager.c	\
	delayed-dialog.h	\
	delayed-dialog.c	\
	$(NULL)

libfont_la_CPPFLAGS = \
	-I$(top_srcdir)/mate-settings-daemon		\
	-DMATE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libfont_la_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

libfont_la_LDFLAGS = 		\
	$(MSD_PLUGIN_LDFLAGS)	\
	$(NULL)

libfont_la_LIBADD  = 		\
	$(SETTINGS_PLUGIN_LIBS)	\
	$(NULL)

plugin_in_files = 		\
	font.mate-settings-plugin.in	\
	$(NULL)

plugin_DATA = $(plugin_in_files:.mate-settings-plugin.in=.mate-settings-plugin)

EXTRA_DIST = 			\
	$(plugin_in_files)	\
	$(NULL)

CLEANFILES = 			\
	$(plugin_DATA)		\
	$(NULL)

DISTCLEANFILES =		\
	$(plugin_DATA)		\
	$(NULL)

@MSD_INTLTOOL_PLUGIN_RULE@
