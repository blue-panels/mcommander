lib_tests = {
  'far2l': 'far2l.c',
  'key': 'key.c',
  'library_independ': 'library_independ.c',
  'mc_build_filename': 'mc_build_filename.c',
  'mc_realpath': 'mc_realpath.c',
  'name_quote': 'name_quote.c',
  'serialize': 'serialize.c',
  'runtime_events': 'runtime_events.c',
  'terminal': 'terminal.c',
  'tty': 'tty.c',
  'tty_color': 'tty_color.c',
  'utilunix__mc_pread_signal': 'utilunix__mc_pread_signal.c',
  'utilunix__mc_pstream_get_string': 'utilunix__mc_pstream_get_string.c',
  'utilunix__my_system_fork_fail': 'utilunix__my_system-fork_fail.c',
  'utilunix__my_system_fork_child_shell': 'utilunix__my_system-fork_child_shell.c',
  'utilunix__my_system_fork_child': 'utilunix__my_system-fork_child.c',
  'x_basename': 'x_basename.c',
}

lua_scripts_src = meson.project_source_root() / 'src' / 'lua' / 'scripts' / 'mc'
lib_test_c_args = test_c_args + [
  '-DTEST_RUNTIME_PLUGIN_DIR="@0@"'.format(meson.current_build_dir()),
  '-DTEST_LUA_RUNTIME_DIR="@0@"'.format(meson.project_build_root() / 'src' / 'lua'),
  '-DTEST_LUA_JAVA_CLASS_DIR="@0@"'.format(lua_scripts_src / 'lua-java-class'),
  '-DTEST_LUA_MARKDOWN_DIR="@0@"'.format(lua_scripts_src / 'lua-markdown'),
  '-DTEST_LUA_MARKDOWN_DATA_DIR="@0@"'.format(meson.current_source_dir() / 'lua_markdown'),
  '-DTEST_LUA_READELF_DIR="@0@"'.format(lua_scripts_src / 'lua-readelf'),
  '-DTEST_LUA_SIXEL_DIR="@0@"'.format(lua_scripts_src / 'lua-sixel'),
]

foreach name, source : lib_tests
  test(name,
    executable(name, source, c_args: lib_test_c_args, dependencies: test_lib_deps),
    workdir: meson.current_build_dir(),
    suite: 'lib')
endforeach

if have_gmodule
  # runtime plugins for the loader to find: the only modules in this directory
  fixtures = [
    shared_module('runtime-plugin-fixture', 'runtime_plugin_fixture.c',
      name_prefix: '', name_suffix: 'so', dependencies: glib_dep),
    # the same plugin, off until the user turns it on
    shared_module('runtime-plugin-off-fixture', 'runtime_plugin_fixture.c',
      c_args: '-DFIXTURE_OFF_BY_DEFAULT', name_prefix: '', name_suffix: 'so', dependencies: glib_dep),
    # the same name as runtime-plugin-fixture, an init() that fails, and found first
    shared_module('runtime-plugin-badinit-fixture', 'runtime_plugin_fixture.c',
      c_args: '-DFIXTURE_INIT_FAILS', name_prefix: '', name_suffix: 'so', dependencies: glib_dep),
  ]

  test('extension_runtime',
    executable('extension_runtime', 'extension_runtime.c',
      c_args: lib_test_c_args, dependencies: test_lib_deps),
    depends: fixtures,
    workdir: meson.current_build_dir(),
    suite: 'lib')

  if lua.found()
    test('lua_runtime',
      executable('lua_runtime', 'lua_runtime.c',
        c_args: lib_test_c_args, dependencies: test_lib_deps),
      depends: is_variable('mc_lua') ? mc_lua : [],
      workdir: meson.current_build_dir(),
      suite: 'lib')
  endif
endif

subdir('mcconfig')
subdir('search')
subdir('strutil')
subdir('vfs')
subdir('widget')
