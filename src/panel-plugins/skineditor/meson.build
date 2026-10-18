skineditor_plugin_dir = panel_plugins_dir / 'skineditor'

# the model and the table, apart from the dialog so that the tests can link them
libmcskinedit = static_library('mcskinedit',
  'skinedit_model.c',
  'skinedit_table.c',
  dependencies: plugin_deps,
  pic: true,
)

shared_module('mc-panel-skineditor',
  'skineditor.c',
  'skineditor_color.c',
  'skineditor_grid.c',
  'skineditor_keylist.c',
  'skineditor_sample.c',
  'skineditor_samples.c',
  'skineditor_ui.c',
  c_args: '-DMC_PLUGIN_DIR="@0@"'.format(skineditor_plugin_dir),
  name_prefix: '',
  name_suffix: 'so',
  link_with: libmcskinedit,
  dependencies: plugin_deps,
  install: true,
  install_dir: skineditor_plugin_dir,
)
install_data('skineditor.md', install_dir: skineditor_plugin_dir)
