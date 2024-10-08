AC_PREREQ([2.60])

m4_define([msd_api_version_major],[1])
m4_define([msd_api_version_minor],[28])
m4_define([msd_api_version_micro],[0])
m4_define([msd_api_version],
	[msd_api_version_major.msd_api_version_minor.msd_api_version_micro])

AC_INIT([mate-settings-daemon],
	[msd_api_version_major.msd_api_version_minor.msd_api_version_micro],
	[https://mate-desktop.org/])

AC_CONFIG_SRCDIR([mate-settings-daemon/mate-settings-manager.c])
AC_CONFIG_MACRO_DIR([m4])

MATE_DEBUG_CHECK
MATE_COMPILE_WARNINGS

AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-xz tar-ustar check-news])
AM_MAINTAINER_MODE([enable])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_STDC_HEADERS
AC_PROG_CXX
AM_PROG_CC_C_O
AC_PROG_LIBTOOL

AC_HEADER_STDC

AC_SUBST(VERSION)

AC_CONFIG_HEADERS([config.h])

AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Name of default gettext domain])

AS_AC_EXPAND(LIBEXECDIR, "$libexecdir")

MSD_INTLTOOL_PLUGIN_RULE='%.mate-settings-plugin:   %.mate-settings-plugin.in   $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< [$]@'
AC_SUBST([MSD_INTLTOOL_PLUGIN_RULE])

dnl Unconditionally use this dir to avoid a circular dep with matecc
MATE_KEYBINDINGS_KEYSDIR="${datadir}/mate-control-center/keybindings"
AC_SUBST(MATE_KEYBINDINGS_KEYSDIR)

dnl ---------------------------------------------------------------------------
dnl - Dependencies
dnl ---------------------------------------------------------------------------

GLIB_REQUIRED_VERSION=2.58.0
GIO_REQUIRED_VERSION=2.50.0
GTK_REQUIRED_VERSION=3.22.0
MATE_DESKTOP_REQUIRED_VERSION=1.27.1
LIBMATEKBD_REQUIRED_VERSION=1.17.0
LIBNOTIFY_REQUIRED_VERSION=0.7.0
LIBATSPI_REQUIRED_VERSION=2.0
LIBMATEMIXER_REQUIRED_VERSION=1.10.0

PKG_CHECK_MODULES(SETTINGS_DAEMON,
	glib-2.0 >= $GLIB_REQUIRED_VERSION
	gtk+-3.0 >= $GTK_REQUIRED_VERSION
	gio-2.0 >= $GIO_REQUIRED_VERSION
	gmodule-2.0
	gthread-2.0
)

PKG_CHECK_MODULES(SETTINGS_PLUGIN,
	gtk+-3.0 >= $GTK_REQUIRED_VERSION
	gio-2.0 >= $GIO_REQUIRED_VERSION
)

MSD_PLUGIN_LDFLAGS="-export_dynamic -module -avoid-version"
case $host_os in
  darwin*)
    MSD_PLUGIN_LDFLAGS="${MSD_PLUGIN_LDFLAGS} -Wl,-bundle_loader,\$(top_builddir)/mate-settings-daemon/mate-settings-daemon"
    ;;
esac
AC_SUBST([MSD_PLUGIN_LDFLAGS])

PKG_CHECK_MODULES(DCONF, dconf >= 0.13.4)

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)
AC_PATH_PROG(GDBUS_CODEGEN, gdbus-codegen)

GLIB_GSETTINGS

dnl ---------------------------------------------------------------------------
dnl - Check for mate-desktop
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(MATE_DESKTOP, mate-desktop-2.0 >= $MATE_DESKTOP_REQUIRED_VERSION)

dnl ---------------------------------------------------------------------------
dnl - Check for libnotify
dnl ---------------------------------------------------------------------------

have_libnotify=no
AC_ARG_WITH(libnotify,
	AS_HELP_STRING([--without-libnotify], [Disable notifications (default: auto)]),
	with_libnotify=$withval, with_libnotify=auto)

if test "x$with_libnotify" != "xno"; then
	PKG_CHECK_MODULES(LIBNOTIFY, libnotify >= $LIBNOTIFY_REQUIRED_VERSION,
			  [AC_DEFINE(HAVE_LIBNOTIFY, 1, [Define if libnotify is available])
			  have_libnotify=yes], have_libnotify=no)
	if test "x$have_libnotify" = xno -a "x$with_libnotify" = xyes; then
		AC_MSG_ERROR([libnotify support requested but libraries not found])
	fi
