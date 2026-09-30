src_test_c_args = test_c_args + [
  '-DTEST_SHARE_DIR="@0@"'.format(meson.current_source_dir() / 'fixtures'),
]

src_tests = ['clipboard_info', 'clipboard_osc52', 'input_clip_text', 'tty_far2l_clipboard',
             'execute__execute_with_vfs_arg', 'execute__execute_external_editor_or_viewer',
             'execute__execute_get_external_cmd_opts_from_config', 'file_history', 'keymap_reload',
             'runtime_host__syntax', 'runtime_host__services', 'learn_modifiers', 'mctree_model',
             'mctree_resolver', 'mctree_view', 'usermenu_ini', 'term_key_merge', 'tty_enter',
             'tty_far2l_input', 'tty_kitty_keyboard', 'tty_mouse_wheel', 'usermenu__test_condition',
             'usermenu__test_expand_format', 'mcterm_far2l', 'mcterm_key_encode',
             'mcterm_osc7_parse', 'mcterm_protocol_parse', 'mcterm_pty_send', 'mcterm_select']

foreach name : src_tests
  test(name,
    executable(name, name + '.c',
      c_args: src_test_c_args,
      include_directories: include_directories('../../lib/vfs'),
      dependencies: test_src_deps),
    workdir: meson.current_build_dir(),
    suite: 'src')
endforeach

subdir('syntax')
subdir('filemanager')
if have_vfs_extfs
  subdir('vfs/extfs/helpers-list')
endif
subdir('panel-plugins')
subdir('viewer')
if get_option('internal_edit')
  subdir('editor')
endif
if get_option('diff_viewer')
  subdir('diffviewer')
endif
