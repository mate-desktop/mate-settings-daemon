plugin_LTLIBRARIES = libhousekeeping.la

libhousekeeping_la_SOURCES = 		\
	msd-ldsm-dialog.c	\
	msd-ldsm-dialog.h	\
	msd-ldsm-trash-empty.c	\
	msd-ldsm-trash-empty.h	\
	msd-disk-space.c		\
	msd-disk-space.h		\
	msd-housekeeping-manager.c	\
	msd-housekeeping-manager.h	\
	msd-housekeeping-plugin.c	\
	msd-housekeeping-plugin.h

libhousekeeping_la_CPPFLAGS = 					\
	-I$(top_srcdir)/mate-settings-daemon			\
	-DMATE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libhousekeeping_la_CFLAGS = 		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(GIOUNIX_CFLAGS)		\
	$(LIBNOTIFY_CFLAGS)		\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

libhousekeeping_la_LDFLAGS = $(MSD_PLUGIN_LDFLAGS)

libhousekeeping_la_LIBADD = $(SETTINGS_PLUGIN_LIBS) $(GIOUNIX_LIBS) $(LIBNOTIFY_LIBS)

plugin_in_files = housekeeping.mate-settings-plugin.desktop.in

plugin_DATA = $(plugin_in_files:.mate-settings-plugin.desktop.in=.mate-settings-plugin)

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA)

DISTCLEANFILES = $(plugin_DATA)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@
