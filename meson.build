project(
  'mate-settings-daemon',
  'c',
  version: '1.29.1',
  meson_version: '>= 0.58.0',
  license: 'GPL-2+',
)

msd_version = meson.project_version()

msd_prefix = get_option('prefix')
msd_bindir = join_paths(msd_prefix, get_option('bindir'))
msd_datadir = join_paths(msd_prefix, get_option('datadir'))
msd_includedir = join_paths(msd_prefix, get_option('includedir'))
msd_libdir = join_paths(msd_prefix, get_option('libdir'))
msd_libexecdir = join_paths(msd_prefix, get_option('libexecdir'))
msd_localedir = join_paths(msd_datadir, 'locale')
msd_pkgdatadir = join_paths(msd_datadir, 'mate-settings-daemon')
msd_plugindir = join_paths(msd_libdir, 'mate-settings-daemon')

msd_mate_keybindings_keysdir = join_paths(msd_datadir, 'mate-control-center/keybindings')

cc = meson.get_compiler('c')

# config_h
config_h = configuration_data()
config_h.set_quoted('GETTEXT_PACKAGE', meson.project_name())
config_h.set_quoted('DATADIR', msd_datadir)
config_h.set_quoted('MATE_SETTINGS_LOCALEDIR', msd_localedir)
config_h.set_quoted('LIBEXECDIR', msd_libexecdir)
config_h.set_quoted('MATE_SETTINGS_PLUGINDIR', msd_plugindir)
config_h.set_quoted('PACKAGE', meson.project_name())
config_h.set_quoted('PACKAGE_VERSION', msd_version)

add_project_arguments('-DHAVE_CONFIG_H', language: 'c')
add_project_arguments('-I' + meson.current_build_dir(), language: 'c')

# dependencies
glib_dep = dependency('glib-2.0', version: '>= 2.58.0')
gtk_dep = dependency('gtk+-3.0', version: '>= 3.22.0')
gio_dep = dependency('gio-2.0', version: '>= 2.50.0')
gmodule_dep = dependency('gmodule-2.0')
gthread_dep = dependency('gthread-2.0')
gio_unix_dep = dependency('gio-unix-2.0')
dconf_dep = dependency('dconf', version: '>= 0.13.4')
xinput_dep = dependency('xi')
fontconfig_dep = dependency('fontconfig')
mate_desktop_dep = dependency('mate-desktop-2.0', version: '>= 1.29.0')
libmatekbdui_dep = dependency('libmatekbdui', version: '>= 1.17.0')
libmatekbd_dep = dependency('libmatekbd', version: '>= 1.17.0')
libxklavier_dep = dependency('libxklavier', version: '>= 5.2')

x11_dep = dependency('x11')
x11_libs = [x11_dep]

if cc.has_header('X11/extensions/XKB.h', dependencies: [x11_dep, gtk_dep])
  config_h.set('HAVE_X11_EXTENSIONS_XKB_H', 1)
endif

settings_daemon_deps = [
  glib_dep,
  gtk_dep,
  gio_dep,
  gmodule_dep,
  gthread_dep,
]

settings_plugin_deps = [
  gtk_dep,
  gio_dep,
]

libmatekbdui_deps = [
  libmatekbdui_dep,
  libmatekbd_dep,
  libxklavier_dep,
]

glib_genmarshal = find_program('glib-genmarshal')
gdbus_codegen = find_program('gdbus-codegen')

# i18n
i18n = import('i18n')

# wayland support
wayland_opt = get_option('wayland')
wayland_client_dep = dependency('wayland-client', version: '>= 1.18', required: wayland_opt)
gtk_layer_shell_dep = dependency('gtk-layer-shell-0', version: '>= 0.6', required: wayland_opt)
appindicator_dep = dependency('ayatana-appindicator3-0.1', required: wayland_opt)

have_wayland = wayland_client_dep.found() and gtk_layer_shell_dep.found() and appindicator_dep.found()

if not have_wayland and wayland_opt.enabled()
  error('Wayland support requested but the required Wayland libraries (wayland-client, gtk-layer-shell or ayatana-appindicator) are missing')
endif

if have_wayland
  wayland_scanner = find_program('wayland-scanner')
  wayland_protocols_dep = dependency('wayland-protocols', required: false)
  wayland_protocols_datadir = wayland_protocols_dep.get_variable(pkgconfig: 'pkgdatadir')
  if wayland_protocols_datadir.startswith('//')
    wayland_protocols_datadir = wayland_protocols_datadir.substring(1)
  endif
  if wayland_protocols_datadir == ''
    error('wayland-protocols not found')
  endif
  config_h.set('HAVE_WAYLAND', 1)
