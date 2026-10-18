shell_link_plugin_dir = panel_plugins_dir / 'shell-link'
shell_link_helpers_dir = shell_link_plugin_dir / 'helpers'

# the helper scripts, named once in helpers/helpers.mk
shell_link_helpers = fs.read('helpers/helpers.mk').split('SHELL_LINK_HELPERS =')[1] \
  .replace('\\', ' ').replace('\t', ' ').replace('\n', ' ').strip().split()
shell_link_helper_files = []
foreach h : shell_link_helpers
  shell_link_helper_files += files('helpers' / h)
endforeach

# every helper script as a C string, so a build run from the source tree has them
# before anything is installed; taken from helpers/, not from a second copy
shelldef_h = custom_target('shelldef.h',
  output: 'shelldef.h',
  input: [shell_link_helper_files, 'helpers/helpers.mk'],
  command: [files('gen-shelldef.sh'), meson.current_source_dir() / 'helpers', '@OUTPUT@',
            shell_link_helpers],
)

shfs_c_args = '-DSHELL_LINK_HELPERS_DIR="@0@"'.format(shell_link_helpers_dir)
shfs_deps = [plugin_deps]
if have_shell_ssh2
  shfs_deps += libssh2
else
  # the headers only, when libssh2 is there but not used
  shfs_deps += libssh2.partial_dependency(compile_args: true, includes: true)
endif

# the protocol library: not installed and not in the core, linked into the plugin,
# so a build without the plugin carries neither the code nor libssh2
libshfs = static_library('shfs',
  'shfs.c',
  'shfs-log.c',
  'shfs-resume.c',
  'shell_ssh2.c',
  shelldef_h,
  c_args: shfs_c_args,
  dependencies: shfs_deps,
  pic: true,
)

shared_module('mc-panel-shell-link',
  'shell-link.c',
  shelldef_h,
  c_args: [shfs_c_args, '-DMC_PLUGIN_DIR="@0@"'.format(shell_link_plugin_dir)],
  name_prefix: '',
  name_suffix: 'so',
  link_with: libshfs,
  dependencies: shfs_deps,
  install: true,
  install_dir: shell_link_plugin_dir,
)
install_data('shell-link_panel.md', install_dir: shell_link_plugin_dir)

# proof that the library stands on its own: libshfs and the support code of mc,
# nothing of the file manager; run by hand to tell a fault of the protocol from
# one of the plugin
executable('shfs-tool',
  'shfs-tool.c',
  shelldef_h,
  c_args: shfs_c_args,
  link_with: libshfs,
  dependencies: [shfs_deps, libmc_dep],
)

install_data('helpers/README.shell', shell_link_helper_files, install_dir: shell_link_helpers_dir)
