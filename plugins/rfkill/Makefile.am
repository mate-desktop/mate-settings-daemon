plugin_LTLIBRARIES = librfkill.la

librfkill_la_SOURCES = 		\
	msd-rfkill-plugin.c	\
	msd-rfkill-plugin.h	\
	msd-rfkill-manager.c	\
	msd-rfkill-manager.h	\
	rfkill-glib.c		\
	rfkill-glib.h		\
	rfkill.h

librfkill_la_CPPFLAGS = \
	-I$(top_srcdir)/mate-settings-daemon		\
	-I$(top_srcdir)/data/				\
	-DMATE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" 		\
	$(AM_CPPFLAGS)

librfkill_la_CFLAGS =			\
	$(PLUGIN_CFLAGS)		\
	$(RFKILL_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

librfkill_la_LDFLAGS = 		\
	$(MSD_PLUGIN_LDFLAGS)

librfkill_la_LIBADD  =						\
	$(RFKILL_LIBS)						\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = rfkill.mate-settings-plugin.desktop.in

plugin_DATA = $(plugin_in_files:.mate-settings-plugin.desktop.in=.mate-settings-plugin)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

udevrulesdir = $(prefix)/lib/udev/rules.d
udevrules_DATA = 61-mate-settings-daemon-rfkill.rules

EXTRA_DIST = $(plugin_in_files) $(udevrules_DATA)
CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)
