liba11y_keyboard_sources = [
  'msd-a11y-keyboard-plugin.h',
  'msd-a11y-keyboard-plugin.c',
  'msd-a11y-keyboard-manager.h',
  'msd-a11y-keyboard-manager.c',
  'msd-a11y-preferences-dialog.h',
  'msd-a11y-preferences-dialog.c',
]

liba11y_keyboard_deps = settings_plugin_deps + [libnotify_dep] + x11_libs + [xinput_dep]

if have_libatspi
  liba11y_keyboard_sources += [
    'msd-a11y-keyboard-atspi.h',
    'msd-a11y-keyboard-atspi.c',
  ]
  liba11y_keyboard_deps += [libatspi_dep]
endif

liba11y_keyboard = shared_module('a11y-keyboard',
  liba11y_keyboard_sources,
  include_directories: [top_inc, msd_daemon_inc],
  dependencies: liba11y_keyboard_deps,
  c_args: [
    '-DGTKBUILDERDIR="@0@"'.format(msd_pkgdatadir),
  ],
  name_suffix: 'so',
  install: true,
  install_dir: msd_plugindir,
)

install_data('msd-a11y-preferences-dialog.ui',
  install_dir: msd_pkgdatadir,
)

i18n.merge_file(
  input: 'a11y-keyboard.mate-settings-plugin.desktop.in',
  output: 'a11y-keyboard.mate-settings-plugin',
  type: 'desktop',
  po_dir: '../../po',
  install: true,
  install_dir: msd_plugindir,
)

test_a11y_preferences_dialog = executable('test-a11y-preferences-dialog',
  'msd-a11y-preferences-dialog.c',
  'msd-a11y-preferences-dialog.h',
  'test-a11y-preferences-dialog.c',
  include_directories: [top_inc, msd_daemon_inc],
  dependencies: settings_daemon_deps + settings_plugin_deps,
  c_args: [
    '-DPIXMAPDIR="@0@"'.format(msd_pkgdatadir),
    '-DGTKBUILDERDIR="@0@"'.format(msd_pkgdatadir),
  ],
)
