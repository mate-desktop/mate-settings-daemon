libxrdb = shared_module('xrdb',
  'msd-xrdb-plugin.h',
  'msd-xrdb-plugin.c',
  'msd-xrdb-manager.h',
  'msd-xrdb-manager.c',
  include_directories: [top_inc, msd_daemon_inc],
  dependencies: settings_plugin_deps,
  c_args: [
    '-DSYSCONFDIR="@0@"'.format(msd_prefix / get_option('sysconfdir')),
  ],
  name_suffix: 'so',
  install: true,
  install_dir: msd_plugindir,
)

i18n.merge_file(
  input: 'xrdb.mate-settings-plugin.desktop.in',
  output: 'xrdb.mate-settings-plugin',
  type: 'desktop',
  po_dir: '../../po',
  install: true,
  install_dir: msd_plugindir,
)

subdir('data')
