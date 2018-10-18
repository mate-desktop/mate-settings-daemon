NULL =

enabled_plugins =	\
	a11y-keyboard	\
	a11y-settings	\
	background	\
	clipboard	\
	datetime	\
	dummy		\
	housekeeping	\
	keybindings	\
	keyboard	\
	media-keys	\
	mpris		\
	mouse		\
	sound		\
	typing-break	\
	xrandr		\
	xrdb		\
	xsettings	\
	$(NULL)

disabled_plugins = $(NULL)

if SMARTCARD_SUPPORT
enabled_plugins += smartcard
else
disabled_plugins += smartcard
endif

if BUILD_RFKILL
enabled_plugins += rfkill
else
disabled_plugins += rfkill
endif

SUBDIRS = common $(enabled_plugins)
DIST_SUBDIRS = $(SUBDIRS) $(disabled_plugins)
