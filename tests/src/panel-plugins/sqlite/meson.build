test('sqlite_plugin_test',
  executable('sqlite_plugin_test', 'sqlite_plugin_test.c',
    pp_src / 'sqlite' / 'sqlite.c', pp_src / 'sqlite' / 'sqlite_json.c',
    c_args: test_c_args + ['-DMC_PLUGIN_DIR="/tmp"'], dependencies: [test_lib_deps, sqlite3]),
  workdir: meson.current_build_dir(),
  suite: 'src/panel-plugins/sqlite')
