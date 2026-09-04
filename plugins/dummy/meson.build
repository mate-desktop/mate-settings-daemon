libdummy = shared_module('dummy',
  'msd-dummy-plugin.h',
  'msd-dummy-plugin.c',
  'msd-dummy-manager.h',
  'msd-dummy-manager.c',
  include_directories: [top_inc, msd_daemon_inc],
  dependencies: settings_plugin_deps,
  name_suffix: 'so',
  install: false,
  build_by_default: true,
)

i18n.merge_file(
  input: 'dummy.mate-settings-plugin.desktop.in',
  output: 'dummy.mate-settings-plugin',
  type: 'desktop',
  po_dir: '../../po',
)
