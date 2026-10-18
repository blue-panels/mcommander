foreach name : ['config_string', 'user_configs_path']
  test(name,
    executable(name, name + '.c',
      c_args: test_c_args + ['-DWORKDIR="@0@"'.format(meson.current_build_dir())],
      dependencies: test_lib_deps),
    workdir: meson.current_build_dir(),
    suite: 'lib/mcconfig')
endforeach
