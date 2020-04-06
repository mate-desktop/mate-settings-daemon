plugin_LTLIBRARIES = \
	libsmartcard.la

libsmartcard_la_SOURCES = \
	msd-smartcard-plugin.h  \
	msd-smartcard-plugin.c  \
	msd-smartcard.h         \
	msd-smartcard.c         \
	msd-smartcard-manager.h \
	msd-smartcard-manager.c

libsmartcard_la_CPPFLAGS = \
	-I$(top_srcdir)/mate-settings-daemon \
	-DMATE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DMSD_SMARTCARD_MANAGER_NSS_DB=\""$(NSS_DATABASE)"\" \
	$(AM_CPPFLAGS)

libsmartcard_la_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(MATE_CFLAGS)			\
	$(NSS_CFLAGS)			\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

libsmartcard_la_LDFLAGS = \
	$(MSD_PLUGIN_LDFLAGS)

libsmartcard_la_LIBADD = \
	$(SETTINGS_PLUGIN_LIBS)	\
	$(NSS_LIBS)

plugin_in_files = \
	smartcard.mate-settings-plugin.desktop.in

plugin_DATA = $(plugin_in_files:.mate-settings-plugin.desktop.in=.mate-settings-plugin)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = \
	$(plugin_in_files)

CLEANFILES = \
	$(plugin_DATA)

DISTCLEANFILES = \
	$(plugin_DATA)
