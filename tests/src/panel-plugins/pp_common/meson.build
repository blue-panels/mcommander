foreach name : ['pp_rename_with_ext', 'pp_add_entry_st']
  test(name,
    executable(name, name + '.c', c_args: test_c_args, dependencies: test_lib_deps),
    workdir: meson.current_build_dir(),
    suite: 'src/panel-plugins/pp_common')
endforeach
