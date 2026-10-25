libinternal_sources = files(
  'args.c',
  'clipboard.c',
  'cons.handler.c',
  'events_init.c',
  'execute.c',
  'file_history.c',
  'help.c',
  'help_md.c',
  'key_learn.c',
  'key_sniffer.c',
  'keybind_dialog.c',
  'keymap.c',
  'manage_plugins.c',
  'runtime-host.c',
  'runtime-panel-provider.c',
  'runtime-screen.c',
  'runtime-viewer-source.c',
  'selcodepage.c',
  'setup.c',
  'textconf.c',
  'usermenu.c',
  'usermenu_cond.c',
  'usermenu_file.c',
  'usermenu_import.c',
  'usermenu_ini.c',
  'util.c',

  'syntax/syntax.c',
  'syntax/tty-backend.c',

  'mctree/mctree-json-provider.c',
  'mctree/mctree-model.c',
  'mctree/mctree-resolver.c',
  'mctree/mctree-view.c',
  'mctree/mctree-xml-provider.c',
  'mctree/mctree-yaml-provider.c',

  'filemanager/achown.c',
  'filemanager/boxes.c',
  'filemanager/cd.c',
  'filemanager/chmod.c',
  'filemanager/chown.c',
  'filemanager/cmd.c',
  'filemanager/command.c',
  'filemanager/dir.c',
  'filemanager/dir_watch.c',
  'filemanager/dnd.c',
  'filemanager/ext.c',
  'filemanager/file.c',
  'filemanager/filegui.c',
  'filemanager/filemanager.c',
  'filemanager/filenot.c',
  'filemanager/find.c',
  'filemanager/hotlist.c',
  'filemanager/info.c',
  'filemanager/layout.c',
  'filemanager/mcmagic.c',
  'filemanager/mcterm_overlay.c',
  'filemanager/mountlist.c',
  'filemanager/panel.c',
  'filemanager/panel_modes.c',
  'filemanager/panel_plugin_ops.c',
  'filemanager/panel_plugin_ui.c',
  'filemanager/runtime_menu.c',
  'filemanager/tree.c',
  'filemanager/treestore.c',
  'filemanager/wprompt.c',

  'vfs/plugins_init.c',
  'vfs/local/local.c',

  'viewer/actions_cmd.c',
  'viewer/ansi.c',
  'viewer/ascii.c',
  'viewer/coord_cache.c',
  'viewer/datasource.c',
  'viewer/dialogs.c',
  'viewer/display.c',
  'viewer/filter.c',
  'viewer/growbuf.c',
  'viewer/hex.c',
  'viewer/lib.c',
  'viewer/mcviewer.c',
  'viewer/move.c',
  'viewer/nroff.c',
  'viewer/search.c',
  'viewer/selection.c',
  'viewer/source.c',
  'viewer/structured.c',
  'viewer/syntax.c',
  'viewer/terminal_buffer.c',
  'viewer/vterm.c',

  'mcterm/mcterm.c',
  'mcterm/mcterm_cwd.c',
  'mcterm/mcterm_far2l.c',
  'mcterm/mcterm_filter.c',
  'mcterm/mcterm_key.c',
  'mcterm/mcterm_proto.c',
  'mcterm/mcterm_select.c',
  'mcterm/mcterm_screen.c',
  'mcterm/mcterm_setup.c',
)

if get_option('background')
  libinternal_sources += files('background.c')
endif
if ext2fs_attr.found()
  libinternal_sources += files('filemanager/chattr.c')
endif
if have_vfs_sfs
  libinternal_sources += files('vfs/sfs/sfs.c')
endif

if get_option('internal_edit')
  libedit_sources = files(
    'editor/bookmark.c',
    'editor/edit.c',
    'editor/editbuffer.c',
    'editor/editcmd.c',
    'editor/editcomplete.c',
    'editor/editdraw.c',
    'editor/editmacros.c',
    'editor/editmenu.c',
    'editor/editoptions.c',
    'editor/editsearch.c',
    'editor/editwidget.c',
    'editor/fold.c',
    'editor/format.c',
    'editor/syntax.c',
    'editor-plugins/ctags/ctags-config.c',
    'editor-plugins/ctags/ctags-fuzzy.c',
    'editor-plugins/ctags/ctags-index.c',
    'editor-plugins/ctags/ctags-parser.c',
    'editor-plugins/ctags/ctags-plugin.c',
    'editor-plugins/ctags/ctags-repository.c',
    'editor-plugins/ctags/ctags-ui.c',
    'editor-plugins/etags/etags.c',
    'editor-plugins/spell/spell.c',
  )
endif


