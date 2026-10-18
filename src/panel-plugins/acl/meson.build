acl_plugin_dir = panel_plugins_dir / 'acl'
shared_module('mc-panel-acl',
  'acl.c',
  'acl-dialog.c',
  'acl-model.c',
  'acl-nt.c',
  'acl-nt-dialog.c',
  c_args: '-DMC_PLUGIN_DIR="@0@"'.format(acl_plugin_dir),
  name_prefix: '',
  name_suffix: 'so',
  dependencies: [plugin_deps, libacl],
  install: true,
  install_dir: acl_plugin_dir,
)
install_data('acl_panel.md', install_dir: acl_plugin_dir)
