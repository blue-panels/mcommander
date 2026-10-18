# the tests read charsets from here, with the names of the encodings of this system
configure_file(input: 'charsets.in', output: 'charsets', configuration: encoding_conf)

foreach name : ['canonicalize_pathname', 'current_dir', 'path_cmp', 'path_len',
                'path_manipulations', 'path_serialize', 'relative_cd', 'tempdir',
                'vfs_adjust_stat', 'vfs_parse_ls_lga', 'vfs_path_from_str_flags',
                'vfs_path_string_convert', 'vfs_prefix_to_class', 'vfs_setup_cwd', 'vfs_split',
                'vfs_s_get_path', 'path_recode', 'vfs_get_encoding']
  test(name,
    executable(name, name + '.c',
      c_args: test_c_args + ['-DTEST_SHARE_DIR="@0@"'.format(meson.current_build_dir())],
      include_directories: include_directories('../../../lib/vfs'),
      dependencies: test_lib_deps),
    workdir: meson.current_build_dir(),
    suite: 'lib/vfs')
endforeach
