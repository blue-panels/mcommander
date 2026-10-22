gh_plugin_dir = panel_plugins_dir / 'gh'
shared_module('mc-panel-gh',
  'gh.c',
  'gh-cmd.c',
  'repos.c',
  'pulls.c',
  'issues.c',
  'files.c',
  c_args: '-DMC_PLUGIN_DIR="@0@"'.format(gh_plugin_dir),
  name_prefix: '',
  name_suffix: 'so',
  dependencies: plugin_deps,
  install: true,
  install_dir: gh_plugin_dir,
)
install_data('gh_panel.md', install_dir: gh_plugin_dir)
