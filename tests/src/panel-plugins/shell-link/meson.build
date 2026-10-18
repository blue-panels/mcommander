shfs_test_deps = [test_lib_deps, shfs_deps]

test('shfs_parse_ls',
  executable('shfs_parse_ls', 'shfs_parse_ls.c',
    c_args: test_c_args, link_with: libshfs, dependencies: shfs_test_deps),
  workdir: meson.current_build_dir(),
  suite: 'src/panel-plugins/shell-link')

# prints a script built into the plugin, for the helpers_*.sh tests
builtin_helper = executable('builtin_helper', 'builtin_helper.c',
  c_args: test_c_args, link_with: libshfs, dependencies: shfs_test_deps)

foreach name : ['helpers_ls.sh', 'helpers_get.sh', 'helpers_transfer.sh', 'helpers_builtin.sh']
  test(name, find_program('sh'),
    args: [meson.current_source_dir() / name],
    env: {'srcdir': meson.current_source_dir()},
    depends: builtin_helper,
    workdir: meson.current_build_dir(),
    suite: 'src/panel-plugins/shell-link')
endforeach