fi
AC_SUBST(LIBNOTIFY_CFLAGS)
AC_SUBST(LIBNOTIFY_LIBS)

dnl ---------------------------------------------------------------------------
dnl - Check for libatspi
dnl ---------------------------------------------------------------------------

AC_ARG_WITH([libatspi],
            [AS_HELP_STRING([--without-libatspi], [Disable key monitoring support (default: auto)])],
            [with_libatspi=$withval], [with_libatspi=auto])

have_libatspi=no
AS_IF([test "x$with_libatspi" != xno],
      [PKG_CHECK_MODULES([LIBATSPI], [atspi-2 >= $LIBATSPI_REQUIRED_VERSION],
                         [AC_DEFINE([HAVE_LIBATSPI], [1], [Define if libatspi is available])
                          have_libatspi=yes], [have_libatspi=no])
       AS_IF([test "x$have_libatspi" = xno -a "x$with_libatspi" = xyes],
             [AC_MSG_ERROR([libatspi support requested but libraries not found])])
       PKG_CHECK_EXISTS([atspi-2 > 2.36.0],
                        [AC_DEFINE([DESTROYING_ATSPI_LISTENER_DOES_NOT_CRASH], [1], [Define if libatspi does not have bug 22])],
                        [])])
AM_CONDITIONAL([HAVE_LIBATSPI], [test "x$have_libatspi" = xyes])

dnl ---------------------------------------------------------------------------
dnl - Check for D-Bus
dnl ---------------------------------------------------------------------------

dnl - Are we specifying a different dbus root ?
AC_ARG_WITH(dbus-sys,
        [AS_HELP_STRING([--with-dbus-sys=<dir>],
        [where D-BUS system.d directory is])])
AC_ARG_WITH(dbus-services,
        [AS_HELP_STRING([--with-dbus-services=<dir>],
        [where D-BUS services directory is])])
if ! test -z "$with_dbus_sys" ; then
        DBUS_SYS_DIR="$with_dbus_sys"
else
        DBUS_SYS_DIR='${datadir}/dbus-1/system.d'
fi
AC_SUBST(DBUS_SYS_DIR)

dnl ---------------------------------------------------------------------------
dnl - X11 stuff
dnl ---------------------------------------------------------------------------

# Like AC_CHECK_HEADERS, but it uses the already-computed -I directories.
#
AC_DEFUN([AC_CHECK_X_HEADERS], [
  ac_save_CPPFLAGS="$CPPFLAGS"
  if test \! -z "$includedir" ; then
    CPPFLAGS="$CPPFLAGS -I$includedir"
  fi
  CPPFLAGS="$CPPFLAGS $X_CFLAGS"
  AC_CHECK_HEADERS([$1],[$2],[$3],[$4])
  CPPFLAGS="$ac_save_CPPFLAGS"])

# Like AC_CHECK_LIB, but it uses the already-computed -I and -L directories.
# Use this sparingly; it probably doesn't work very well on X programs.
#
AC_DEFUN([AC_CHECK_X_LIB], [
  ac_save_CPPFLAGS="$CPPFLAGS"
  ac_save_LDFLAGS="$LDFLAGS"
#  ac_save_LIBS="$LIBS"

  if test \! -z "$includedir" ; then
    CPPFLAGS="$CPPFLAGS -I$includedir"
  fi
  # note: $X_CFLAGS includes $x_includes
  CPPFLAGS="$CPPFLAGS $X_CFLAGS"

  if test \! -z "$libdir" ; then
    LDFLAGS="$LDFLAGS -L$libdir"
  fi
  # note: $X_LIBS includes $x_libraries
  LDFLAGS="$LDFLAGS $ALL_X_LIBS"

  AC_CHECK_LIB([$1], [$2], [$3], [$4], [$5])
  CPPFLAGS="$ac_save_CPPFLAGS"
  LDFLAGS="$ac_save_LDFLAGS"
#  LIBS="$ac_save_LIBS"
  ])

AC_PATH_XTRA

ALL_X_LIBS="$X_LIBS $X_PRE_LIBS -lXext -lX11 $X_EXTRA_LIBS"

X11_LIBS="$ALL_X_LIBS"
AC_SUBST(X11_LIBS)

