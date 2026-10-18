test('mongo_render_test',
  executable('mongo_render_test', 'mongo_render_test.c', pp_src / 'mongo' / 'mongo_render.c',
    c_args: test_c_args, dependencies: [test_lib_deps, mongoc]),
  workdir: meson.current_build_dir(),
  suite: 'src/panel-plugins/mongo')
