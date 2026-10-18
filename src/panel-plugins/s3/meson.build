s3_plugin_dir = panel_plugins_dir / 's3'
shared_module('mc-panel-s3',
  's3.c',
  's3_connection.c',
  's3_connect_status.c',
  c_args: '-DMC_PLUGIN_DIR="@0@"'.format(s3_plugin_dir),
  name_prefix: '',
  name_suffix: 'so',
  dependencies: [plugin_deps, libcurl_s3],
  install: true,
  install_dir: s3_plugin_dir,
)
install_data('s3_panel.md', install_dir: s3_plugin_dir)
