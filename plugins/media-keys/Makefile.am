icondir = $(datadir)/icons/mate
context = actions

NULL =

plugin_LTLIBRARIES = libmedia-keys.la

BUILT_SOURCES = 			\
	msd-media-keys-manager-glue.h	\
	msd-marshal.h			\
	msd-marshal.c			\
	$(NULL)

msd-media-keys-manager-glue.h: msd-media-keys-manager.xml Makefile
	$(AM_V_GEN) dbus-binding-tool --prefix=msd_media_keys_manager --mode=glib-server $< > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $@ || cp xgen-$(@F) $@ ) \
	&& rm -f xgen-$(@F)

msd-marshal.c: msd-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=msd_marshal $< --header --body --internal > $@

msd-marshal.h: msd-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=msd_marshal $< --header --internal > $@

libmedia_keys_la_SOURCES = 		\
	msd-media-keys-plugin.h		\
	msd-media-keys-plugin.c		\
	msd-media-keys-manager.h	\
	msd-media-keys-manager.c	\
	msd-media-keys-window.h		\
	msd-media-keys-window.c		\
	acme.h				\
	$(BUILT_SOURCES)		\
	$(NULL)

libmedia_keys_la_CPPFLAGS = \
	-I$(top_srcdir)/mate-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DMATE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libmedia_keys_la_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(LIBMATEMIXER_CFLAGS)		\
	$(LIBCANBERRA_CFLAGS)		\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(NULL)

libmedia_keys_la_LDFLAGS = 		\
	$(MSD_PLUGIN_LDFLAGS)

libmedia_keys_la_LIBADD  = \
	$(top_builddir)/plugins/common/libcommon.la \
	$(SETTINGS_PLUGIN_LIBS) \
	$(LIBMATEMIXER_LIBS) \
	$(LIBCANBERRA_LIBS) \
	-lm

plugin_in_files = 		\
	media-keys.mate-settings-plugin.desktop.in

plugin_DATA = $(plugin_in_files:.mate-settings-plugin.desktop.in=.mate-settings-plugin)

noinst_PROGRAMS =				\
	test-media-keys				\
	test-media-window			\
	$(NULL)

test_media_window_SOURCES =			\
	msd-media-keys-window.c			\
	msd-media-keys-window.h			\
	test-media-window.c			\
	$(NULL)

test_media_window_CPPFLAGS = \
	-I$(top_srcdir)/mate-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DDATADIR=\""$(datadir)"\"				\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DMATE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

test_media_window_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(NULL)

test_media_window_LDADD = \
	$(top_builddir)/plugins/common/libcommon.la			\
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)			\
	$(GST_LIBS)				\
	-lm

test_media_keys_SOURCES =			\
	msd-media-keys-manager.c		\
	msd-media-keys-manager.h		\
	msd-media-keys-window.h			\
	msd-media-keys-window.c			\
	test-media-keys.c			\
	$(BUILT_SOURCES)			\
	$(NULL)

test_media_keys_CPPFLAGS = \
	-I$(top_srcdir)/mate-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DMATE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

test_media_keys_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(LIBMATEMIXER_CFLAGS)		\
	$(LIBCANBERRA_CFLAGS)		\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(NULL)

test_media_keys_LDADD = \
	$(top_builddir)/mate-settings-daemon/libmsd-profile.la \
	$(top_builddir)/plugins/common/libcommon.la \
	$(SETTINGS_DAEMON_LIBS) \
	$(SETTINGS_PLUGIN_LIBS) \
	$(LIBMATEMIXER_LIBS) \
	$(LIBCANBERRA_LIBS) \
	-lm

gtkbuilderdir = $(pkgdatadir)
gtkbuilder_DATA =	\
	acme.ui		\
	$(NULL)

EXTRA_DIST = 				\
	msd-media-keys-manager.xml	\
	msd-marshal.list		\
	$(plugin_in_files)		\
	$(gtkbuilder_DATA)

CLEANFILES = 			\
	$(BUILT_SOURCES)	\
	$(plugin_DATA)

DISTCLEANFILES =		\
	$(plugin_DATA)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@
