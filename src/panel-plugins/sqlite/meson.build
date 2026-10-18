sqlite_plugin_dir = panel_plugins_dir / 'sqlite'
shared_module('mc-panel-sqlite',
  'sqlite.c',
  'sqlite_json.c',
  c_args: '-DMC_PLUGIN_DIR="@0@"'.format(sqlite_plugin_dir),
  name_prefix: '',
  name_suffix: 'so',
  dependencies: [plugin_deps, sqlite3],
  install: true,
  install_dir: sqlite_plugin_dir,
)
install_data('sqlite_panel.md', install_dir: sqlite_plugin_dir)
