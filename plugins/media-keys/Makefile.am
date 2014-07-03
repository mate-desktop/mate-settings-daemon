icondir = $(datadir)/icons/mate
context = actions

NULL =

plugin_LTLIBRARIES = libmedia-keys.la

BUILT_SOURCES = 			\
	msd-media-keys-manager-glue.h	\
	msd-marshal.h			\
	msd-marshal.c			\
	$(NULL)

ICON_FILES = 			\
	touchpad-disabled-16.png	\
	touchpad-enabled-16.png		\
	touchpad-disabled-22.png	\
	touchpad-enabled-22.png		\
	touchpad-disabled-24.png	\
	touchpad-enabled-24.png		\
	touchpad-disabled-32.png	\
	touchpad-enabled-32.png		\
	touchpad-disabled-48.png	\
	touchpad-enabled-48.png		\
	touchpad-disabled.svg		\
	touchpad-enabled.svg

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(icondir)/16x16/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/22x22/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/24x24/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/32x32/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/scalable/$(context)
	$(INSTALL_DATA) $(srcdir)/touchpad-enabled-16.png $(DESTDIR)$(icondir)/16x16/$(context)/touchpad-enabled.png
	$(INSTALL_DATA) $(srcdir)/touchpad-enabled-22.png $(DESTDIR)$(icondir)/22x22/$(context)/touchpad-enabled.png
	$(INSTALL_DATA) $(srcdir)/touchpad-enabled-24.png $(DESTDIR)$(icondir)/24x24/$(context)/touchpad-enabled.png
	$(INSTALL_DATA) $(srcdir)/touchpad-enabled-32.png $(DESTDIR)$(icondir)/32x32/$(context)/touchpad-enabled.png
	$(INSTALL_DATA) $(srcdir)/touchpad-enabled.svg $(DESTDIR)$(icondir)/scalable/$(context)/touchpad-enabled.svg
	$(INSTALL_DATA) $(srcdir)/touchpad-disabled-16.png $(DESTDIR)$(icondir)/16x16/$(context)/touchpad-disabled.png
	$(INSTALL_DATA) $(srcdir)/touchpad-disabled-22.png $(DESTDIR)$(icondir)/22x22/$(context)/touchpad-disabled.png
	$(INSTALL_DATA) $(srcdir)/touchpad-disabled-24.png $(DESTDIR)$(icondir)/24x24/$(context)/touchpad-disabled.png
	$(INSTALL_DATA) $(srcdir)/touchpad-disabled-32.png $(DESTDIR)$(icondir)/32x32/$(context)/touchpad-disabled.png
	$(INSTALL_DATA) $(srcdir)/touchpad-disabled.svg $(DESTDIR)$(icondir)/scalable/$(context)/touchpad-disabled.svg

uninstall-local:
	rm -f $(DESTDIR)$(icondir)/16x16/$(context)/touchpad-enabled.png
	rm -f $(DESTDIR)$(icondir)/22x22/$(context)/touchpad-enabled.png
	rm -f $(DESTDIR)$(icondir)/24x24/$(context)/touchpad-enabled.png
	rm -f $(DESTDIR)$(icondir)/32x32/$(context)/touchpad-enabled.png
	rm -f $(DESTDIR)$(icondir)/scalable/$(context)/touchpad-enabled.svg
	rm -f $(DESTDIR)$(icondir)/16x16/$(context)/touchpad-disabled.png
	rm -f $(DESTDIR)$(icondir)/22x22/$(context)/touchpad-disabled.png
	rm -f $(DESTDIR)$(icondir)/24x24/$(context)/touchpad-disabled.png
	rm -f $(DESTDIR)$(icondir)/32x32/$(context)/touchpad-disabled.png
	rm -f $(DESTDIR)$(icondir)/scalable/$(context)/touchpad-disabled.svg

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

libmedia_keys_la_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
    $(LIBMATEMIXER_CFLAGS)   \
    $(LIBCANBERRA_CFLAGS)   \
	$(AM_CFLAGS)

libmedia_keys_la_LDFLAGS = 		\
	$(MSD_PLUGIN_LDFLAGS)

libmedia_keys_la_LIBADD  = 		\
	$(top_builddir)/plugins/common/libcommon.la			\
	$(SETTINGS_PLUGIN_LIBS)						\
    $(LIBMATEMIXER_LIBS)                     \
    $(LIBCANBERRA_LIBS)         \
	$(XF86MISC_LIBS)						\
	-lm

plugin_in_files = 		\
	media-keys.mate-settings-plugin.in

plugin_DATA = $(plugin_in_files:.mate-settings-plugin.in=.mate-settings-plugin)

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
	-I$(top_srcdir)/plugins/media-keys/cut-n-paste		\
	-DDATADIR=\""$(datadir)"\"				\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DMATE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

test_media_window_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

test_media_window_LDADD = \
	$(top_builddir)/plugins/common/libcommon.la			\
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)			\
	$(XF86MISC_LIBS)			\
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

test_media_keys_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
    $(LIBMATEMIXER_CFLAGS)   \
    $(LIBCANBERRA_CFLAGS)   \
	$(AM_CFLAGS)

test_media_keys_LDADD = \
	$(top_builddir)/mate-settings-daemon/libmsd-profile.la		\
	$(top_builddir)/plugins/common/libcommon.la			\
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)			\
    $(LIBMATEMIXER_LIBS)         \
    $(LIBCANBERRA_LIBS)         \
	$(XF86MISC_LIBS)			\
	-lm

gtkbuilderdir = $(pkgdatadir)
gtkbuilder_DATA =	\
	acme.ui		\
	$(NULL)

EXTRA_DIST = 				\
	msd-media-keys-manager.xml	\
	msd-marshal.list		\
	$(plugin_in_files)		\
	$(gtkbuilder_DATA)		\
	$(pixmaps_DATA)			\
	touchpad-enabled-template.svg	\
	touchpad-disabled-template.svg	\
	$(ICON_FILES)

CLEANFILES = 			\
	$(BUILT_SOURCES)	\
	$(plugin_DATA)

DISTCLEANFILES =		\
	$(plugin_DATA)

@MSD_INTLTOOL_PLUGIN_RULE@
