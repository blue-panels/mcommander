foreach name : ['cd_to', 'examine_cd', 'ext__exec_make_shell_string',
                'exec_get_export_variables_ext', 'filegui_is_wildcarded', 'get_random_hint', 'magic',
                'panel_modes', 'panel_quick_filter', 'panel_scroll_marker_pos', 'panel_plugin_close',
                'panel_plugin_file_listing']
  test(name,
    executable(name, name + '.c',
      c_args: test_c_args + [
        '-DTEST_SHARE_DIR="@0@"'.format(meson.current_source_dir() / '..' / 'fixtures'),
      ],
      include_directories: include_directories('../../../lib/vfs'),
      dependencies: test_src_deps),
    workdir: meson.current_build_dir(),
    suite: 'src/filemanager')
endforeach
