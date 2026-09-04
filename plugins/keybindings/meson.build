libkeybindings = shared_module('keybindings',
  'dconf-util.h',
  'dconf-util.c',
  'msd-keybindings-plugin.h',
  'msd-keybindings-plugin.c',
  'msd-keybindings-manager.h',
  'msd-keybindings-manager.c',
  include_directories: [top_inc, msd_daemon_inc, include_directories('../common')],
  link_with: libcommon,
  dependencies: settings_plugin_deps + [dconf_dep],
  name_suffix: 'so',
  install: true,
  install_dir: msd_plugindir,
)

i18n.merge_file(
  input: 'keybindings.mate-settings-plugin.desktop.in',
  output: 'keybindings.mate-settings-plugin',
  type: 'desktop',
  po_dir: '../../po',
  install: true,
  install_dir: msd_plugindir,
)
