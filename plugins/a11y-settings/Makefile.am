plugin_name = ally-settings

plugin_LTLIBRARIES = \
	liba11y-settings.la

liba11y_settings_la_SOURCES = 		\
	msd-a11y-settings-manager.c	\
	msd-a11y-settings-manager.h	\
	msd-a11y-settings-plugin.c	\
	msd-a11y-settings-plugin.h

liba11y_settings_la_CPPFLAGS = \
	-I$(top_srcdir)/mate-settings-daemon		\
	-DMATE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

liba11y_settings_la_CFLAGS =		\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

liba11y_settings_la_LDFLAGS = 		\
	$(MSD_PLUGIN_LDFLAGS)

liba11y_settings_la_LIBADD  = 		\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = 		\
	a11y-settings.mate-settings-plugin.desktop.in

plugin_DATA = $(plugin_in_files:.mate-settings-plugin.desktop.in=.mate-settings-plugin)

EXTRA_DIST = 			\
	$(plugin_in_files)

CLEANFILES = 			\
	$(plugin_DATA)

DISTCLEANFILES =		\
	$(plugin_DATA)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@
