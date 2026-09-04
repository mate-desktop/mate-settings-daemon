foreach size : ['16', '22', '24', '32', '48', '64', '96']
  install_data('msd-xrandr-@0@.png'.format(size),
    install_dir: join_paths(msd_datadir, 'icons/hicolor/@0@x@0@/apps'.format(size)),
    rename: 'msd-xrandr.png',
  )
endforeach

install_data('msd-xrandr.svg',
  install_dir: join_paths(msd_datadir, 'icons/hicolor/scalable/apps'),
)

libxrandr = shared_module('xrandr',
  'msd-xrandr-plugin.h',
  'msd-xrandr-plugin.c',
  'msd-xrandr-manager.h',
  'msd-xrandr-manager.c',
  include_directories: [top_inc, msd_daemon_inc],
  dependencies: settings_plugin_deps + [libnotify_dep, mate_desktop_dep] + x11_libs,
  c_args: [
    '-DBINDIR="@0@"'.format(msd_bindir),
  ],
  name_suffix: 'so',
  install: true,
  install_dir: msd_plugindir,
)

i18n.merge_file(
  input: 'xrandr.mate-settings-plugin.desktop.in',
  output: 'xrandr.mate-settings-plugin',
  type: 'desktop',
  po_dir: '../../po',
  install: true,
  install_dir: msd_plugindir,
)
