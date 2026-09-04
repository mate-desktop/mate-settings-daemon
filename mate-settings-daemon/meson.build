libmsd_profile = static_library('msd-profile',
  'mate-settings-profile.c',
  'mate-settings-profile.h',
  include_directories: top_inc,
  dependencies: settings_daemon_deps,
)

mate_settings_daemon_deps = [
  glib_dep,
  gtk_dep,
  gio_dep,
  mate_desktop_dep,
  libnotify_dep,
]

executable('mate-settings-daemon',
  'main.c',
  'mate-settings-bus.c',
  'mate-settings-bus.h',
  'mate-settings-manager.c',
  'mate-settings-manager.h',
  'mate-settings-plugin.c',
  'mate-settings-plugin.h',
  'mate-settings-plugin-info.c',
  'mate-settings-plugin-info.h',
  'mate-settings-module.c',
  'mate-settings-module.h',
  include_directories: top_inc,
  link_with: libmsd_profile,
  dependencies: settings_daemon_deps + mate_settings_daemon_deps,
  install: true,
  install_dir: msd_libexecdir,
)

install_headers('mate-settings-plugin.h', subdir: 'mate-settings-daemon')
