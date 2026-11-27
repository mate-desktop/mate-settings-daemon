libtyping_break = shared_module('typing-break',
  'msd-typing-break-plugin.h',
  'msd-typing-break-plugin.c',
  'msd-typing-break-manager.h',
  'msd-typing-break-manager.c',
  include_directories: [top_inc, msd_daemon_inc],
  dependencies: settings_plugin_deps,
  name_suffix: 'so',
  install: true,
  install_dir: msd_plugindir,
)

i18n.merge_file(
  input: 'typing-break.mate-settings-plugin.desktop.in',
  output: 'typing-break.mate-settings-plugin',
  type: 'desktop',
  po_dir: '../../po',
  install: true,
  install_dir: msd_plugindir,
)
