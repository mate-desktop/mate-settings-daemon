NULL =

plugin_LTLIBRARIES = \
	libkeyboard.la		\
	$(NULL)

themedir = $(pkgdatadir)/icons/hicolor
size = 64x64
context = devices

iconsdir = $(themedir)/$(size)/$(context)
icons_DATA = \
	  kbd-capslock-off.png kbd-numlock-off.png kbd-scrolllock-off.png \
	  kbd-capslock-on.png kbd-numlock-on.png kbd-scrolllock-on.png

libkeyboard_la_SOURCES = 	\
	msd-keyboard-plugin.h	\
	msd-keyboard-plugin.c	\
	msd-keyboard-manager.h	\
	msd-keyboard-manager.c	\
	msd-keyboard-xkb.h	\
	msd-keyboard-xkb.c	\
	delayed-dialog.h	\
	delayed-dialog.c	\
	$(NULL)

libkeyboard_la_CPPFLAGS = \
	-I$(top_srcdir)/mate-settings-daemon		\
	-DDATADIR=\""$(pkgdatadir)"\"	\
	-DMATE_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libkeyboard_la_CFLAGS =			\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(LIBMATEKBDUI_CFLAGS)		\
	$(AM_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(NULL)

libkeyboard_la_LDFLAGS = 	\
	$(MSD_PLUGIN_LDFLAGS)	\
	$(NULL)

libkeyboard_la_LIBADD  = 	\
	$(SETTINGS_PLUGIN_LIBS)	\
	$(LIBMATEKBDUI_LIBS)	\
	$(NULL)

plugin_in_files = 		\
	keyboard.mate-settings-plugin.desktop.in	\
	$(NULL)

plugin_DATA = $(plugin_in_files:.mate-settings-plugin.desktop.in=.mate-settings-plugin)

EXTRA_DIST = 			\
	$(icons_DATA)		\
	$(plugin_in_files)	\
	$(ui_DATA)		\
	$(NULL)

CLEANFILES = 			\
	$(plugin_DATA)		\
	$(NULL)

DISTCLEANFILES =		\
	$(plugin_DATA)		\
	$(NULL)

$(plugin_DATA): $(plugin_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@