dnl ---------------------------------------------------------------------------
dnl - XInput
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(XINPUT, xi)

dnl ---------------------------------------------------------------------------
dnl - Fontconfig
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(FONTCONFIG, fontconfig)

dnl ---------------------------------------------------------------------------
dnl - Keyboard plugin stuff
dnl ---------------------------------------------------------------------------

AC_CHECK_X_HEADERS([X11/extensions/XKB.h])

PKG_CHECK_MODULES(LIBMATEKBDUI, [libmatekbdui >= $LIBMATEKBD_REQUIRED_VERSION libmatekbd >= $LIBMATEKBD_REQUIRED_VERSION libxklavier >= 5.2])
AC_SUBST(LIBMATEKBDUI_CFLAGS)
AC_SUBST(LIBMATEKBDUI_LIBS)

dnl ---------------------------------------------------------------------------
dnl - Check for sound & mixer libraries
dnl ---------------------------------------------------------------------------

have_pulse=false
AC_ARG_ENABLE(pulse,
  AS_HELP_STRING([--disable-pulse], [Disable PulseAudio support]),
       [case "${enableval}" in
               yes) WANT_PULSE=yes ;;
               no)  WANT_PULSE=no ;;
               *) AC_MSG_ERROR(bad value ${enableval} for --disable-pulse) ;;
       esac],
       [WANT_PULSE=no]) dnl Default value

if test x$WANT_PULSE = xyes ; then
       PA_REQUIRED_VERSION=0.9.16
       PKG_CHECK_MODULES(PULSE, libpulse >= $PA_REQUIRED_VERSION,
             [have_pulse=true
              AC_DEFINE(HAVE_PULSE, 1, [Define if PulseAudio support is available])],
             [have_pulse=false])
fi
AM_CONDITIONAL(HAVE_PULSE, test "x$have_pulse" = "xtrue")

AC_SUBST(PULSE_CFLAGS)
AC_SUBST(PULSE_LIBS)

have_libcanberra=no
AC_ARG_WITH(libcanberra,
    AS_HELP_STRING([--without-libcanberra], [Disable sound events (default: auto)]),
    with_libcanberra=$withval, with_libcanberra=auto)

if test "x$with_libcanberra" != "xno"; then
    PKG_CHECK_MODULES(LIBCANBERRA, libcanberra-gtk3,
              [AC_DEFINE(HAVE_LIBCANBERRA, 1, [Define if libcanberra-gtk3 is available])
              have_libcanberra=yes], have_libcanberra=no)
    if test "x$have_libcanberra" = xno -a "x$with_libcanberra" = xyes; then
        AC_MSG_ERROR([libcanberra support requested but library not found])
    fi
fi
AC_SUBST(LIBCANBERRA_CFLAGS)
AC_SUBST(LIBCANBERRA_LIBS)

have_libmatemixer=no
AC_ARG_WITH(libmatemixer,
    AS_HELP_STRING([--without-libmatemixer], [Disable volume event handling (default: auto)]),
    with_libmatemixer=$withval, with_libmatemixer=auto)

if test "x$with_libmatemixer" != "xno"; then
    PKG_CHECK_MODULES(LIBMATEMIXER, libmatemixer >= $LIBMATEMIXER_REQUIRED_VERSION,
              [AC_DEFINE(HAVE_LIBMATEMIXER, 1, [Define if libmatemixer is available])
              have_libmatemixer=yes], have_libmatemixer=no)
    if test "x$have_libmatemixer" = xno -a "x$with_libmatemixer" = xyes; then
        AC_MSG_ERROR([libmatemixer support requested but library not found])
    fi
fi
AC_SUBST(LIBMATEMIXER_CFLAGS)
AC_SUBST(LIBMATEMIXER_LIBS)

dnl ---------------------------------------------------------------------------
dnl - Housekeeping plugin stuff
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(GIOUNIX, [gio-unix-2.0])
AC_SUBST(GIOUNIX_CFLAGS)
AC_SUBST(GIOUNIX_LIBS)

dnl ==============================================
dnl smartcard section
dnl ==============================================
have_smartcard_support=false
AC_ARG_ENABLE(smartcard-support,
  AS_HELP_STRING([--disable-smartcard-support],
                 [turn off smartcard support]),
       [case "${enableval}" in
               yes) WANT_SMARTCARD_SUPPORT=yes ;;
               no)  WANT_SMARTCARD_SUPPORT=no ;;
               *) AC_MSG_ERROR(bad value ${enableval} for --disable-smartcard-support) ;;
       esac],
       [WANT_SMARTCARD_SUPPORT=yes])

