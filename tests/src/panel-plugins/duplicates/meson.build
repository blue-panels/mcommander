dup_src = pp_src / 'duplicates'

test('dup_engine',
  executable('dup_engine', 'dup_engine.c',
    dup_src / 'dup_engine.c', dup_src / 'dup_engine_similar.c', dup_src / 'dup_cache.c',
    dup_src / 'dup_filter.c', dup_src / 'dup_similar.c',
    c_args: test_c_args + dup_c_args, dependencies: [test_lib_deps, dup_image_deps]),
  workdir: meson.current_build_dir(),
  suite: 'src/panel-plugins/duplicates')
