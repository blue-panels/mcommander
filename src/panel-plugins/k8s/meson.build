k8s_plugin_dir = panel_plugins_dir / 'k8s'
shared_module('mc-panel-k8s',
  'k8s.c',
  'k8s-cmd.c',
  'k8s-logs.c',
  'k8s-ui.c',
  'pods.c',
  'workloads.c',
  'services.c',
  'nodes.c',
  c_args: '-DMC_PLUGIN_DIR="@0@"'.format(k8s_plugin_dir),
  name_prefix: '',
  name_suffix: 'so',
  dependencies: plugin_deps,
  install: true,
  install_dir: k8s_plugin_dir,
)
install_data('k8s_panel.md', install_dir: k8s_plugin_dir)
# meant to be run
install_data('klog.sh', install_dir: k8s_plugin_dir, install_mode: 'rwxr-xr-x')
