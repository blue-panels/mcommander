foreach name : ['buttonbar_modifiers', 'complete_engine', 'filechooser', 'hotkey_equal',
                'group_init_destroy', 'group_set_state', 'hexstrip', 'paste', 'scrollbar',
                'table_datasource', 'table_quick_search', 'textarea', 'widget_find_by_id',
                'widget_make_global_local']
  test(name,
    executable(name, name + '.c',
      c_args: test_c_args,
      include_directories: include_directories('../../../lib/vfs'),
      # filechooser includes the local file system, which needs the flags of ext2
      dependencies: test_lib_deps + [ext2fs_attr]),
    workdir: meson.current_build_dir(),
    suite: 'lib/widget')
endforeach