endif

# libnotify
libnotify_opt = get_option('libnotify')
libnotify_dep = dependency('libnotify', version: '>= 0.7.0', required: libnotify_opt)
have_libnotify = libnotify_dep.found()
if have_libnotify
  config_h.set('HAVE_LIBNOTIFY', 1)
endif

# libatspi
libatspi_opt = get_option('libatspi')
libatspi_dep = dependency('atspi-2', version: '>= 2.0', required: libatspi_opt)
have_libatspi = libatspi_dep.found()
if have_libatspi
  config_h.set('HAVE_LIBATSPI', 1)
endif
atspi_2_36 = dependency('atspi-2', version: '> 2.36.0', required: false)
if atspi_2_36.found()
  config_h.set('DESTROYING_ATSPI_LISTENER_DOES_NOT_CRASH', 1)
endif

# PulseAudio
pulse_opt = get_option('pulse')
have_pulse = false
pulse_dep = []
if pulse_opt
  pulse_dep = dependency('libpulse', version: '>= 0.9.16', required: false)
  have_pulse = pulse_dep.found()
  if have_pulse
    config_h.set('HAVE_PULSE', 1)
  endif
endif

# libcanberra
libcanberra_dep = dependency('libcanberra-gtk3', required: false)
have_libcanberra = libcanberra_dep.found()
if have_libcanberra
  config_h.set('HAVE_LIBCANBERRA', 1)
endif

# libmatemixer
libmatemixer_dep = dependency('libmatemixer', version: '>= 1.10.0', required: false)
have_libmatemixer = libmatemixer_dep.found()
if have_libmatemixer
  config_h.set('HAVE_LIBMATEMIXER', 1)
endif

# smartcard support (NSS)
smartcard_opt = get_option('smartcard')
have_smartcard_support = false
nss_dep = []
if smartcard_opt
  nss_dep = dependency('nss', version: '>= 3.11.2', required: false)
  have_smartcard_support = nss_dep.found()
  if have_smartcard_support
    config_h.set('SMARTCARD_SUPPORT', 1)
  endif
endif

# nssdb path
nssdb_opt = get_option('nssdb')
if have_smartcard_support
  if nssdb_opt != ''
    nss_database = nssdb_opt
  else
    nss_database = join_paths(msd_prefix, get_option('sysconfdir'), 'pki/nssdb')
  endif
elif nssdb_opt != ''
  warning('nssdb specified when smartcard support is disabled')
  nss_database = nssdb_opt
else
  nss_database = ''
endif

# PolicyKit
polkit_opt = get_option('polkit')
polkit_dep = dependency('polkit-gobject-1', version: '>= 0.97', required: polkit_opt)
have_polkit = polkit_dep.found()
if have_polkit
  config_h.set('HAVE_POLKIT', 1)
endif

# rfkill
rfkill_opt = get_option('rfkill')
have_rfkill = rfkill_opt
if have_rfkill
  if not cc.has_header('linux/rfkill.h')
    error('RFKill headers not found but rfkill support requested')
  endif
endif

# profiling
profiling_opt = get_option('profiling')
if profiling_opt
  config_h.set('ENABLE_PROFILING', 1)
endif

# dbus dirs
dbus_sys_dir_opt = get_option('dbus_sys_dir')
if dbus_sys_dir_opt != ''
  dbus_sys_dir = dbus_sys_dir_opt
else
  dbus_sys_dir = join_paths(msd_datadir, 'dbus-1/system.d')
endif

dbus_services_dir_opt = get_option('dbus_services_dir')
if dbus_services_dir_opt != ''
  dbus_services_dir = dbus_services_dir_opt
else
  dbus_services_dir = join_paths(msd_datadir, 'dbus-1/services')
endif

# top-level include directory
top_inc = include_directories('.')
msd_daemon_inc = include_directories('mate-settings-daemon')

subdir('po')
subdir('mate-settings-daemon')
subdir('plugins')
subdir('data')

configure_file(output: 'config.h', configuration: config_h)

summary({
  'Wayland support': have_wayland,
  'PulseAudio support': have_pulse,
  'libnotify support': have_libnotify,
  'libatspi support': have_libatspi,
  'libcanberra support': have_libcanberra,
  'libmatemixer support': have_libmatemixer,
  'Smartcard support': have_smartcard_support,
  'RFKill support': have_rfkill,
  'PolicyKit support': have_polkit,
  'D-Bus system.d dir': dbus_sys_dir,
  'NSS database': nss_database,
  'Profiling support': profiling_opt,
}, section: 'Configuration')
