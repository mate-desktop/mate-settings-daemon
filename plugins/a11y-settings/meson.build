shared_module('a11y-settings',
  'msd-a11y-settings-plugin.h',
  'msd-a11y-settings-plugin.c',
  'msd-a11y-settings-manager.h',
  'msd-a11y-settings-manager.c',
  include_directories: [top_inc, msd_daemon_inc],
  dependencies: settings_plugin_deps,
  name_suffix: 'so',
  install: true,
  install_dir: msd_plugindir,
)

i18n.merge_file(
  input: 'a11y-settings.mate-settings-plugin.desktop.in',
  output: 'a11y-settings.mate-settings-plugin',
  type: 'desktop',
  po_dir: '../../po',
  install: true,
  install_dir: msd_plugindir,
)
