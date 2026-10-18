subdir('ext.d')
subdir('macros.d')
subdir('skins')
if get_option('internal_edit')
  subdir('syntax')
endif

install_data('defaults.ini', install_dir: pkgdatadir)
configure_file(input: 'charsets.in', output: 'charsets', configuration: encoding_conf, install_dir: pkgdatadir)

install_data(
  'filehighlight.ini',
  'panels.ftp.ini',
  'panels.git.ini',
  'keymap.default.ini',
  'keymap.emacs.ini',
  'keymap.vim.ini',
  'spell.keymap',
  'ctags.keymap',
  install_dir: pkgsysconfdir,
)
install_data('edit.indent.rc', install_dir: pkgsysconfdir, install_mode: 'rwxr-xr-x')
install_symlink('keymap.ini', pointing_to: 'keymap.default.ini', install_dir: pkgsysconfdir)

configure_file(
  input: 'mcedit.menu.in',
  output: 'mcedit.menu',
  configuration: {'MAN_FLAGS': man_flags},
  install_dir: pkgsysconfdir,
)
configure_file(
  input: 'extensions.ini.in',
  output: 'extensions.ini',
  configuration: {'EXTHELPERSDIR': exthelpersdir},
  install_dir: pkgsysconfdir,
)

# the rules of magic.ini for the plugins that are built; a ''' string takes no escapes,
# so "\\." lands in the file as it stands, as in the rules of the file itself
magic_rules = {}
magic_rules += {'MAGIC_ARCMC_RULES': not libarchive.found() ? '' : '''[arcmc.archive]
Regex=\\.(tar|tar\\.gz|tgz|tar\\.bz2|tbz2|tar\\.xz|txz|tar\\.zst|tzst|zip|jar|war|ear|7z|cpio)$
RegexIgnoreCase=true
Open=%plugin{arcmc:open}
View=%plugin{arcmc:view}'''}
# pictures go to lua-sixel, which draws them with chafa where the terminal has no sixel
magic_rules += {'MAGIC_LUA_CHAFA_RULES': not (lua.found() and libmagic.found()) ? '' : '''[image]
Mime=^image/
View=%plugin{lua(lua-sixel):view}'''}
magic_rules += {'MAGIC_LUA_PDF_RULES': not (lua.found() and libmagic.found()) ? '' : '''[pdf]
Mime=^application/pdf
Open=%plugin{lua(lua-pdf):open}
View=%plugin{lua(lua-pdf):view}'''}
magic_rules += {'MAGIC_LUA_JAVA_CLASS_RULES': not have_lua_java_class ? '' : '''[javaclass]
Shell=.class
View=%plugin{lua(lua-java-class):view}'''}
magic_rules += {'MAGIC_LUA_DBF_RULES': not lua.found() ? '' : '''[dbf]
Regex=\\.dbf$
RegexIgnoreCase=true
Open=%plugin{lua(lua-dbf):open}
View=%plugin{lua(lua-dbf):view}'''}
magic_rules += {'MAGIC_LUA_MARKDOWN_RULES': not lua.found() ? '' : '''[markdown]
Regex=\\.(mk?d|markdown)$
RegexIgnoreCase=true
View=%plugin{lua(lua-markdown):view}'''}
magic_rules += {'MAGIC_LUA_READELF_RULES': not (have_lua_readelf and libmagic.found()) ? '' : '''[elf]
Mime=^application/x-(pie-executable|executable|sharedlib|object)$
View=%plugin{lua(lua-readelf):view}'''}
magic_rules += {'MAGIC_SQLITE_RULES': not sqlite3.found() ? '' : '''[sqlite.database]
Type=^SQLite 3\\.x database
Open=%plugin{sqlite:open}
View=%plugin{sqlite:view}

[sqlite.database-name]
Regex=\\.(sqlite|sqlite3|sqlitedb)$
RegexIgnoreCase=true
Open=%plugin{sqlite:open}
View=%plugin{sqlite:view}'''}
configure_file(
  input: 'magic.ini.in',
  output: 'magic.ini',
  configuration: magic_rules,
  install_dir: pkgsysconfdir,
)
