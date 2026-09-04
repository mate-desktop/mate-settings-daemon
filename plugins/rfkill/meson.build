librfkill = shared_module('rfkill',
  'msd-rfkill-plugin.c',
  'msd-rfkill-plugin.h',
  'msd-rfkill-manager.c',
  'msd-rfkill-manager.h',
  'rfkill-glib.c',
  'rfkill-glib.h',
  'rfkill.h',
  include_directories: [top_inc, msd_daemon_inc],
  dependencies: settings_plugin_deps,
  c_args: [
    '-DLIBEXECDIR="@0@"'.format(msd_libexecdir),
  ],
  name_suffix: 'so',
  install: true,
  install_dir: msd_plugindir,
)

i18n.merge_file(
  input: 'rfkill.mate-settings-plugin.desktop.in',
  output: 'rfkill.mate-settings-plugin',
  type: 'desktop',
  po_dir: '../../po',
  install: true,
  install_dir: msd_plugindir,
)

install_data('61-mate-settings-daemon-rfkill.rules',
  install_dir: join_paths(msd_prefix, 'lib/udev/rules.d'),
)
