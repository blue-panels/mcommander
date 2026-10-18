# test_all runs the extfs helpers on recorded input and compares what
# mc_parse_ls_l makes of their output with the recorded result
mc_parse_ls_l = executable('mc_parse_ls_l', 'mc_parse_ls_l.c', dependencies: libmc_dep)

awk = find_program('gawk', 'mawk', 'nawk', 'awk', required: false)
configure_file(input: 'data/config.sh.in', output: 'config.sh',
  configuration: {
    'PERL': perl_path,
    'AWK': awk.found() ? fs.name(awk.full_path()) : 'awk',
    'PYTHON': python_path,
    'RUBY': ruby_path,
  })

# where to find mc_parse_ls_l and mc_xcat
helpers_list_env = environment()
helpers_list_env.prepend('PATH', meson.current_build_dir(), meson.current_source_dir())

test('helpers-list', find_program('test_all'),
  args: [
    '--data-dir', meson.current_source_dir() / 'data',
    '--data-build-dir', meson.current_build_dir(),
    # before installation, some helpers are in the build tree, some in the source tree
    '--helpers-dir', meson.project_build_root() / 'src' / 'vfs' / 'extfs' / 'helpers',
    '--helpers-dir', meson.project_source_root() / 'src' / 'vfs' / 'extfs' / 'helpers',
  ],
  env: helpers_list_env,
  depends: mc_parse_ls_l,
  workdir: meson.current_build_dir(),
  suite: 'src/vfs/extfs')
