icondir = $(datadir)/icons/hicolor
context = apps

BUILT_SOURCES =				\
	msd-xrandr-manager-glue.h

ICON_FILES = 			\
	msd-xrandr-16.png	\
	msd-xrandr-22.png	\
	msd-xrandr-24.png	\
	msd-xrandr-32.png	\
	msd-xrandr.svg

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/mate-settings-daemon/xrandr
	$(mkinstalldirs) $(DESTDIR)$(icondir)/16x16/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/22x22/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/24x24/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/32x32/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/scalable/$(context)
	$(INSTALL_DATA) $(srcdir)/msd-xrandr-16.png $(DESTDIR)$(icondir)/16x16/$(context)/msd-xrandr.png
	$(INSTALL_DATA) $(srcdir)/msd-xrandr-22.png $(DESTDIR)$(icondir)/22x22/$(context)/msd-xrandr.png
	$(INSTALL_DATA) $(srcdir)/msd-xrandr-24.png $(DESTDIR)$(icondir)/24x24/$(context)/msd-xrandr.png
	$(INSTALL_DATA) $(srcdir)/msd-xrandr-32.png $(DESTDIR)$(icondir)/32x32/$(context)/msd-xrandr.png
	$(INSTALL_DATA) $(srcdir)/msd-xrandr.svg $(DESTDIR)$(icondir)/scalable/$(context)/msd-xrandr.svg

uninstall-local:
	rm -f $(DESTDIR)$(icondir)/16x16/$(context)/msd-xrandr.png
	rm -f $(DESTDIR)$(icondir)/22x22/$(context)/msd-xrandr.png
	rm -f $(DESTDIR)$(icondir)/24x24/$(context)/msd-xrandr.png
	rm -f $(DESTDIR)$(icondir)/32x32/$(context)/msd-xrandr.png
	rm -f $(DESTDIR)$(icondir)/scalable/$(context)/msd-xrandr.svg

plugin_LTLIBRARIES = \
	libxrandr.la

msd-xrandr-manager-glue.h: msd-xrandr-manager.xml Makefile
	$(AM_V_GEN) dbus-binding-tool --prefix=msd_xrandr_manager --mode=glib-server $< > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $@ || cp xgen-$(@F) $@ ) \
	&& rm -f xgen-$(@F)

libxrandr_la_SOURCES = 		\
	$(BUILT_SOURCES)	\
	msd-xrandr-plugin.h	\
	msd-xrandr-plugin.c	\
	msd-xrandr-manager.h	\
	msd-xrandr-manager.c

libxrandr_la_CPPFLAGS =						\
	-I$(top_srcdir)/mate-settings-daemon			\
	-DBINDIR=\"$(bindir)\"					\
	-DMATE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libxrandr_la_CFLAGS =			\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(LIBNOTIFY_CFLAGS)		\
	$(MATE_DESKTOP_CFLAGS)		\
	$(AM_CFLAGS)

libxrandr_la_LDFLAGS = 			\
	$(MSD_PLUGIN_LDFLAGS)

libxrandr_la_LIBADD  =			\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(LIBNOTIFY_LIBS)		\
	$(MATE_DESKTOP_LIBS)

plugin_in_files =			\
	xrandr.mate-settings-plugin.in

plugin_DATA = $(plugin_in_files:.mate-settings-plugin.in=.mate-settings-plugin)

EXTRA_DIST = $(plugin_in_files) $(ICON_FILES) msd-xrandr-manager.xml
CLEANFILES = $(plugin_DATA) $(BUILT_SOURCES)
DISTCLEANFILES = $(plugin_DATA)

@MSD_INTLTOOL_PLUGIN_RULE@
