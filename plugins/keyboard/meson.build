install_data(
  'kbd-capslock-off.png',
  'kbd-numlock-off.png',
  'kbd-scrolllock-off.png',
  'kbd-capslock-on.png',
  'kbd-numlock-on.png',
  'kbd-scrolllock-on.png',
  install_dir: join_paths(msd_pkgdatadir, 'icons/hicolor/64x64/devices'),
)

libkeyboard = shared_module('keyboard',
  'msd-keyboard-plugin.h',
  'msd-keyboard-plugin.c',
  'msd-keyboard-manager.h',
  'msd-keyboard-manager.c',
  'msd-keyboard-xkb.h',
  'msd-keyboard-xkb.c',
  'delayed-dialog.h',
  'delayed-dialog.c',
  include_directories: [top_inc, msd_daemon_inc],
  dependencies: settings_plugin_deps + libmatekbdui_deps + [mate_desktop_dep] + x11_libs + [xinput_dep],
  c_args: [
    '-DDATADIR="@0@"'.format(msd_pkgdatadir),
  ],
  name_suffix: 'so',
  install: true,
  install_dir: msd_plugindir,
)

i18n.merge_file(
  input: 'keyboard.mate-settings-plugin.desktop.in',
  output: 'keyboard.mate-settings-plugin',
  type: 'desktop',
  po_dir: '../../po',
  install: true,
  install_dir: msd_plugindir,
)
