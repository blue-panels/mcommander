# the image libraries are optional: without one, its format is not compared by the picture
dup_image_deps = [meson.get_compiler('c').find_library('m', required: false)]
dup_c_args = []
dup_jpeg = dependency('libjpeg', required: false)
if dup_jpeg.found()
  dup_image_deps += dup_jpeg
  dup_c_args += '-DDUP_HAVE_JPEG'
endif
dup_png = dependency('libpng', required: false)
if dup_png.found()
  dup_image_deps += dup_png
  dup_c_args += '-DDUP_HAVE_PNG'
endif

shared_module('mc-panel-duplicates',
  'duplicates.c',
  'dup_cache.c',
  'dup_dialog.c',
  'dup_engine.c',
  'dup_engine_similar.c',
  'dup_filter.c',
  'dup_search.c',
  'dup_session.c',
  'dup_similar.c',
  name_prefix: '',
  name_suffix: 'so',
  c_args: dup_c_args + '-DMC_PLUGIN_DIR="@0@"'.format(panel_plugins_dir / 'duplicates'),
  dependencies: [plugin_deps, dup_image_deps],
  install: true,
  install_dir: panel_plugins_dir / 'duplicates',
)
install_data('duplicates_panel.md', install_dir: panel_plugins_dir / 'duplicates')
