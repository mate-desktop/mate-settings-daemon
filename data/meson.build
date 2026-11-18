msd_gschemas_in = [
  'org.mate.applications-at.gschema.xml.in',
  'org.mate.font-rendering.gschema.xml.in',
  'org.mate.peripherals-keyboard.gschema.xml.in',
  'org.mate.peripherals-mouse.gschema.xml.in',
  'org.mate.peripherals-smartcard.gschema.xml.in',
  'org.mate.peripherals-touchpad.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.a11y-keyboard.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.a11y-settings.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.background.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.clipboard.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.datetime.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.housekeeping.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.keybindings.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.keyboard.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.media-keys.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.mpris.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.mouse.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.rfkill.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.smartcard.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.sound.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.typing-break.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.xrandr.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.xrdb.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.xsettings.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.wlrandr.gschema.xml.in',
  'org.mate.SettingsDaemon.plugins.gschema.xml.in',
]

msd_gschemas = []

foreach schema_in : msd_gschemas_in
  schema = schema_in.replace('.xml.in', '.xml')
  msd_gschemas += configure_file(
    input: schema_in,
    output: schema,
    configuration: {
      'GETTEXT_PACKAGE': meson.project_name(),
    },
    install: true,
    install_dir: join_paths(msd_datadir, 'glib-2.0/schemas'),
  )
endforeach

glib_compile_schemas = find_program('glib-compile-schemas')

# Compile the gschemas from the build directory, where the generated
# .gschema.xml files produced by configure_file() above actually live.
gschemas_compiled = custom_target('gschemas.compiled',
  output: 'gschemas.compiled',
  input: msd_gschemas,
  command: [glib_compile_schemas, '--targetdir', meson.current_build_dir(), meson.current_build_dir()],
  build_by_default: true,
)

mate_settings_daemon_service = configure_file(
  input: 'org.mate.SettingsDaemon.service.in',
  output: 'org.mate.SettingsDaemon.service',
  configuration: {
    'libexecdir': msd_libexecdir,
  },
)

install_data(mate_settings_daemon_service,
  install_dir: dbus_services_dir,
)

mate_settings_daemon_desktop = configure_file(
  input: 'mate-settings-daemon.desktop.in.in',
  output: 'mate-settings-daemon.desktop.in',
  configuration: {
    'LIBEXECDIR': msd_libexecdir,
  },
)

i18n.merge_file(
  input: mate_settings_daemon_desktop,
  output: 'mate-settings-daemon.desktop',
  type: 'desktop',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(msd_prefix, get_option('sysconfdir'), 'xdg/autostart'),
)

pkgconfig = import('pkgconfig')

pkgconfig.generate(
  name: 'mate-settings-daemon',
  description: 'Utility library for accessing mate-settings-daemon over DBUS',
  version: msd_version,
  requires: ['glib-2.0', 'dbus-1'],
  install_dir: join_paths(msd_libdir, 'pkgconfig'),
  variables: [
    'libexecdir=' + msd_libexecdir,
    'plugindir=' + msd_plugindir,
    'binary=' + join_paths(msd_libexecdir, 'mate-settings-daemon'),
  ],
)

install_data(
  'mate-settings-daemon.1',
  'msd-datetime-mechanism.1',
  'msd-locate-pointer.1',
  install_dir: join_paths(msd_datadir, 'man/man1'),
)

msd_msgfmt = find_program('msgfmt')

accessibility_xml = custom_target('50-accessibility.xml',
  input: '50-accessibility.xml.in',
  output: '50-accessibility.xml',
  command: [
    msd_msgfmt,
    '--xml',
    '--template', '@INPUT@',
    '-d', join_paths(meson.project_source_root(), 'po'),
    '-o', '@OUTPUT@',
  ],
  env: ['GETTEXTDATADIR=' + meson.project_source_root()],
  install: true,
  install_dir: msd_mate_keybindings_keysdir,
)
