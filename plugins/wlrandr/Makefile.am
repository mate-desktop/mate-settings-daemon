icondir = $(datadir)/icons/hicolor
context = apps


ICON_FILES = 			\
	msd-wlrandr-16.png	\
	msd-wlrandr-22.png	\
	msd-wlrandr-24.png	\
	msd-wlrandr-32.png	\
	msd-wlrandr-48.png	\
	msd-wlrandr-64.png	\
	msd-wlrandr-96.png	\
	msd-wlrandr.svg

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(icondir)/16x16/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/22x22/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/24x24/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/32x32/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/48x48/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/64x64/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/96x96/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/scalable/$(context)
	$(INSTALL_DATA) $(srcdir)/msd-wlrandr-16.png $(DESTDIR)$(icondir)/16x16/$(context)/msd-wlrandr.png
	$(INSTALL_DATA) $(srcdir)/msd-wlrandr-22.png $(DESTDIR)$(icondir)/22x22/$(context)/msd-wlrandr.png
	$(INSTALL_DATA) $(srcdir)/msd-wlrandr-24.png $(DESTDIR)$(icondir)/24x24/$(context)/msd-wlrandr.png
	$(INSTALL_DATA) $(srcdir)/msd-wlrandr-32.png $(DESTDIR)$(icondir)/32x32/$(context)/msd-wlrandr.png
	$(INSTALL_DATA) $(srcdir)/msd-wlrandr-48.png $(DESTDIR)$(icondir)/48x48/$(context)/msd-wlrandr.png
	$(INSTALL_DATA) $(srcdir)/msd-wlrandr-64.png $(DESTDIR)$(icondir)/64x64/$(context)/msd-wlrandr.png
	$(INSTALL_DATA) $(srcdir)/msd-wlrandr-96.png $(DESTDIR)$(icondir)/96x96/$(context)/msd-wlrandr.png
	$(INSTALL_DATA) $(srcdir)/msd-wlrandr.svg $(DESTDIR)$(icondir)/scalable/$(context)/msd-wlrandr.svg

uninstall-local:
	rm -f $(DESTDIR)$(icondir)/16x16/$(context)/msd-wlrandr.png
	rm -f $(DESTDIR)$(icondir)/22x22/$(context)/msd-wlrandr.png
	rm -f $(DESTDIR)$(icondir)/24x24/$(context)/msd-wlrandr.png
	rm -f $(DESTDIR)$(icondir)/32x32/$(context)/msd-wlrandr.png
	rm -f $(DESTDIR)$(icondir)/48x48/$(context)/msd-wlrandr.png
	rm -f $(DESTDIR)$(icondir)/64x64/$(context)/msd-wlrandr.png
	rm -f $(DESTDIR)$(icondir)/96x96/$(context)/msd-wlrandr.png
	rm -f $(DESTDIR)$(icondir)/scalable/$(context)/msd-wlrandr.svg

plugin_LTLIBRARIES = \
	libwlrandr.la

protocol_src_dir = protocol
protocol_src = $(protocol_src_dir)/wlr-output-management-unstable-v1.xml

generated_protocol_sources = \
	wlr-output-management-unstable-v1-protocol.c	\
	wlr-output-management-unstable-v1-client-protocol.h

libwlrandr_la_SOURCES = 		\
	msd-wlrandr-plugin.h	\
	msd-wlrandr-plugin.c	\
	msd-wlrandr-manager.h	\
	msd-wlrandr-manager.c	\
	$(generated_protocol_sources)

libwlrandr_la_CPPFLAGS =						\
	-I$(top_srcdir)/mate-settings-daemon			\
	-I$(builddir)						\
	-DBINDIR=\"$(bindir)\"					\
	-DMATE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libwlrandr_la_CFLAGS =			\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(WAYLAND_CLIENT_CFLAGS)	\
	$(APPINDICATOR_CFLAGS)		\
	$(LIBNOTIFY_CFLAGS)		\
	$(MATE_DESKTOP_CFLAGS)		\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

libwlrandr_la_LDFLAGS = 		\
	$(MSD_PLUGIN_LDFLAGS)

libwlrandr_la_LIBADD  =			\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(WAYLAND_CLIENT_LIBS)		\
	$(APPINDICATOR_LIBS)		\
	$(LIBNOTIFY_LIBS)		\
	$(MATE_DESKTOP_LIBS)		\
	$(NULL)

BUILT_SOURCES = $(generated_protocol_sources)

CLEANFILES = $(generated_protocol_sources)

wlr-output-management-unstable-v1-client-protocol.h: $(protocol_src)
	$(AM_V_GEN) $(WAYLAND_SCANNER) client-header $< $@

wlr-output-management-unstable-v1-protocol.c: $(protocol_src)
	$(AM_V_GEN) $(WAYLAND_SCANNER) private-code $< $@

plugin_in_files =			\
	wlrandr.mate-settings-plugin.desktop.in

plugin_DATA = $(plugin_in_files:.mate-settings-plugin.desktop.in=.mate-settings-plugin)

EXTRA_DIST = $(plugin_in_files) $(ICON_FILES) $(protocol_src)
DISTCLEANFILES = $(plugin_DATA)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

-include $(top_srcdir)/git.mk
