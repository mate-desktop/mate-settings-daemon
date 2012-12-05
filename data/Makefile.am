NULL =

gsettings_SCHEMAS = \
	org.mate.applications-at.gschema.xml \
	org.mate.font-rendering.gschema.xml \
	org.mate.keybindings.gschema.xml \
	org.mate.peripherals-smartcard.gschema.xml \
	org.mate.peripherals-touchpad.gschema.xml \
	org.mate.SettingsDaemon.plugins.a11y-keyboard.gschema.xml \
	org.mate.SettingsDaemon.plugins.background.gschema.xml \
	org.mate.SettingsDaemon.plugins.clipboard.gschema.xml \
	org.mate.SettingsDaemon.plugins.datetime.gschema.xml \
	org.mate.SettingsDaemon.plugins.housekeeping.gschema.xml \
	org.mate.SettingsDaemon.plugins.keyboard.gschema.xml \
	org.mate.SettingsDaemon.plugins.media-keys.gschema.xml \
	org.mate.SettingsDaemon.plugins.mouse.gschema.xml \
	org.mate.SettingsDaemon.plugins.smartcard.gschema.xml \
	org.mate.SettingsDaemon.plugins.sound.gschema.xml \
	org.mate.SettingsDaemon.plugins.xrandr.gschema.xml \
	org.mate.SettingsDaemon.plugins.xrdb.gschema.xml \
	org.mate.SettingsDaemon.plugins.xsettings.gschema.xml \
	$(NULL)

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = mate-settings-daemon.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

servicedir = $(datadir)/dbus-1/services
service_in_files = org.mate.SettingsDaemon.service.in
service_DATA = $(service_in_files:.service.in=.service)

org.mate.SettingsDaemon.service: org.mate.SettingsDaemon.service.in Makefile
	@sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = mate-settings-daemon.pc

@INTLTOOL_XML_NOMERGE_RULE@

#disabled until we support it
#xmldir = @MATE_KEYBINDINGS_KEYSDIR@
#xml_in_files = 50-accessibility.xml.in
#xml_DATA = $(xml_in_files:.xml.in=.xml)

EXTRA_DIST = 				\
	$(schemas_in_files)		\
	$(service_in_files)		\
	$(desktop_in_files)		\
	$(xml_in_files)			\
	mate-settings-daemon.pc.in	\
	$(NULL)

DISTCLEANFILES = 			\
	$(schemas_DATA)			\
	$(service_DATA)			\
	$(desktop_DATA)			\
	$(xml_DATA)			\
	$(NULL)

MAINTAINERCLEANFILES =			\
	*~				\
	Makefile.in