if get_option('diff_viewer')
  libinternal_sources += files(
    'diffviewer/align3.c',
    'diffviewer/compare.c',
    'diffviewer/search.c',
    'diffviewer/syntax.c',
    'diffviewer/ydiff.c',
    'diffviewer/xdiff/xdiffi.c',
    'diffviewer/xdiff/xemit.c',
    'diffviewer/xdiff/xhistogram.c',
    'diffviewer/xdiff/xmerge.c',
    'diffviewer/xdiff/xpatience.c',
    'diffviewer/xdiff/xprepare.c',
    'diffviewer/xdiff/xutils.c',
  )
endif

src_c_args = [
  '-DSYSCONFDIR="@0@/"'.format(pkgsysconfdir),
  '-DLIBEXECDIR="@0@/"'.format(pkglibexecdir),
  '-DDATADIR="@0@/"'.format(pkgdatadir),
  '-DLOCALEDIR="@0@"'.format(localedir),
  '-DEXTHELPERSDIR="@0@/"'.format(exthelpersdir),
  '-DMC_PANEL_PLUGINS_DIR="@0@"'.format(panel_plugins_dir),
  '-DMC_EDITOR_PLUGINS_DIR="@0@"'.format(editor_plugins_dir),
  '-DMC_RUNTIME_PLUGINS_DIR="@0@"'.format(runtime_plugins_dir),
  '-DMC_PKGDATADIR="@0@"'.format(pkgdatadir),
  '-DMC_SYSCONFDIR="@0@"'.format(sysconfdir),
  '-DMC_PKGLIBEXECDIR="@0@"'.format(pkglibexecdir),
  '-DMC_BINDIR="@0@"'.format(bindir),
]
if lua.found()
  src_c_args += [
    '-DMC_LUA_SYSTEM_SCRIPTS_DIR="@0@"'.format(lua_scripts_dir),
    '-DMC_LUA_SYSTEM_MODULES_DIR="@0@"'.format(lua_modules_dir),
  ]
endif
# the console saver, Linux only
have_cons_saver = is_linux
if have_cons_saver
  src_c_args += '-DSAVERDIR="@0@"'.format(pkglibexecdir)
endif

libinternal_deps = [libmc_dep, intl, libutil, libmagic, fsinfo_deps]

# extfs.c takes LIBEXECDIR as the directory of its helpers, without the slash at the end
libinternal_link = []
if have_vfs_extfs
  libinternal_link += static_library('vfs-extfs',
    'vfs/extfs/extfs.c',
    c_args: '-DLIBEXECDIR="@0@"'.format(pkglibexecdir),
    dependencies: libinternal_deps,
    implicit_include_directories: false,
  )
endif

# the editor plugins include the headers of the editor by their names, as automake let them
if get_option('internal_edit')
  libinternal_link += static_library('edit',
    libedit_sources,
    c_args: src_c_args,
    include_directories: include_directories('editor'),
    dependencies: libinternal_deps,
    implicit_include_directories: false,
  )
endif

# Not the directory of the sources on the path of <...>: src/util.h would
# hide the <util.h> of the system, which has openpty() on macOS and NetBSD.
# The sources include what is beside them by "...", and the rest by its
# path from the top.
libinternal = static_library('internal',
  libinternal_sources,
  c_args: src_c_args,
  dependencies: libinternal_deps,
  link_with: libinternal_link,
  implicit_include_directories: false,
)

libinternal_dep = declare_dependency(link_with: libinternal, dependencies: libinternal_deps)

mcommander = executable('mcommander',
  'main.c',
  c_args: src_c_args,
  dependencies: libinternal_dep,
  implicit_include_directories: false,
  # libmc.so exports everything; a static libmc goes in whole for the same reason
  link_whole: get_option('mclib') ? [] : libmc,
  # the plugins call back into the program
  export_dynamic: have_gmodule,
  install: true,
)

# the names the program is started by: it takes its role from argv[0]
foreach name : ['mc6', 'mview', 'mctree', 'mcstruct']
  install_symlink(name, pointing_to: 'mcommander', install_dir: bindir)
endforeach
if get_option('internal_edit')
  install_symlink('mcedit6', pointing_to: 'mcommander', install_dir: bindir)
endif
if get_option('diff_viewer')
  install_symlink('mcdiff6', pointing_to: 'mcommander', install_dir: bindir)
endif

if have_cons_saver
  executable('cons.saver',
    'consaver/cons.saver.c',
    implicit_include_directories: false,
    install: true,
    install_dir: pkglibexecdir,
  )
endif

if have_vfs_sfs
  install_data('vfs/sfs/sfs.ini', install_dir: pkgsysconfdir)
endif

if get_option('internal_edit')
  install_data(
    'editor-plugins/ctags/ctags.md',
    'editor-plugins/etags/etags.md',
    'editor-plugins/spell/spell.md',
    install_dir: pkgdatadir / 'help',
  )
endif

if have_vfs_extfs
  subdir('vfs/extfs/helpers')
endif
subdir('panel-plugins')
# after the panel plugins: the session module takes their plugin_deps
if have_session
  subdir('session')
endif
if lua.found()
  subdir('lua')
endif
