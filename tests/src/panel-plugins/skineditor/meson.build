foreach name : ['model_value', 'model_resolve', 'model_save', 'model_chars']
  test(name,
    executable(name, name + '.c',
      c_args: test_c_args, link_with: libmcskinedit, dependencies: test_lib_deps),
    workdir: meson.current_build_dir(),
    suite: 'src/panel-plugins/skineditor')
endforeach

# every skin key the code reads must have a row in the label table
skinedit_table_dump = executable('skinedit_table_dump', 'skinedit_table_dump.c',
  c_args: test_c_args, link_with: libmcskinedit, dependencies: test_lib_deps)
test('check_table.sh', find_program('sh'),
  args: [meson.current_source_dir() / 'check_table.sh'],
  env: {'srcdir': meson.current_source_dir()},
  depends: skinedit_table_dump,
  workdir: meson.current_build_dir(),
  suite: 'src/panel-plugins/skineditor')
