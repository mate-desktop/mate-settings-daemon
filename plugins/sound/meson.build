libsound_sources = [
  'msd-sound-plugin.h',
  'msd-sound-plugin.c',
  'msd-sound-manager.h',
  'msd-sound-manager.c',
]

libsound_deps = settings_plugin_deps

if have_pulse
  libsound_deps += [pulse_dep]
endif

libsound = shared_module('sound',
  libsound_sources,
  include_directories: [top_inc, msd_daemon_inc],
  dependencies: libsound_deps,
  name_suffix: 'so',
  install: true,
  install_dir: msd_plugindir,
)

i18n.merge_file(
  input: 'sound.mate-settings-plugin.desktop.in',
  output: 'sound.mate-settings-plugin',
  type: 'desktop',
  po_dir: '../../po',
  install: true,
  install_dir: msd_plugindir,
)