if test x$WANT_SMARTCARD_SUPPORT = xyes ; then
       NSS_REQUIRED_VERSION=3.11.2
       PKG_CHECK_MODULES(NSS, nss >= $NSS_REQUIRED_VERSION,
             [have_smartcard_support=true
              AC_DEFINE(SMARTCARD_SUPPORT, 1, [Define if smartcard support should be enabled])],
             [have_smartcard_support=false])
fi
AM_CONDITIONAL(SMARTCARD_SUPPORT, test "x$have_smartcard_support" = "xtrue")

AC_SUBST(NSS_CFLAGS)
AC_SUBST(NSS_LIBS)

AC_ARG_WITH(nssdb,
  AS_HELP_STRING([--with-nssdb],
                 [where system NSS database is]))

NSS_DATABASE=""
if test "x$have_smartcard_support" = "xtrue"; then
        if ! test -z "$with_nssdb" ; then
                NSS_DATABASE="$with_nssdb"
        else
                NSS_DATABASE="${sysconfdir}/pki/nssdb"
        fi
else
        if ! test -z "$with_nssdb" ; then
                AC_MSG_WARN([nssdb specified when smartcard support is disabled])
        fi
fi

AC_SUBST(NSS_DATABASE)

# ---------------------------------------------------------------------------
# PolicyKit
# ---------------------------------------------------------------------------

POLKIT_REQUIRED=0.97

# PolicyKit detection; defaults to 'auto' (use it if it's available)
#
POLKIT_CFLAGS=
POLKIT_LIBS=
AC_ARG_ENABLE(polkit,
              AS_HELP_STRING([--enable-polkit],
                             [Enable PolicyKit support (auto)]),
              enable_polkit=$enableval,
              enable_polkit=auto)

if test "x$enable_polkit" = "xno" ; then
  HAVE_POLKIT=no
else
  HAVE_POLKIT=no
  PKG_CHECK_MODULES(POLKIT, polkit-gobject-1 >= $POLKIT_REQUIRED, HAVE_POLKIT=yes, HAVE_POLKIT=no)

  if test "x$enable_polkit" = "xyes" -a "x$HAVE_POLKIT" = "xno" ; then
    AC_MSG_ERROR(PolicyKit support explicity enabled but not available)
  fi

  if test "x$HAVE_POLKIT" = "xyes" ; then
    AC_DEFINE(HAVE_POLKIT, 1, [Defined if PolicyKit support is enabled])
  fi
fi
AM_CONDITIONAL(HAVE_POLKIT, test "x$HAVE_POLKIT" = "xyes")
AC_SUBST(POLKIT_CFLAGS)
AC_SUBST(POLKIT_LIBS)

# ---------------------------------------------------------------------------
# Rfkill
# ---------------------------------------------------------------------------

AC_ARG_ENABLE(rfkill,
              AS_HELP_STRING([--disable-rfkill], [disable rfkill support (default: enabled)]),,
              enable_rfkill=yes, enabled_rfkill=no)

if test x"$enable_rfkill" != x"no" ; then
  AC_CHECK_HEADERS([linux/rfkill.h],,
                   AC_MSG_ERROR([RFKill headers not found but rfkill support requested]))
fi

AM_CONDITIONAL(BUILD_RFKILL, [test x"$enable_rfkill" = x"yes"])

# ---------------------------------------------------------------------------
# Enable Profiling
# ---------------------------------------------------------------------------
AC_ARG_ENABLE(profiling,
	[AS_HELP_STRING([--enable-profiling],
	[turn on profiling])],
	, enable_profiling=no)
if test "x$enable_profiling" = "xyes"; then
    AC_DEFINE(ENABLE_PROFILING,1,[enable profiling])
fi

# ---------------------------------------------------------------------------
# Plugins
# ---------------------------------------------------------------------------

plugindir='$(libdir)/mate-settings-daemon'
AC_SUBST([plugindir])

dnl ---------------------------------------------------------------------------
dnl - Finish
dnl ---------------------------------------------------------------------------

