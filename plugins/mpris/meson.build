libmpris = shared_module('mpris',
  'bus-watch-namespace.h',
  'bus-watch-namespace.c',
  'msd-mpris-manager.h',
  'msd-mpris-manager.c',
  'msd-mpris-plugin.h',
  'msd-mpris-plugin.c',
  include_directories: [top_inc, msd_daemon_inc],
  dependencies: settings_plugin_deps,
  name_suffix: 'so',
  install: true,
  install_dir: msd_plugindir,
)

i18n.merge_file(
  input: 'mpris.mate-settings-plugin.desktop.in',
  output: 'mpris.mate-settings-plugin',
  type: 'desktop',
  po_dir: '../../po',
  install: true,
  install_dir: msd_plugindir,
)
