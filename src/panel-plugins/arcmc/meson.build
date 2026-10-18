arcmc_plugin_dir = panel_plugins_dir / 'arcmc'
shared_module('mc-panel-arcmc',
  'arcmc.c',
  'arcmc-config.c',
  'arcmc-ext.c',
  'arcmc-reader.c',
  'archive-io.c',
  'arcmc-entries.c',
  'progress.c',
  'dialog-pack.c',
  'dialog-settings.c',
  meson.project_source_root() / 'lib/widget/table.c',
  c_args: [
    '-DLIBEXECDIR="@0@"'.format(pkglibexecdir),
    '-DMC_PLUGIN_DIR="@0@"'.format(arcmc_plugin_dir),
  ],
  name_prefix: '',
  name_suffix: 'so',
  dependencies: [plugin_deps, libarchive, libmagic],
  install: true,
  install_dir: arcmc_plugin_dir,
)
install_data('arcmc_panel.md', install_dir: arcmc_plugin_dir)
