msd_marshal_h = custom_target('msd-marshal-header',
  input: 'msd-marshal.list',
  output: 'msd-marshal.h',
  command: [glib_genmarshal, '--prefix=msd_marshal', '@INPUT@', '--header', '--internal', '--output', '@OUTPUT@'],
)

msd_marshal_c = custom_target('msd-marshal-body',
  input: 'msd-marshal.list',
  output: 'msd-marshal.c',
  command: [glib_genmarshal, '--prefix=msd_marshal', '@INPUT@', '--body', '--prototypes', '--internal', '--output', '@OUTPUT@'],
)

libmedia_keys = shared_module('media-keys',
  'msd-media-keys-plugin.h',
  'msd-media-keys-plugin.c',
  'msd-media-keys-manager.h',
  'msd-media-keys-manager.c',
  'msd-media-keys-window.h',
  'msd-media-keys-window.c',
  'acme.h',
  msd_marshal_h,
  msd_marshal_c,
  include_directories: [top_inc, msd_daemon_inc, include_directories('../common')],
  dependencies: settings_plugin_deps + [libmatemixer_dep, libcanberra_dep],
  c_args: [
    '-DPIXMAPDIR="@0@"'.format(msd_pkgdatadir),
    '-DGTKBUILDERDIR="@0@"'.format(msd_pkgdatadir),
  ],
  link_args: ['-lm'],
  name_suffix: 'so',
  install: true,
  install_dir: msd_plugindir,
)

install_data('acme.ui',
  install_dir: msd_pkgdatadir,
)

i18n.merge_file(
  input: 'media-keys.mate-settings-plugin.desktop.in',
  output: 'media-keys.mate-settings-plugin',
  type: 'desktop',
  po_dir: '../../po',
  install: true,
  install_dir: msd_plugindir,
)

test_media_window = executable('test-media-window',
  'msd-media-keys-window.c',
  'msd-media-keys-window.h',
  'test-media-window.c',
  include_directories: [top_inc, msd_daemon_inc, include_directories('../common')],
  link_with: libcommon,
  dependencies: settings_daemon_deps + settings_plugin_deps,
  c_args: [
    '-DPIXMAPDIR="@0@"'.format(msd_pkgdatadir),
    '-DGTKBUILDERDIR="@0@"'.format(msd_pkgdatadir),
  ],
  link_args: ['-lm'],
)

test_media_keys = executable('test-media-keys',
  'msd-media-keys-manager.c',
  'msd-media-keys-manager.h',
  'msd-media-keys-window.h',
  'msd-media-keys-window.c',
  'test-media-keys.c',
  msd_marshal_h,
  msd_marshal_c,
  include_directories: [top_inc, msd_daemon_inc, include_directories('../common')],
  link_with: [libmsd_profile, libcommon],
  dependencies: settings_daemon_deps + settings_plugin_deps + [libmatemixer_dep, libcanberra_dep],
  c_args: [
    '-DPIXMAPDIR="@0@"'.format(msd_pkgdatadir),
    '-DGTKBUILDERDIR="@0@"'.format(msd_pkgdatadir),
  ],
  link_args: ['-lm'],
)
