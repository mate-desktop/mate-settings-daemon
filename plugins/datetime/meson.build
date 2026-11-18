if have_polkit
  msd_datetime_generated = custom_target('msd-datetime-generated',
    input: 'msd-datetime-mechanism.xml',
    output: ['msd-datetime-generated.c', 'msd-datetime-generated.h'],
    command: [
      gdbus_codegen,
      '--interface-prefix', 'org.mate.SettingsDaemon.',
      '--c-namespace', 'MateSettings',
      '--generate-c-code', '@OUTDIR@/msd-datetime-generated',
      '@INPUT@',
    ],
  )

  executable('msd-datetime-mechanism',
    'msd-datetime-mechanism.c',
    'msd-datetime-mechanism.h',
    'msd-datetime-mechanism-main.c',
    'system-timezone.c',
    'system-timezone.h',
    msd_datetime_generated,
    include_directories: [top_inc, msd_daemon_inc],
    dependencies: settings_plugin_deps + [polkit_dep],
    install: true,
    install_dir: msd_libexecdir,
  )

  dbus_services = configure_file(
    input: 'org.mate.SettingsDaemon.DateTimeMechanism.service.in',
    output: 'org.mate.SettingsDaemon.DateTimeMechanism.service',
    configuration: {
      'LIBEXECDIR': msd_libexecdir,
    },
  )

  install_data(dbus_services,
    install_dir: join_paths(msd_datadir, 'dbus-1/system-services'),
  )

  install_data('org.mate.SettingsDaemon.DateTimeMechanism.conf',
    install_dir: dbus_sys_dir,
  )

  i18n.merge_file(
    input: 'org.mate.settingsdaemon.datetimemechanism.policy.in',
    output: 'org.mate.settingsdaemon.datetimemechanism.policy',
    type: 'xml',
    po_dir: '../../po',
    install: true,
    install_dir: join_paths(msd_datadir, 'polkit-1/actions'),
  )
endif
