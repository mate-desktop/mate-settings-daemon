dbus_servicesdir = $(datadir)/dbus-1/system-services
dbus_confdir = $(DBUS_SYS_DIR)
polkitdir = $(datadir)/polkit-1/actions

dbus_services_in_files = org.mate.SettingsDaemon.DateTimeMechanism.service.in
polkit_in_files = org.mate.settingsdaemon.datetimemechanism.policy.in

$(BUILT_SOURCES) : $(srcdir)/msd-datetime-mechanism.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--interface-prefix org.mate.SettingsDaemon. \
		--c-namespace MateSettings \
		--generate-c-code msd-datetime-generated $<

if HAVE_POLKIT
libexec_PROGRAMS = msd-datetime-mechanism
endif

nodist_msd_datetime_mechanism_SOURCES =	$(BUILT_SOURCES)
msd_datetime_mechanism_SOURCES =		\
	msd-datetime-mechanism.c		\
	msd-datetime-mechanism.h		\
	msd-datetime-mechanism-main.c	\
	system-timezone.c			\
	system-timezone.h


if HAVE_POLKIT
BUILT_SOURCES = 				\
	msd-datetime-generated.h	\
	msd-datetime-generated.c
endif

AM_CFLAGS = $(WARN_CFLAGS) $(SETTINGS_PLUGIN_CFLAGS) $(POLKIT_CFLAGS)
msd_datetime_mechanism_LDADD = $(POLKIT_LIBS) $(SETTINGS_PLUGIN_LIBS)


if HAVE_POLKIT
dbus_services_DATA = $(dbus_services_in_files:.service.in=.service)

$(dbus_services_DATA): $(dbus_services_in_files)
	$(AM_V_GEN)sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

dbus_conf_DATA = org.mate.SettingsDaemon.DateTimeMechanism.conf

polkit_DATA = $(polkit_in_files:.policy.in=.policy)
$(polkit_DATA): $(polkit_in_files)
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@
else
dbus_services_DATA =
dbus_conf_DATA =
polkit_DATA =
endif

EXTRA_DIST =						\
	$(dbus_services_in_files)			\
	org.mate.SettingsDaemon.DateTimeMechanism.conf	\
	$(polkit_in_files)				\
	msd-datetime-mechanism.xml

CLEANFILES = 		\
	org.mate.SettingsDaemon.DateTimeMechanism.service	\
	org.mate.settingsdaemon.datetimemechanism.policy	\
	$(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
