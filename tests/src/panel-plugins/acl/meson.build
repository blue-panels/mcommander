test('acl_model',
  executable('acl_model', 'acl_model.c', pp_src / 'acl' / 'acl-model.c',
    c_args: test_c_args, dependencies: [test_lib_deps, libacl]),
  workdir: meson.current_build_dir(),
  suite: 'src/panel-plugins/acl')
test('acl_nt',
  executable('acl_nt', 'acl_nt.c', pp_src / 'acl' / 'acl-nt.c',
    c_args: test_c_args, dependencies: [test_lib_deps, libacl]),
  workdir: meson.current_build_dir(),
  suite: 'src/panel-plugins/acl')
