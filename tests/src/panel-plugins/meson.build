pp_src = meson.project_source_root() / 'src' / 'panel-plugins'

subdir('ftp')
subdir('arcmc')
subdir('samba')
subdir('docker')
subdir('git')
subdir('pp_common')
subdir('mcstruct')
subdir('skineditor')
if mongoc.found()
  subdir('mongo')
endif
if sqlite3.found()
  subdir('sqlite')
endif
if have_panel_plugin_shell_link
  subdir('shell-link')
endif
if libacl.found()
  subdir('acl')
endif
