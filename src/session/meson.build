shared_module('session',
  'session.c',
  'session_proto.c',
  c_args: '-DMC_SESSION_HELPER="@0@"'.format(pkglibexecdir / 'session_helper'),
  # the headers of lib/tty include those of the screen library
  dependencies: plugin_deps,
  name_prefix: '',
  name_suffix: 'so',
  implicit_include_directories: false,
  install: true,
  install_dir: runtime_plugins_dir,
)

executable('session_helper',
  'session_helper.c',
  'session_proto.c',
  c_args: [
    '-DMC_SESSION_HELPER="@0@"'.format(pkglibexecdir / 'session_helper'),
    '-DLOCALEDIR="@0@"'.format(localedir),
  ],
  # openpty() is in libutil on glibc before 2.34 and on the BSDs
  dependencies: [glib_dep, intl, libutil],
  implicit_include_directories: false,
  install: true,
  install_dir: pkglibexecdir,
)
