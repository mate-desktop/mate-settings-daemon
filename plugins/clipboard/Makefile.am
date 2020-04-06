NULL =

plugin_LTLIBRARIES = \
	libclipboard.la		\
	$(NULL)

libclipboard_la_SOURCES = 	\
	msd-clipboard-plugin.h	\
	msd-clipboard-plugin.c	\
	msd-clipboard-manager.h	\
	msd-clipboard-manager.c	\
	xutils.h		\
	xutils.c		\
	list.h			\
	list.c			\
	$(NULL)

libclipboard_la_CPPFLAGS = \
	-I$(top_srcdir)/mate-settings-daemon		\
	-DMATE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libclipboard_la_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

libclipboard_la_LDFLAGS = 	\
	$(MSD_PLUGIN_LDFLAGS)	\
	$(NULL)

libclipboard_la_LIBADD  = 	\
	$(SETTINGS_PLUGIN_LIBS)	\
	$(NULL)

plugin_in_files = 		\
	clipboard.mate-settings-plugin.desktop.in	\
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
