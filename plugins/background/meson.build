libbackground_sources = [
  'msd-background-plugin.h',
  'msd-background-plugin.c',
  'msd-background-manager.h',
  'msd-background-manager.c',
]

libbackground_deps = settings_plugin_deps + [mate_desktop_dep] + x11_libs

if have_wayland
  libbackground_sources += [
    'msd-background-manager-wayland.c',
    'msd-background-manager-wayland.h',
  ]
  libbackground_deps += [gtk_layer_shell_dep, wayland_client_dep]
endif

libbackground = shared_module('background',
  libbackground_sources,
  include_directories: [top_inc, msd_daemon_inc],
  dependencies: libbackground_deps,
  name_suffix: 'so',
  install: true,
  install_dir: msd_plugindir,
)

i18n.merge_file(
  input: 'background.mate-settings-plugin.desktop.in',
  output: 'background.mate-settings-plugin',
  type: 'desktop',
  po_dir: '../../po',
  install: true,
  install_dir: msd_plugindir,
)

test_background_sources = [
  'test-background.c',
  'msd-background-manager.h',
  'msd-background-manager.c',
]

test_background_deps = settings_plugin_deps + [mate_desktop_dep] + x11_libs

if have_wayland
  test_background_sources += [
    'msd-background-manager-wayland.c',
    'msd-background-manager-wayland.h',
  ]
  test_background_deps += [gtk_layer_shell_dep, wayland_client_dep]
endif

test_background = executable('test-background',
  test_background_sources,
  include_directories: [top_inc, msd_daemon_inc],
  link_with: libmsd_profile,
  dependencies: test_background_deps,
)
