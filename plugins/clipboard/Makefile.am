NULL =

if BUILD_WAYLAND
WAYLAND_PROTOCOL_XML = $(WAYLAND_PROTOCOLS_DATADIR)/staging/ext-data-control/ext-data-control-v1.xml
WAYLAND_PROTOCOL_GENERATED = \
	ext-data-control-v1-client.c		\
	ext-data-control-v1-client.h		\
	$(NULL)
WAYLAND_CLIPBOARD_SOURCES = \
	msd-clipboard-manager-wayland.c	\
	msd-clipboard-manager-wayland.h	\
	$(NULL)
else
WAYLAND_PROTOCOL_XML =
WAYLAND_PROTOCOL_GENERATED =
WAYLAND_CLIPBOARD_SOURCES =
endif

BUILT_SOURCES = \
	$(WAYLAND_PROTOCOL_GENERATED)	\
	$(NULL)

plugin_LTLIBRARIES = \
	libclipboard.la		\
	$(NULL)

libclipboard_la_SOURCES = 	\
	msd-clipboard-plugin.h	\
	msd-clipboard-plugin.c	\
	msd-clipboard-manager.h	\
	msd-clipboard-manager.c	\
	$(WAYLAND_CLIPBOARD_SOURCES) \
	xutils.h		\
	xutils.c		\
	list.h			\
	list.c			\
	$(WAYLAND_PROTOCOL_GENERATED) \
	$(NULL)

libclipboard_la_CPPFLAGS = \
	-I$(top_srcdir)/mate-settings-daemon		\
	-DMATE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(WAYLAND_CLIENT_CFLAGS) \
	$(AM_CPPFLAGS)

libclipboard_la_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(WAYLAND_CLIENT_CFLAGS)	\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)

libclipboard_la_LDFLAGS = 	\
	$(MSD_PLUGIN_LDFLAGS)	\
	$(NULL)

libclipboard_la_LIBADD  = 	\
	$(SETTINGS_PLUGIN_LIBS)	\
	$(X11_LIBS)		\
	$(XINPUT_LIBS)		\
	$(WAYLAND_CLIENT_LIBS)	\
	$(NULL)

$(WAYLAND_PROTOCOL_GENERATED): $(WAYLAND_PROTOCOL_XML)

ext-data-control-v1-client.h: $(WAYLAND_PROTOCOL_XML)
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header $(WAYLAND_PROTOCOL_XML) $@

ext-data-control-v1-client.c: $(WAYLAND_PROTOCOL_XML)
	$(AM_V_GEN)$(WAYLAND_SCANNER) private-code $(WAYLAND_PROTOCOL_XML) $@

plugin_in_files = 		\
	clipboard.mate-settings-plugin.desktop.in	\
	$(NULL)

plugin_DATA = $(plugin_in_files:.mate-settings-plugin.desktop.in=.mate-settings-plugin)

EXTRA_DIST = 			\
	$(plugin_in_files)	\
	$(NULL)

CLEANFILES = 			\
	$(plugin_DATA)		\
	$(WAYLAND_PROTOCOL_GENERATED)	\
	$(NULL)

DISTCLEANFILES =		\
	$(plugin_DATA)		\
	$(WAYLAND_PROTOCOL_GENERATED)	\
	$(NULL)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

-include $(top_srcdir)/git.mk
