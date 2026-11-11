test('panel_plugin_cache',
  executable('panel_plugin_cache', 'panel_plugin_cache.c',
    c_args: lib_test_c_args, dependencies: test_lib_deps),
  workdir: meson.current_build_dir(),
  suite: 'lib')

# A directory of their own: the extension_runtime test takes every module in tests/lib as its own.
panel_plugin_fixtures = [
  shared_module('panel-plugin-fixture', 'panel_plugin_fixture.c',
    name_prefix: '', name_suffix: 'so', dependencies: glib_dep),
  shared_module('panel-plugin-eager-fixture', 'panel_plugin_fixture.c',
    c_args: '-DFIXTURE_EAGER', name_prefix: '', name_suffix: 'so', dependencies: glib_dep),
  shared_module('panel-plugin-v2-fixture', 'panel_plugin_fixture.c',
    c_args: '-DFIXTURE_V2', name_prefix: '', name_suffix: 'so', dependencies: glib_dep),
  shared_module('panel-plugin-other-fixture', 'panel_plugin_fixture.c',
    c_args: '-DFIXTURE_NAME="other"', name_prefix: '', name_suffix: 'so', dependencies: glib_dep),
]

test('panel_plugin_lazy',
  executable('panel_plugin_lazy', 'panel_plugin_lazy.c',
    c_args: lib_test_c_args + [
      '-DTEST_PANEL_PLUGIN_DIR="@0@"'.format(meson.current_build_dir()),
    ],
    dependencies: test_lib_deps),
  depends: panel_plugin_fixtures,
  workdir: meson.current_build_dir(),
  suite: 'lib')
