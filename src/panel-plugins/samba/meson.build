samba_plugin_dir = panel_plugins_dir / 'samba'
shared_module('mc-panel-samba',
  'samba.c',
  c_args: '-DMC_PLUGIN_DIR="@0@"'.format(samba_plugin_dir),
  name_prefix: '',
  name_suffix: 'so',
  dependencies: [plugin_deps, smbclient],
  install: true,
  install_dir: samba_plugin_dir,
)
install_data('samba_panel.md', install_dir: samba_plugin_dir)