AC_CONFIG_FILES([
Makefile
mate-settings-daemon/Makefile
plugins/Makefile
plugins/a11y-keyboard/Makefile
plugins/a11y-settings/Makefile
plugins/background/Makefile
plugins/clipboard/Makefile
plugins/common/Makefile
plugins/datetime/Makefile
plugins/dummy/Makefile
plugins/housekeeping/Makefile
plugins/keybindings/Makefile
plugins/keyboard/Makefile
plugins/media-keys/Makefile
plugins/mpris/Makefile
plugins/mouse/Makefile
plugins/rfkill/Makefile
plugins/smartcard/Makefile
plugins/sound/Makefile
plugins/typing-break/Makefile
plugins/xrandr/Makefile
plugins/xrdb/Makefile
plugins/xrdb/data/Makefile
plugins/xsettings/Makefile
data/Makefile
data/mate-settings-daemon.desktop.in
data/mate-settings-daemon.pc
data/mate-settings-daemon-uninstalled.pc
data/org.mate.applications-at.gschema.xml
data/org.mate.font-rendering.gschema.xml
data/org.mate.peripherals-keyboard.gschema.xml
data/org.mate.peripherals-mouse.gschema.xml
data/org.mate.peripherals-smartcard.gschema.xml
data/org.mate.peripherals-touchpad.gschema.xml
data/org.mate.SettingsDaemon.plugins.a11y-keyboard.gschema.xml
data/org.mate.SettingsDaemon.plugins.a11y-settings.gschema.xml
data/org.mate.SettingsDaemon.plugins.background.gschema.xml
data/org.mate.SettingsDaemon.plugins.clipboard.gschema.xml
data/org.mate.SettingsDaemon.plugins.datetime.gschema.xml
data/org.mate.SettingsDaemon.plugins.housekeeping.gschema.xml
data/org.mate.SettingsDaemon.plugins.keybindings.gschema.xml
data/org.mate.SettingsDaemon.plugins.keyboard.gschema.xml
data/org.mate.SettingsDaemon.plugins.media-keys.gschema.xml
data/org.mate.SettingsDaemon.plugins.mpris.gschema.xml
data/org.mate.SettingsDaemon.plugins.mouse.gschema.xml
data/org.mate.SettingsDaemon.plugins.rfkill.gschema.xml
data/org.mate.SettingsDaemon.plugins.smartcard.gschema.xml
data/org.mate.SettingsDaemon.plugins.sound.gschema.xml
data/org.mate.SettingsDaemon.plugins.typing-break.gschema.xml
data/org.mate.SettingsDaemon.plugins.xrandr.gschema.xml
data/org.mate.SettingsDaemon.plugins.xrdb.gschema.xml
data/org.mate.SettingsDaemon.plugins.xsettings.gschema.xml
data/org.mate.SettingsDaemon.plugins.gschema.xml
po/Makefile.in
])
AC_OUTPUT

dnl ---------------------------------------------------------------------------
dnl - Show summary
dnl ---------------------------------------------------------------------------

echo "
Configure summary:

    ${PACKAGE_STRING}
    `echo $PACKAGE_STRING | sed "s/./=/g"`

    prefix ..................: ${prefix}
    exec_prefix .............: ${exec_prefix}
    libdir ..................: ${libdir}
    bindir ..................: ${bindir}
    sbindir .................: ${sbindir}
    sysconfdir...............: ${sysconfdir}
    sysconfsubdir ...........: ${sysconfsubdir}
    localstatedir............: ${localstatedir}
    plugindir ...............: ${plugindir}
    datadir..................: ${datadir}
    source code location ....: ${srcdir}
    compiler.................: ${CC}
    cflags ..................: ${CFLAGS}
    warning cflags ..........: ${WARN_CFLAGS}
    Maintainer mode .........: ${USE_MAINTAINER_MODE}

    dbus-1 system.d dir .....: ${DBUS_SYS_DIR}
    PolicyKit support .......: ${HAVE_POLKIT}

    PulseAudio support ......: ${have_pulse}
    Libnotify support .......: ${have_libnotify}
    Libatspi support ........: ${have_libatspi}
    Libcanberra support .....: ${have_libcanberra}
    Libmatemixer support ....: ${have_libmatemixer}
    Smartcard support .......: ${have_smartcard_support}
    RFKill support ..........: ${enable_rfkill}
${NSS_DATABASE:+\
    System nssdb ............: ${NSS_DATABASE}
}\
    Profiling support .......: ${enable_profiling}
"
