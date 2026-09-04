protocol_src = 'protocol/wlr-output-management-unstable-v1.xml'

wlr_output_management_client_h = custom_target('wlr-output-management-client-header',
  input: protocol_src,
  output: 'wlr-output-management-unstable-v1-client-protocol.h',
  command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)

wlr_output_management_private_c = custom_target('wlr-output-management-private-code',
  input: protocol_src,
  output: 'wlr-output-management-unstable-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

foreach size : ['16', '22', '24', '32', '48', '64', '96']
  install_data('msd-wlrandr-@0@.png'.format(size),
    install_dir: join_paths(msd_datadir, 'icons/hicolor/@0@x@0@/apps'.format(size)),
    rename: 'msd-wlrandr.png',
  )
endforeach

install_data('msd-wlrandr.svg',
  install_dir: join_paths(msd_datadir, 'icons/hicolor/scalable/apps'),
)

libwlrandr = shared_module('wlrandr',
  'msd-wlrandr-plugin.h',
  'msd-wlrandr-plugin.c',
  'msd-wlrandr-manager.h',
  'msd-wlrandr-manager.c',
  wlr_output_management_client_h,
  wlr_output_management_private_c,
  include_directories: [top_inc, msd_daemon_inc],
  dependencies: settings_plugin_deps + [wayland_client_dep, appindicator_dep, libnotify_dep, mate_desktop_dep],
  c_args: [
    '-DBINDIR="@0@"'.format(msd_bindir),
  ],
  name_suffix: 'so',
  install: true,
  install_dir: msd_plugindir,
)

i18n.merge_file(
  input: 'wlrandr.mate-settings-plugin.desktop.in',
  output: 'wlrandr.mate-settings-plugin',
  type: 'desktop',
  po_dir: '../../po',
  install: true,
  install_dir: msd_plugindir,
)
