foreach name : ['mcview__load_zip_magic', 'mcview__stream', 'mcview__filter', 'mcview__lcache',
                'mcview__selection', 'mcview__nroff', 'ansi_parser', 'vterm_terminal']
  test(name,
    executable(name, name + '.c',
      c_args: test_c_args + [
        '-DTEST_SHARE_DIR="@0@"'.format(meson.current_source_dir() / '..' / 'fixtures'),
      ],
      include_directories: include_directories('../../../lib/vfs'),
      dependencies: test_src_deps),
    workdir: meson.current_build_dir(),
    suite: 'src/viewer')
endforeach
