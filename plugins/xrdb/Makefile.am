NULL =

SUBDIRS =		\
	data		\
	$(NULL)

plugin_LTLIBRARIES =	\
	libxrdb.la	\
	$(NULL)

libxrdb_la_SOURCES = 		\
	msd-xrdb-plugin.h	\
	msd-xrdb-plugin.c	\
	msd-xrdb-manager.h	\
	msd-xrdb-manager.c	\
	$(NULL)

libxrdb_la_CPPFLAGS = \
	-I$(top_srcdir)/mate-settings-daemon		\
	-DSYSCONFDIR=\""$(sysconfdir)"\"	\
	-DMATE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libxrdb_la_CFLAGS =			\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(NULL)

libxrdb_la_LDFLAGS = 		\
	$(MSD_PLUGIN_LDFLAGS)	\
	$(NULL)

libxrdb_la_LIBADD  = 		\
	$(SETTINGS_PLUGIN_LIBS)	\
	$(NULL)

plugin_in_files = 		\
	xrdb.mate-settings-plugin.desktop.in	\
	$(NULL)

plugin_DATA = $(plugin_in_files:.mate-settings-plugin.desktop.in=.mate-settings-plugin)

EXTRA_DIST = 			\
	$(plugin_in_files)	\
	$(NULL)

CLEANFILES = 			\
	$(plugin_DATA)		\
	$(NULL)

DISTCLEANFILES =		\
	$(plugin_DATA)		\
	$(NULL)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@
