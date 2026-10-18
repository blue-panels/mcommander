foreach name : ['parse_integer', 'str_replace_all', 'str_verscmp', 'str_rstrip_eol', 'filevercmp']
  test(name,
    executable(name, name + '.c', c_args: test_c_args, dependencies: test_lib_deps),
    workdir: meson.current_build_dir(),
    suite: 'lib/strutil')
endforeach
