NULL =

msd_gschemas_in_in = \
	org.mate.applications-at.gschema.xml.in.in \
	org.mate.font-rendering.gschema.xml.in.in \
	org.mate.peripherals-smartcard.gschema.xml.in.in \
	org.mate.peripherals-touchpad.gschema.xml.in.in \
	org.mate.SettingsDaemon.plugins.a11y-keyboard.gschema.xml.in.in \
	org.mate.SettingsDaemon.plugins.background.gschema.xml.in.in \
	org.mate.SettingsDaemon.plugins.clipboard.gschema.xml.in.in \
	org.mate.SettingsDaemon.plugins.datetime.gschema.xml.in.in \
	org.mate.SettingsDaemon.plugins.housekeeping.gschema.xml.in.in \
	org.mate.SettingsDaemon.plugins.keybindings.gschema.xml.in.in \
	org.mate.SettingsDaemon.plugins.keyboard.gschema.xml.in.in \
	org.mate.SettingsDaemon.plugins.media-keys.gschema.xml.in.in \
	org.mate.SettingsDaemon.plugins.mpris.gschema.xml.in.in \
	org.mate.SettingsDaemon.plugins.mouse.gschema.xml.in.in \
	org.mate.SettingsDaemon.plugins.smartcard.gschema.xml.in.in \
	org.mate.SettingsDaemon.plugins.sound.gschema.xml.in.in \
	org.mate.SettingsDaemon.plugins.typing-break.gschema.xml.in.in \
	org.mate.SettingsDaemon.plugins.xrandr.gschema.xml.in.in \
	org.mate.SettingsDaemon.plugins.xrdb.gschema.xml.in.in \
	org.mate.SettingsDaemon.plugins.xsettings.gschema.xml.in.in \
	$(NULL)

@INTLTOOL_XML_NOMERGE_RULE@

msd_gschemas_in = $(msd_gschemas_in_in:.xml.in.in=.xml.in)
gsettings_SCHEMAS = $(msd_gschemas_in:.xml.in=.xml)

%.gschema.xml.in: %.gschema.xml.in.in Makefile
	$(AM_V_GEN) $(SED) -e 's^\@GETTEXT_PACKAGE\@^$(GETTEXT_PACKAGE)^g' < $< > $@

@GSETTINGS_RULES@

convertdir = $(datadir)/MateConf/gsettings
convert_DATA = mate-settings-daemon.convert

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = mate-settings-daemon.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

servicedir = $(datadir)/dbus-1/services
service_in_files = org.mate.SettingsDaemon.service.in
service_DATA = $(service_in_files:.service.in=.service)

org.mate.SettingsDaemon.service: org.mate.SettingsDaemon.service.in Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = mate-settings-daemon.pc

man_MANS = mate-settings-daemon.1

@INTLTOOL_XML_NOMERGE_RULE@

#disabled until we support it
xmldir = @MATE_KEYBINDINGS_KEYSDIR@
xml_in_files = 50-accessibility.xml.in
xml_DATA = $(xml_in_files:.xml.in=.xml)

EXTRA_DIST = 				\
	$(msd_gschemas_in_in)		\
	$(convert_DATA)			\
	$(service_in_files)		\
	$(desktop_in_files)		\
	$(xml_in_files)			\
	mate-settings-daemon.pc.in	\
	$(man_MANS)				\
	$(NULL)

DISTCLEANFILES = 			\
	$(gsettings_SCHEMAS)			\
	$(msd_gschemas_in)		\
	$(service_DATA)			\
	$(desktop_DATA)			\
	$(xml_DATA)			\
	$(NULL)

MAINTAINERCLEANFILES =			\
	*~				\
	Makefile.in
