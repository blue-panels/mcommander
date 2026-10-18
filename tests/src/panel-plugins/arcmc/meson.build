arcmc_tests = {
  'arcmc_ext_registry': [pp_src / 'arcmc' / 'arcmc-ext.c'],
  'arcmc_utils': [],
  'arcmc_links': [pp_src / 'arcmc' / 'arcmc-entries.c'],
}
# the reader speaks to libarchive, which the plugin itself may have been built without
if libarchive.found()
  arcmc_tests += {'arcmc_reader': [pp_src / 'arcmc' / 'arcmc-reader.c']}
endif

foreach name, extra : arcmc_tests
  test(name,
    executable(name, name + '.c', extra,
      c_args: test_c_args, dependencies: [test_lib_deps, libarchive]),
    workdir: meson.current_build_dir(),
    suite: 'src/panel-plugins/arcmc')
endforeach
