libmouse = shared_module('mouse',
  'msd-mouse-plugin.h',
  'msd-mouse-plugin.c',
  'msd-mouse-manager.h',
  'msd-mouse-manager.c',
  include_directories: [top_inc, msd_daemon_inc, include_directories('../common')],
  link_with: libcommon,
  dependencies: settings_plugin_deps + x11_libs + [xinput_dep],
  name_suffix: 'so',
  install: true,
  install_dir: msd_plugindir,
)

i18n.merge_file(
  input: 'mouse.mate-settings-plugin.desktop.in',
  output: 'mouse.mate-settings-plugin',
  type: 'desktop',
  po_dir: '../../po',
  install: true,
  install_dir: msd_plugindir,
)

executable('msd-locate-pointer',
  'msd-locate-pointer.h',
  'msd-locate-pointer.c',
  'msd-timeline.h',
  'msd-timeline.c',
  include_directories: [top_inc, msd_daemon_inc],
  dependencies: settings_plugin_deps + x11_libs,
  link_args: ['-lm'],
  install: true,
  install_dir: msd_libexecdir,
)
