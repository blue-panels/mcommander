sftp_plugin_dir = panel_plugins_dir / 'sftp'
shared_module('mc-panel-sftp',
  'sftp.c',
  c_args: '-DMC_PLUGIN_DIR="@0@"'.format(sftp_plugin_dir),
  name_prefix: '',
  name_suffix: 'so',
  dependencies: [plugin_deps, libssh2],
  install: true,
  install_dir: sftp_plugin_dir,
)
install_data('sftp_panel.md', install_dir: sftp_plugin_dir)
