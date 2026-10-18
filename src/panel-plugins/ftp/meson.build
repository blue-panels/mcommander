ftp_plugin_dir = panel_plugins_dir / 'ftp'
shared_module('mc-panel-ftp',
  'ftp.c',
  c_args: '-DMC_PLUGIN_DIR="@0@"'.format(ftp_plugin_dir),
  name_prefix: '',
  name_suffix: 'so',
  dependencies: [plugin_deps, libcurl],
  install: true,
  install_dir: ftp_plugin_dir,
)
install_data('ftp_panel.md', install_dir: ftp_plugin_dir)
