postgres_plugin_dir = panel_plugins_dir / 'postgres'
shared_module('mc-panel-postgres',
  'postgres.c',
  'postgres_columns.c',
  'postgres_config.c',
  'postgres_conn.c',
  'postgres_conndlg.c',
  'postgres_connections.c',
  'postgres_nav.c',
  'postgres_settings.c',
  'postgres_ui.c',
  c_args: '-DMC_PLUGIN_DIR="@0@"'.format(postgres_plugin_dir),
  name_prefix: '',
  name_suffix: 'so',
  dependencies: [plugin_deps, libpq],
  install: true,
  install_dir: postgres_plugin_dir,
)
install_data('postgres_panel.md', install_dir: postgres_plugin_dir)
