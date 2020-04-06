plugin_LTLIBRARIES = \
	libsound.la

libsound_la_SOURCES = \
	msd-sound-plugin.h \
	msd-sound-plugin.c \
	msd-sound-manager.h \
	msd-sound-manager.c

libsound_la_CPPFLAGS = \
	-I$(top_srcdir)/mate-settings-daemon \
	-DMATE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libsound_la_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS) \
	$(MATE_CFLAGS) \
	$(PULSE_CFLAGS) \
	$(AM_CFLAGS) \
	$(WARN_CFLAGS)

libsound_la_LDFLAGS = \
	$(MSD_PLUGIN_LDFLAGS)

libsound_la_LIBADD = \
	$(SETTINGS_PLUGIN_LIBS)	\
	$(PULSE_LIBS)

plugin_in_files = \
	sound.mate-settings-plugin.desktop.in

plugin_DATA = $(plugin_in_files:.mate-settings-plugin.desktop.in=.mate-settings-plugin)

EXTRA_DIST = \
	$(plugin_in_files)

CLEANFILES = \
	$(plugin_DATA)

DISTCLEANFILES = \
	$(plugin_DATA)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@
