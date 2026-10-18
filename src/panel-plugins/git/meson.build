git_plugin_dir = panel_plugins_dir / 'git'
shared_module('mc-panel-git',
  'git.c',
  c_args: '-DMC_PLUGIN_DIR="@0@"'.format(git_plugin_dir),
  name_prefix: '',
  name_suffix: 'so',
  dependencies: plugin_deps,
  install: true,
  install_dir: git_plugin_dir,
)
install_data('git_panel.md', install_dir: git_plugin_dir)
