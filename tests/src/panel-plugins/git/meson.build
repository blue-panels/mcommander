test('git_range',
  executable('git_range', 'git_range.c',
    c_args: test_c_args + ['-DMC_PLUGIN_DIR="/tmp"'], dependencies: test_lib_deps),
  workdir: meson.current_build_dir(),
  suite: 'src/panel-plugins/git')
