# The panel plugins: modules the program opens at run time. They call the core
# through the program (export_dynamic), so they do not link libmc.
# Every module is named <name>.so, on macOS too, as libtool named them.

# the headers of the screen library, which lib/tty/tty.h includes; not the library itself
plugin_screen_headers = []
foreach d : screen_deps
  plugin_screen_headers += d.partial_dependency(compile_args: true, includes: true)
endforeach
plugin_deps = [glib_dep, plugin_screen_headers]

subdir('systemd')
subdir('hello')
subdir('panelize')
subdir('mcstruct')
subdir('skineditor')
subdir('mcpeek')
if have_panel_plugin_sftp
  subdir('sftp')
endif
if smbclient.found()
  subdir('samba')
endif
if libacl.found()
  subdir('acl')
endif
if libcurl.found()
  subdir('ftp')
endif
if libarchive.found()
  subdir('arcmc')
endif
if libcurl_s3.found()
  subdir('s3')
endif
if mongoc.found()
  subdir('mongo')
endif
if sqlite3.found()
  subdir('sqlite')
endif
subdir('docker')
subdir('git')
subdir('k8s')
if have_panel_plugin_shell_link
  subdir('shell-link')
endif
