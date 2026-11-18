libclipboard_sources = [
  'msd-clipboard-plugin.h',
  'msd-clipboard-plugin.c',
  'msd-clipboard-manager.h',
  'msd-clipboard-manager.c',
  'xutils.h',
  'xutils.c',
  'list.h',
  'list.c',
]

libclipboard_deps = settings_plugin_deps + [xinput_dep] + x11_libs

if have_wayland
  wayland_protocol_xml = join_paths(wayland_protocols_datadir, 'staging/ext-data-control/ext-data-control-v1.xml')

  ext_data_control_v1_client_h = custom_target('ext-data-control-v1-client-header',
    input: wayland_protocol_xml,
    output: 'ext-data-control-v1-client.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )

  ext_data_control_v1_client_c = custom_target('ext-data-control-v1-client-code',
    input: wayland_protocol_xml,
    output: 'ext-data-control-v1-client.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )

  libclipboard_sources += [
    'msd-clipboard-manager-wayland.c',
    'msd-clipboard-manager-wayland.h',
    ext_data_control_v1_client_h,
    ext_data_control_v1_client_c,
  ]
  libclipboard_deps += [wayland_client_dep]
endif

libclipboard = shared_module('clipboard',
  libclipboard_sources,
  include_directories: [top_inc, msd_daemon_inc],
  dependencies: libclipboard_deps,
  name_suffix: 'so',
  install: true,
  install_dir: msd_plugindir,
)

i18n.merge_file(
  input: 'clipboard.mate-settings-plugin.desktop.in',
  output: 'clipboard.mate-settings-plugin',
  type: 'desktop',
  po_dir: '../../po',
  install: true,
  install_dir: msd_plugindir,
)
