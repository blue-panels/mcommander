docker_plugin_dir = panel_plugins_dir / 'docker'
shared_module('mc-panel-docker',
  'docker.c',
  'docker-ui.c',
  'docker-logs.c',
  'containers.c',
  'container-files.c',
  'images.c',
  'volumes.c',
  'networks.c',
  'docker-cp-stream.c',
  'docker-tar.c',
  c_args: '-DMC_PLUGIN_DIR="@0@"'.format(docker_plugin_dir),
  name_prefix: '',
  name_suffix: 'so',
  dependencies: plugin_deps,
  install: true,
  install_dir: docker_plugin_dir,
)
install_data('docker_panel.md', install_dir: docker_plugin_dir)
# meant to be run
install_data('dlog.sh', install_dir: docker_plugin_dir, install_mode: 'rwxr-xr-x')
