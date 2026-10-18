foreach name : ['glob_prepare_replace_str', 'glob_translate_to_regex', 'hex_translate_to_regex',
                'regex_replace_esc_seq', 'regex_process_escape_sequence',
                'translate_replace_glob_to_regex']
  test(name,
    executable(name, name + '.c',
      c_args: test_c_args,
      include_directories: include_directories('../../../lib/search'),
      dependencies: test_lib_deps),
    workdir: meson.current_build_dir(),
    suite: 'lib/search')
endforeach
