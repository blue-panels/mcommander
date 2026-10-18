mcpeek_plugin_dir = panel_plugins_dir / 'mcpeek'
shared_module('mc-panel-mcpeek',
  'mcpeek.c',
  'mcpeek-pe.c',
  'mcpeek-meta.c',
  'mcpeek-sig.c',
  'mcpeek-il.c',
  'mcpeek-cs.c',
  'mcpeek-resolve.c',
  'mcpeek-find.c',
  c_args: '-DMC_PLUGIN_DIR="@0@"'.format(mcpeek_plugin_dir),
  name_prefix: '',
  name_suffix: 'so',
  dependencies: plugin_deps,
  install: true,
  install_dir: mcpeek_plugin_dir,
)
install_data('mcpeek_panel.md', install_dir: mcpeek_plugin_dir)
