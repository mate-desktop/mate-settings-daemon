libsmartcard = shared_module('smartcard',
  'msd-smartcard-plugin.h',
  'msd-smartcard-plugin.c',
  'msd-smartcard.h',
  'msd-smartcard.c',
  'msd-smartcard-manager.h',
  'msd-smartcard-manager.c',
  include_directories: [top_inc, msd_daemon_inc],
  dependencies: settings_plugin_deps + [nss_dep],
  c_args: [
    '-DSYSCONFDIR="@0@"'.format(msd_prefix / get_option('sysconfdir')),
    '-DLIBDIR="@0@"'.format(msd_libdir),
    '-DMSD_SMARTCARD_MANAGER_NSS_DB="@0@"'.format(nss_database),
  ],
  name_suffix: 'so',
  install: true,
  install_dir: msd_plugindir,
)

i18n.merge_file(
  input: 'smartcard.mate-settings-plugin.desktop.in',
  output: 'smartcard.mate-settings-plugin',
  type: 'desktop',
  po_dir: '../../po',
  install: true,
  install_dir: msd_plugindir,
)
