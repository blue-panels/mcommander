project('mcommander', 'c',
  version: run_command('version.sh', meson.project_source_root(), '--short',
                       check: true).stdout().strip(),
  license: 'GPL-3.0-or-later',
  meson_version: '>= 0.61',
  default_options: [
    'c_std=gnu11',
    'warning_level=2',
    'buildtype=debugoptimized',
    # the layout of the autotools build: plugins and libmc under PREFIX/lib, not
    # the multiarch directory meson picks on Debian
    'libdir=lib',
  ],
)

cc = meson.get_compiler('c')
fs = import('fs')

# Linux and Android: the "linux*" host of autotools
is_linux = host_machine.system() in ['linux', 'android']

# PACKAGE: the gettext domain, the data paths and the name of the archive.
# PACKAGE_NAME lands in the middle of sentences such as "Type 'exit' to
# return to the %s", so it stays a short noun phrase.
package = meson.project_name()
package_name = 'M-Commander'
bugreport = 'https://github.com/blue-panels/mcommander/issues'
package_url = 'https://github.com/blue-panels/mcommander/wiki'

# ---------------------------------------------------------------------------------------------
# Directories

prefix = get_option('prefix')
bindir = prefix / get_option('bindir')
libdir = prefix / get_option('libdir')
datadir = prefix / get_option('datadir')
sysconfdir = prefix / get_option('sysconfdir')
libexecdir = prefix / get_option('libexecdir')
localedir = prefix / get_option('localedir')
mandir = prefix / get_option('mandir')
docdir = datadir / 'doc' / package

pkgdatadir = datadir / package
pkgsysconfdir = sysconfdir / package
pkglibexecdir = libexecdir / package
exthelpersdir = pkglibexecdir / 'ext.d'
extfshelpersdir = pkglibexecdir / 'extfs.d'

panel_plugins_dir = get_option('panel_plugins_dir')
if panel_plugins_dir == ''
  panel_plugins_dir = libdir / package / 'panel-plugins'
endif
editor_plugins_dir = get_option('editor_plugins_dir')
if editor_plugins_dir == ''
  editor_plugins_dir = libdir / package / 'editor-plugins'
endif
runtime_plugins_dir = get_option('runtime_plugins_dir')
if runtime_plugins_dir == ''
  runtime_plugins_dir = libdir / package / 'runtime-plugins'
endif
lua_scripts_dir = get_option('lua_scripts_dir')
if lua_scripts_dir == ''
  lua_scripts_dir = pkgdatadir / 'lua' / 'scripts'
endif
lua_modules_dir = get_option('lua_modules_dir')
if lua_modules_dir == ''
  lua_modules_dir = pkgdatadir / 'lua' / 'lib'
endif

# ---------------------------------------------------------------------------------------------
# Compiler flags

add_project_arguments(cc.get_supported_arguments([
    '-fdiagnostics-show-option',
    '-Wattributes',
    '-Wimplicit-function-declaration',
    '-Wimplicit-int',
    '-Wno-declaration-after-statement',
    '-Wno-long-long',
    '-Wno-vla',
    '-Wincompatible-pointer-types',
    '-Wint-conversion',
    '-Wbad-function-cast',
    '-Wconditional-uninitialized',
    '-Wfloat-conversion',
    '-Wfloat-equal',
    '-Wformat-security',
    '-Wformat-signedness',
    '-Wimplicit-fallthrough',
    '-Wmissing-declarations',
    '-Wmissing-format-attribute',
    '-Wmissing-prototypes',
    '-Wmissing-variable-declarations',
    '-Wnested-externs',
    '-Wpointer-arith',
    '-Wredundant-decls',
    '-Wshadow',
    '-Wstrict-prototypes',
    '-Wswitch-default',
    '-Wundef',
    '-Wunreachable-code',
    '-Wunused-result',
    '-Wwrite-strings',
    # casts like (GCompareDataFunc) with a missing parameter
    '-Wno-cast-function-type',
    # https://github.com/llvm/llvm-project/issues/20574
    '-Wno-assign-enum',
  ]),
  language: 'c')

# the sources include "lib/...", "src/..." and <config.h> from the top of the tree
add_project_arguments('-I' + meson.project_build_root(), '-I' + meson.project_source_root(),
                      language: 'c')

# ---------------------------------------------------------------------------------------------
# Dependencies

conf = configuration_data()

glib = dependency('glib-2.0', version: '>= 2.58')

gmodule = dependency('gmodule-no-export-2.0', version: '>= 2.58', required: false)
if not gmodule.found()
  gmodule = dependency('gmodule-2.0', version: '>= 2.58', required: false)
endif
have_gmodule = gmodule.found() and gmodule.get_variable('gmodule_supported', default_value: 'false') == 'true'
conf.set('HAVE_GMODULE', have_gmodule ? 1 : false)
glib_dep = have_gmodule ? [glib, gmodule] : [glib]

screen_deps = []
if get_option('screen') == 'slang'
  slang = dependency('slang', version: '>= 2.0', required: false)
  if not slang.found()
    slang = cc.find_library('slang')
  endif
  screen_deps += slang
  conf.set('HAVE_SLANG', 1)
  screen_msg = 'S-Lang'
  # a S-Lang built for termcap needs a library that provides tgoto
  if not cc.links('''#include <slang.h>
                     int main (void) { SLtt_get_terminfo (); SLtt_tgetflag ((char *) ""); return 0; }''',
                  dependencies: slang, name: 'S-Lang uses terminfo')
    termcap = cc.find_library('termcap', required: false)
    if not termcap.found()
      termcap = cc.find_library('curses', required: true)
    endif
    screen_deps += termcap
    conf.set('USE_TERMCAP', 1)
    screen_msg += ' with termcap database'
  endif
else
  ncurses = dependency('ncursesw', required: false)
  screen_msg = 'NCursesw'
  if not ncurses.found()
    ncurses = dependency('ncurses')
    screen_msg = 'NCurses'
  endif
  screen_deps += ncurses
  if not cc.has_function('stdscr', dependencies: ncurses, prefix: '#include <curses.h>')
    tinfo = dependency('tinfow', required: false)
    if not tinfo.found()
      tinfo = dependency('tinfo')
    endif
    screen_deps += tinfo
  endif
  conf.set('HAVE_NCURSES', 1)
  conf.set_quoted('NCURSES_LIB_DISPLAYNAME', screen_msg)
  conf.set('HAVE_NCURSES_WIDECHAR', cc.has_function('addwstr', dependencies: screen_deps) ? 1 : false)
  found = false
  foreach h : ['ncursesw/ncurses.h', 'ncursesw/curses.h', 'ncurses/ncurses.h', 'ncurses/curses.h',
               'ncurses.h', 'curses.h']
    if not found and cc.has_header(h, dependencies: screen_deps)
      conf.set('HAVE_' + h.underscorify().to_upper(), 1)
      found = true
    endif
  endforeach
  if not found
    error('NCurses(w) header file not found')
  endif
  found = false
  foreach h : ['ncursesw/term.h', 'ncurses/term.h', 'term.h']
    if not found and cc.has_header(h, dependencies: screen_deps)
      conf.set('HAVE_' + h.underscorify().to_upper(), 1)
      found = true
    endif
  endforeach
  if not found
    error('NCurses(w) term.h header file not found')
  endif
  # if ncurses exports ESCDELAY, it is set to 0: without it Esc has to be pressed three times
  conf.set('HAVE_ESCDELAY', cc.links('extern int ESCDELAY; int main (void) { ESCDELAY = 0; return 0; }',
                                     dependencies: screen_deps, name: 'ESCDELAY variable') ? 1 : false)
  conf.set('HAVE_RESIZETERM', cc.has_function('resizeterm', dependencies: screen_deps) ? 1 : false)
endif

gpm = dependency('', required: false)
if is_linux and not get_option('gpm').disabled()
  gpm = cc.find_library('gpm', required: get_option('gpm'))
  if gpm.found() and not cc.has_function('Gpm_Repeat', dependencies: gpm)
    if get_option('gpm').enabled()
      error('libgpm is missing or older than 0.18')
    endif
    gpm = dependency('', required: false)
  endif
endif
conf.set('HAVE_LIBGPM', gpm.found() ? 1 : false)

# the X server is only asked for the keyboard modifiers; with gmodule libX11 is opened at run time
x11 = dependency('x11', required: get_option('x11'))
x11_dep = []
if x11.found()
  conf.set('HAVE_TEXTMODE_X11_SUPPORT', 1)
  x11_dep = have_gmodule ? x11.partial_dependency(compile_args: true, includes: true) : x11
endif

# the terminal widget runs a shell on a pty; openpty() is in libc or in libutil
libutil = []
if not cc.has_function('openpty')
  libutil = cc.find_library('util', required: false)
  if not (libutil.found() and cc.has_function('openpty', dependencies: libutil))
    error('openpty() is required but was not found.\n' +
          'Install the appropriate development package (e.g. libutil-dev, libbsd-dev).')
  endif
endif
conf.set('HAVE_OPENPTY', 1)
conf.set('ENABLE_MCTERM', 1)
foreach h : ['pty.h', 'util.h', 'libutil.h']
  conf.set('HAVE_' + h.underscorify().to_upper(), cc.has_header(h) ? 1 : false)
endforeach
conf.set('HAVE_CLOSEFROM', cc.has_function('closefrom') ? 1 : false)

ext2fs_attr = dependency('', required: false)
ext2fs = dependency('ext2fs', version: '>= 1.42.4', required: get_option('ext2fs_attr'))
if ext2fs.found()
  e2p = dependency('e2p', version: '>= 1.42.4', required: get_option('ext2fs_attr'))
  if e2p.found()
    ext2fs_attr = declare_dependency(dependencies: [ext2fs.partial_dependency(compile_args: true), e2p])
  endif
endif
conf.set('ENABLE_EXT2FS_ATTR', ext2fs_attr.found() ? 1 : false)

lua = dependency('', required: false)
if have_gmodule and not get_option('lua').disabled()
  foreach name : ['lua5.4', 'lua-5.4', 'lua54', 'lua5.3', 'lua-5.3', 'lua53', 'lua']
    if not lua.found()
      lua = dependency(name, version: '>= 5.3', required: false)
    endif
  endforeach
endif
if get_option('lua').enabled() and not lua.found()
  error(have_gmodule ? 'Lua 5.3 or newer was not found' : 'the Lua plugin needs gmodule')
endif
conf.set('ENABLE_LUA_PLUGIN', lua.found() ? 1 : false)

have_session = have_gmodule and not get_option('session').disabled()
if get_option('session').enabled() and not have_gmodule
  error('the session plugin needs gmodule')
endif

# libmagic: the file types of the file operations, arcmc and the tree view
libmagic = dependency('libmagic', required: false)
if not libmagic.found() and not get_option('magic').disabled()
  libmagic = cc.find_library('magic', has_headers: ['magic.h'], required: false)
  if libmagic.found() and not cc.links('''#include <magic.h>
                                           int main (void) { magic_t c = magic_open (0); magic_close (c); return 0; }''',
                                       dependencies: libmagic, name: 'magic_open in -lmagic')
    libmagic = dependency('', required: false)
  endif
endif
if get_option('magic').disabled()
  libmagic = dependency('', required: false)
elif get_option('magic').enabled() and not libmagic.found()
  error('libmagic was not found. Install the libmagic development package (for example: libmagic-dev).')
endif
conf.set('HAVE_LIBMAGIC', libmagic.found() ? 1 : false)
conf.set('HAVE_MCTREE_MAGIC', libmagic.found() ? 1 : false)
if get_option('magic_file') == ''
  conf.set('MC_LIBMAGIC_FILE', 'NULL')
elif get_option('magic_file').startswith('/')
  conf.set_quoted('MC_LIBMAGIC_FILE', get_option('magic_file'))
else
  error('magic_file requires an absolute path')
endif

zlib = dependency('zlib', required: get_option('zlib'))
conf.set('HAVE_ZLIB', zlib.found() ? 1 : false)

# unit tests: the test programs run on the build machine
check = dependency('check', version: '>= 0.9.10',
                   required: get_option('tests').disable_auto_if(meson.is_cross_build()))
conf.set('HAVE_TESTS', check.found() ? 1 : false)

intl = dependency('intl', required: get_option('nls'))
msgfmt = find_program('msgfmt', required: get_option('nls'))
have_nls = intl.found() and msgfmt.found()
conf.set('ENABLE_NLS', have_nls ? 1 : false)

# ---------------------------------------------------------------------------------------------
# The plugins

libssh2 = dependency('libssh2', version: '>= 1.9.0', required: false)

have_panel_plugin_sftp = libssh2.found() and not get_option('panel_plugin_sftp').disabled()
if get_option('panel_plugin_sftp').enabled() and not have_panel_plugin_sftp
  error('SFTP panel plugin requested but libssh2 >= 1.9.0 was not found.')
endif

smbclient = dependency('smbclient', required: get_option('panel_plugin_samba'))
libacl = dependency('libacl', required: get_option('panel_plugin_acl'))

have_panel_plugin_shell_link = not get_option('panel_plugin_shell_link').disabled()
# without libssh2 the shell link plugin talks to an external ssh, which cannot
# do password authentication but handles keys, the agent and host key confirmation
have_shell_ssh2 = have_panel_plugin_shell_link and libssh2.found() \
  and not get_option('shell_ssh2').disabled()
if get_option('shell_ssh2').enabled() and have_panel_plugin_shell_link and not have_shell_ssh2
  error('Shell link libssh2 transport requested but libssh2 >= 1.9.0 was not found.')
endif
conf.set('ENABLE_SHELL_SSH2', have_shell_ssh2 ? 1 : false)

libcurl = dependency('libcurl', version: '>= 7.20.0', required: get_option('panel_plugin_ftp'))
if libcurl.found() and not cc.links('#include <curl/curl.h>\nint main (void) { curl_easy_init (); return 0; }',
                                    dependencies: libcurl, name: 'usable libcurl')
  if get_option('panel_plugin_ftp').enabled()
    error('FTP panel plugin requested but usable libcurl headers/libs were not found.')
  endif
  libcurl = dependency('', required: false)
endif

# S3 needs CURLOPT_AWS_SIGV4
libcurl_s3 = dependency('libcurl', version: '>= 7.75.0', required: get_option('panel_plugin_s3'))

libarchive = dependency('', required: false)
if not get_option('panel_plugin_arcmc').disabled()
  libarchive = dependency('libarchive', version: '>= 3.0', required: false)
  # FreeBSD carries libarchive in the base system and ships no pkg-config file for it
  if not libarchive.found()
    libarchive = cc.find_library('archive', has_headers: ['archive.h'], required: false)
    if libarchive.found() and not cc.has_function('archive_read_free', dependencies: libarchive)
      libarchive = dependency('', required: false)
    endif
  endif
  if get_option('panel_plugin_arcmc').enabled() and not libarchive.found()
    error('Archive browser panel plugin requested but libarchive >= 3.0 was not found.')
  endif
endif

# driver 2 renamed its pkg-config modules to mongoc2 and bson2; either generation will do
mongoc = dependency('', required: false)
if not get_option('panel_plugin_mongo').disabled()
  mongoc1 = dependency('libmongoc-1.0', version: '>= 1.21', required: false)
  bson1 = dependency('libbson-1.0', version: '>= 1.21', required: false)
  if mongoc1.found() and bson1.found()
    mongoc = declare_dependency(dependencies: [mongoc1, bson1])
  else
    mongoc2 = dependency('mongoc2', version: '>= 2.0', required: false)
    bson2 = dependency('bson2', version: '>= 2.0', required: false)
    if mongoc2.found() and bson2.found()
      mongoc = declare_dependency(dependencies: [mongoc2, bson2])
    endif
  endif
  if get_option('panel_plugin_mongo').enabled() and not mongoc.found()
    error('MongoDB panel plugin requested but no MongoDB C driver was found:\n' +
          'neither libmongoc-1.0 >= 1.21 with libbson-1.0 >= 1.21, nor mongoc2 >= 2.0 with bson2 >= 2.0.')
  endif
endif

sqlite3 = dependency('sqlite3', version: '>= 3.26', required: get_option('panel_plugin_sqlite'))

# the external tools some Lua viewers run
chafa = find_program('chafa', required: false)
procyon = find_program('procyon', required: false)
script_tool = find_program('script', required: false)
env_tool = find_program('env', required: false)
readelf = find_program('readelf', required: false)
have_lua_chafa = lua.found() and chafa.found()
have_lua_java_class = lua.found() and procyon.found() and script_tool.found() and env_tool.found()
have_lua_readelf = lua.found() and readelf.found()

# ---------------------------------------------------------------------------------------------
# Programs the build and the installed scripts use

perl = find_program('perl', required: false)
python3 = find_program('python3', required: false)
ruby = find_program('ruby', required: false)
perl_path = perl.found() ? perl.full_path() : '/usr/bin/env perl'
python_path = python3.found() ? python3.full_path() : '/usr/bin/env python3'
ruby_path = ruby.found() ? ruby.full_path() : '/usr/bin/env ruby'

# the file command: -z is a must, -b, -L and -S are used when it takes them
file_cmd = find_program('file', required: false)
if file_cmd.found() and run_command(file_cmd, '-z', '.', check: false).returncode() == 0
  conf.set('USE_FILE_CMD', 1)
  if run_command(file_cmd, '-b', '.', check: false).returncode() == 0
    conf.set_quoted('FILE_B', '-b ')
  endif
  conf.set_quoted('FILE_L', run_command(file_cmd, '-L', '.', check: false).returncode() == 0 ? '-L ' : '')
  # since file 5.33
  conf.set_quoted('FILE_S', run_command(file_cmd, '-S', '.', check: false).returncode() == 0 ? '-S ' : '')
elif file_cmd.found()
  warning('The file command does not accept the -z option and will not be used')
endif

# the man pages shown by the help of the viewer
nroff = find_program('nroff', required: false)
mandoc = '-man'
man_flags = ''
nroff_utf8 = ''
nroff_8bit = ''
if nroff.found()
  if run_command(nroff, '-mandoc', '/dev/null', check: false).returncode() == 0
    mandoc = '-mandoc'
  endif
  if run_command(nroff, '-c', '/dev/null', check: false).returncode() == 0
    man_flags = '-c'
  endif
  if run_command('sh', '-c', 'echo x | preconv 2>/dev/null | nroff -Tutf8 >/dev/null 2>&1',
                 check: false).returncode() == 0
    nroff_utf8 = '-Tutf8'
  endif
  if run_command(nroff, '-Tlatin1', '/dev/null', check: false).returncode() == 0
    nroff_8bit = '-Tlatin1'
  elif run_command(nroff, '-Tascii', '/dev/null', check: false).returncode() == 0
    nroff_8bit = '-Tascii'
  endif
endif

# ---------------------------------------------------------------------------------------------
# config.h

conf.set_quoted('PACKAGE', package)
conf.set_quoted('PACKAGE_NAME', package_name)
conf.set_quoted('PACKAGE_TARNAME', package)
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_STRING', package_name + ' ' + meson.project_version())
conf.set_quoted('PACKAGE_BUGREPORT', bugreport)
conf.set_quoted('PACKAGE_URL', package_url)

# AC_USE_SYSTEM_EXTENSIONS
foreach m : ['_GNU_SOURCE', '_ALL_SOURCE', '_DARWIN_C_SOURCE', '__EXTENSIONS__', '_NETBSD_SOURCE',
             '_OPENBSD_SOURCE', '_POSIX_PTHREAD_SEMANTICS', '_TANDEM_SOURCE',
             '_HPUX_ALT_XOPEN_SOCKET_API']
  conf.set(m, 1)
endforeach
# AC_SYS_LARGEFILE
if cc.sizeof('off_t', prefix: '#include <sys/types.h>') < 8
  conf.set('_FILE_OFFSET_BITS', 64)
endif

if not get_option('assert')
  conf.set('G_DISABLE_ASSERT', 1)
endif
if get_option('debug')
  conf.set('USE_MAINTAINER_MODE', 1)
endif
if host_machine.system() == 'aix'
  conf.set('IS_AIX', 1)
endif
# read(1) on a pty can return 0 for a fd that is not closed
if host_machine.system() == 'os400'
  conf.set('PTY_ZEROREAD', 1)
endif

conf.set('USE_INTERNAL_EDIT', get_option('internal_edit') ? 1 : false)
# spell checking loads aspell or hunspell at run time
conf.set('HAVE_ASPELL', get_option('internal_edit') and have_gmodule ? 1 : false)
conf.set('USE_DIFF_VIEW', get_option('diff_viewer') ? 1 : false)
conf.set('ENABLE_BACKGROUND', get_option('background') ? 1 : false)

have_vfs = get_option('vfs')
have_vfs_extfs = have_vfs and get_option('vfs_extfs')
have_vfs_sfs = have_vfs and get_option('vfs_sfs')
conf.set('ENABLE_VFS', have_vfs ? 1 : false)
conf.set('ENABLE_VFS_EXTFS', have_vfs_extfs ? 1 : false)
conf.set('ENABLE_VFS_SFS', have_vfs_sfs ? 1 : false)
vfs_flags = []
if have_vfs_extfs
  vfs_flags += 'extfs'
endif
if have_vfs_sfs
  vfs_flags += 'sfs'
endif

foreach h : ['string.h', 'memory.h', 'limits.h', 'malloc.h', 'utime.h', 'sys/statfs.h',
             'sys/vfs.h', 'sys/select.h', 'sys/ioctl.h', 'stropts.h', 'arpa/inet.h',
             'sys/socket.h', 'sys/inotify.h', 'sys/timerfd.h', 'fcntl.h', 'gnu/libc-version.h']
  conf.set('HAVE_' + h.underscorify().to_upper(), cc.has_header(h) ? 1 : false)
endforeach
# FICLONE, the clone of a file on btrfs
if is_linux
  conf.set('HAVE_LINUX_FS_H', cc.has_header('linux/fs.h') ? 1 : false)
endif

# in the order of AC_HEADER_MAJOR: Solaris has a sys/sysmacros.h too, but
# makedev() is in sys/mkdev.h there
if cc.has_header_symbol('sys/mkdev.h', 'makedev')
  conf.set('MAJOR_IN_MKDEV', 1)
elif cc.has_header_symbol('sys/sysmacros.h', 'makedev')
  conf.set('MAJOR_IN_SYSMACROS', 1)
endif

foreach f : ['strverscmp', 'strncasecmp', 'realpath', 'utimensat', 'setlocale',
             'statlstat', 'getpwent', 'getgrent']
  conf.set('HAVE_' + f.to_upper(), cc.has_function(f) ? 1 : false)
endforeach
# Bionic has no getpwent and getgrent before Android 8 (API 26)

conf.set('HAVE_STRUCT_DIRENT_D_TYPE', cc.has_member('struct dirent', 'd_type', prefix: '#include <dirent.h>') ? 1 : false)

conf.set('HAVE_SIGHANDLER_T', cc.has_type('sighandler_t', prefix: '#define _GNU_SOURCE\n#include <signal.h>') ? 1 : false)
sig_atomic_h = '#include <signal.h>'
if run_command('sh', '-c', 'echo "@0@" | @1@ -E - | grep -q "volatile.*sig_atomic_t"'.format(
                 sig_atomic_h, ' '.join(cc.cmd_array())), check: false).returncode() == 0
  conf.set('SIG_ATOMIC_VOLATILE_T', 'sig_atomic_t')
else
  conf.set('SIG_ATOMIC_VOLATILE_T', 'volatile sig_atomic_t')
endif
conf.set10('HAVE_DECL_ENVIRON', cc.has_header_symbol('unistd.h', 'environ', prefix: '#define _GNU_SOURCE'))

foreach m : ['st_blksize', 'st_blocks', 'st_rdev', 'st_mtim', 'st_mtimespec', 'st_mtimensec']
  conf.set('HAVE_STRUCT_STAT_' + m.to_upper(),
           cc.has_member('struct stat', m, prefix: '#include <sys/stat.h>') ? 1 : false)
endforeach

# the type a mode_t becomes when it is passed through "..."
if cc.sizeof('mode_t', prefix: '#include <sys/types.h>') < cc.sizeof('int')
  conf.set('PROMOTED_MODE_T', 'int')
else
  conf.set('PROMOTED_MODE_T', 'mode_t')
endif

# ---------------------------------------------------------------------------------------------
# The list of mounted file systems and their usage, after gnulib

# ---------------------------------------------------------------------------------------------
# File systems: the list of the mounted ones, their usage and their type (gnulib mountlist,
# fsusage and fstypename, as m4.include/mc-get-fs-info.m4 runs them)

# libraries the getmntent() of Unixware needs; empty elsewhere
fsinfo_deps = []
fs_h = {}

foreach h : ['sys/param.h', 'sys/statvfs.h', 'sys/vfs.h', 'sys/fs_types.h', 'mntent.h',
             'sys/mntent.h', 'sys/statfs.h', 'sys/fs/s5param.h', 'fcntl.h', 'utime.h',
             'netinet/in.h', 'OS.h']
  fs_h += {h: cc.has_header(h)}
  conf.set('HAVE_' + h.underscorify().to_upper(), fs_h[h] ? 1 : false)
endforeach
fs_h += {'sys/ucred.h': cc.has_header('sys/ucred.h', prefix: '#include <grp.h>')}
conf.set('HAVE_SYS_UCRED_H', fs_h['sys/ucred.h'] ? 1 : false)

# the includes the checks below share, as the found headers allow
fs_param_inc = fs_h['sys/param.h'] ? '#include <sys/param.h>\n' : ''
have_sys_mount_h = cc.has_header('sys/mount.h', prefix: '#include <sys/types.h>\n' + fs_param_inc)
conf.set('HAVE_SYS_MOUNT_H', have_sys_mount_h ? 1 : false)
fs_mount_inc = have_sys_mount_h ? '#include <sys/mount.h>\n' : ''
fs_vfs_inc = fs_h['sys/vfs.h'] ? '#include <sys/vfs.h>\n' : ''
fs_statvfs_inc = fs_h['sys/statvfs.h'] ? '#include <sys/statvfs.h>\n' : ''

# --- mountlist: how to read the list of mounted file systems

getfsstat_inc = fs_param_inc + '#include <sys/types.h>\n' + fs_mount_inc
if fs_h['sys/ucred.h']
  getfsstat_inc = '#include <grp.h>\n#include <sys/ucred.h>\n' + getfsstat_inc
endif
if fs_h['sys/fs_types.h']
  getfsstat_inc += '#include <sys/fs_types.h>\n'
endif
have_fsstat_fstypename = cc.has_member('struct fsstat', 'f_fstypename', prefix: getfsstat_inc)
conf.set('HAVE_STRUCT_FSSTAT_F_FSTYPENAME', have_fsstat_fstypename ? 1 : false)

# getmntent is in libc on most systems, in libgen on Unixware
have_getmntent = cc.has_function('getmntent')
if not have_getmntent
  libgen = cc.find_library('gen', required: false)
  if libgen.found() and cc.has_function('getmntent', dependencies: libgen)
    have_getmntent = true
    fsinfo_deps += libgen
  endif
endif
conf.set('HAVE_GETMNTENT', have_getmntent ? 1 : false)

mounted_fs = ''

# AIX
if mounted_fs == '' and cc.has_header('fshelp.h')
  mounted_fs = 'MOUNTED_VMOUNT'
endif

if have_getmntent
  # glibc, musl, Cygwin, Android
  if mounted_fs == '' and cc.compiles('''#include <stdio.h>
#include <mntent.h>
#if defined __ANDROID__
# undef MOUNTED
# define MOUNTED "/proc/mounts"
#elif !defined MOUNTED
# if defined _PATH_MOUNTED
#  define MOUNTED _PATH_MOUNTED
# endif
# if defined MNT_MNTTAB
#  define MOUNTED MNT_MNTTAB
# endif
#endif
int main (void) { struct mntent *mnt = 0; char *table = MOUNTED;
                  if (sizeof mnt && sizeof table) return 0; return 0; }''',
                                      name: 'one-argument getmntent')
    mounted_fs = 'MOUNTED_GETMNTENT1'
    foreach f : ['setmntent', 'endmntent', 'hasmntopt']
      conf.set('HAVE_' + f.to_upper(), cc.has_function(f, dependencies: fsinfo_deps) ? 1 : false)
    endforeach
  endif
  # Solaris >= 8
  if mounted_fs == '' and cc.has_header_symbol('sys/mnttab.h', 'getextmntent')
    mounted_fs = 'MOUNTED_GETEXTMNTENT'
  endif
  # Solaris < 8, SVR4
  if mounted_fs == '' and cc.has_header_symbol('sys/mnttab.h', 'getmntent')
    mounted_fs = 'MOUNTED_GETMNTENT2'
    conf.set('HAVE_HASMNTOPT', cc.has_function('hasmntopt', dependencies: fsinfo_deps) ? 1 : false)
  endif
endif

# OSF/1
if have_fsstat_fstypename
  getfsstat_inc = '#define HAVE_STRUCT_FSSTAT_F_FSTYPENAME 1\n' + getfsstat_inc
endif
if mounted_fs == '' and cc.links(getfsstat_inc + '''
#if HAVE_STRUCT_FSSTAT_F_FSTYPENAME
# define FS_TYPE(Ent) ((Ent).f_fstypename)
#else
# define FS_TYPE(Ent) mnt_names[(Ent).f_type]
#endif
int main (void) { struct statfs *stats; int numsys = getfsstat ((struct statfs *) 0, 0L, MNT_WAIT);
                  char *t = FS_TYPE (*stats); (void) numsys; (void) t; return 0; }''',
                                 name: 'getfsstat function')
  mounted_fs = 'MOUNTED_GETFSSTAT'
endif

# SVR3: <sys/fstyp.h> and <sys/statfs.h> without a function to read the list; left out

# macOS, FreeBSD, NetBSD, OpenBSD; the function has other names at the link level, so it is
# looked for by a call through the headers
getmntinfo_inc = fs_param_inc + '#include <sys/types.h>\n' + fs_mount_inc + fs_statvfs_inc
if mounted_fs == '' and cc.links(getmntinfo_inc + '#include <stdlib.h>\n' +
                                 'int main (void) { int count = getmntinfo (NULL, MNT_WAIT); (void) count; return 0; }',
                                 name: 'getmntinfo function')
  # NetBSD returns statvfs structures: a declaration with statfs does not compile there
  if cc.compiles(getmntinfo_inc + 'extern int getmntinfo (struct statfs **, int);',
                 name: 'getmntinfo returns statfs structures')
    mounted_fs = 'MOUNTED_GETMNTINFO'
  else
    mounted_fs = 'MOUNTED_GETMNTINFO2'
  endif
endif

# Haiku, BeOS
if mounted_fs == '' and cc.has_header('fs_info.h') and cc.has_function('next_dev') and cc.has_function('fs_stat_dev')
  mounted_fs = 'MOUNTED_FS_STAT_DEV'
endif

# Interix: left out

have_infomount_list = mounted_fs != ''
if mounted_fs != ''
  conf.set(mounted_fs, 1)
elif host_machine.system() != 'windows'
  conf.set('MOUNTED_NOT_PORTED', 1)
endif
conf.set('HAVE_INFOMOUNT_LIST', have_infomount_list ? 1 : false)

have_statfs_fstypename = cc.has_member('struct statfs', 'f_fstypename',
  prefix: '#include <sys/types.h>\n#include <sys/param.h>\n#include <sys/mount.h>')
conf.set('HAVE_STRUCT_STATFS_F_FSTYPENAME', have_statfs_fstypename ? 1 : false)

# --- fsusage: how to read the size and the free space of a file system

fsusage_found = false

# glibc, musl, bionic, FreeBSD, NetBSD, OpenBSD, Solaris, Cygwin; not macOS, where f_blocks
# of struct statvfs has 32 bits
have_statvfs = cc.links('''#include <sys/types.h>
#include <sys/statvfs.h>
struct statvfs fsd;
#if defined __APPLE__ && defined __MACH__
#include <limits.h>
int check_f_blocks_size[sizeof fsd.f_blocks * CHAR_BIT <= 32 ? -1 : 1];
#endif
int main (void) { statvfs (0, &fsd); return 0; }''', name: 'statvfs function')
have_statvfs64 = false
if have_statvfs
  fsusage_found = true
  # AIX: statvfs64 has a wider f_blocks
  have_statvfs64 = cc.links('''#include <sys/types.h>
#include <sys/statvfs.h>
struct statvfs64 fsd;
int check_f_blocks_larger_in_statvfs64
  [sizeof (((struct statvfs64 *) 0)->f_blocks) > sizeof (((struct statvfs *) 0)->f_blocks) ? 1 : -1];
int main (void) { statvfs64 (0, &fsd); return 0; }''', name: 'statvfs64 is wider')
  conf.set(have_statvfs64 ? 'STAT_STATVFS64' : 'STAT_STATVFS', 1)
endif

# a statfs() run on "."; never when cross compiling, as autoconf does
fs_statfs_inc = fs_param_inc + fs_mount_inc + fs_vfs_inc
fs_run_ok = {}
foreach name, src : {
    'STAT_STATFS2_FRSIZE': fs_statfs_inc + '''int main (void) { struct statfs fsd; fsd.f_frsize = 0;
                                                     return statfs (".", &fsd) != 0; }''',
    'STAT_STATFS3_OSF1': '''#include <sys/param.h>
#include <sys/types.h>
#include <sys/mount.h>
int main (void) { struct statfs fsd; fsd.f_fsize = 0; return statfs (".", &fsd, sizeof (struct statfs)) != 0; }''',
    'STAT_STATFS2_BSIZE': fs_statfs_inc + '''int main (void) { struct statfs fsd; fsd.f_bsize = 0;
                                                    return statfs (".", &fsd) != 0; }''',
    'STAT_STATFS4': '''#include <sys/types.h>
#include <sys/statfs.h>
int main (void) { struct statfs fsd; return statfs (".", &fsd, sizeof fsd, 0) != 0; }''',
    'STAT_STATFS2_FSIZE': '#include <sys/types.h>\n' + fs_param_inc + fs_mount_inc +
                          '''int main (void) { struct statfs fsd; fsd.f_fsize = 0;
                                               return statfs (".", &fsd) != 0; }''',
  }
  ok = false
  if not meson.is_cross_build()
    r = cc.run(src, name: name.to_lower())
    ok = r.compiled() and r.returncode() == 0
  endif
  fs_run_ok += {name: ok}
endforeach

# checked always: the fallback of glibc/Linux > 2.6 < 2.6.36
if fs_run_ok['STAT_STATFS2_FRSIZE']
  fsusage_found = true
  conf.set('STAT_STATFS2_FRSIZE', 1)
endif
foreach name : ['STAT_STATFS3_OSF1', 'STAT_STATFS2_BSIZE', 'STAT_STATFS4', 'STAT_STATFS2_FSIZE']
  if not fsusage_found and fs_run_ok[name]
    fsusage_found = true
    conf.set(name, 1)
  endif
endforeach

# --- the type of a file system, by statvfs or by statfs (mc_cu_PREREQ_STAT_PROG)

statvfs_inc = '#include <sys/types.h>\n#include <sys/statvfs.h>\n'
statfs_inc = '#include <sys/types.h>\n'
if fs_h['sys/vfs.h']
  statfs_inc += '#include <sys/vfs.h>\n'
elif have_sys_mount_h and fs_h['sys/param.h']
  statfs_inc += '#include <sys/param.h>\n#include <sys/mount.h>\n'
elif fs_h['OS.h']
  statfs_inc += '#include <fs_info.h>\n'
endif

vfs_basetype = false
vfs_fstypename = false
vfs_type = false
use_statvfs = false
if have_statvfs
  vfs_basetype = cc.has_member('struct statvfs', 'f_basetype', prefix: statvfs_inc)
  if not vfs_basetype
    vfs_fstypename = cc.has_member('struct statvfs', 'f_fstypename', prefix: statvfs_inc)
    if not vfs_fstypename and not have_statfs_fstypename
      vfs_type = cc.has_member('struct statvfs', 'f_type', prefix: statvfs_inc)
    endif
  endif
  use_statvfs = vfs_basetype or vfs_fstypename or vfs_type
endif
conf.set('HAVE_STRUCT_STATVFS_F_BASETYPE', vfs_basetype ? 1 : false)
conf.set('HAVE_STRUCT_STATVFS_F_FSTYPENAME', vfs_fstypename ? 1 : false)
conf.set('HAVE_STRUCT_STATVFS_F_TYPE', vfs_type ? 1 : false)
conf.set('USE_STATVFS', use_statvfs ? 1 : false)
if not use_statvfs
  foreach m : ['f_namelen', 'f_type', 'f_frsize']
    conf.set('HAVE_STRUCT_STATFS_' + m.to_upper(),
             cc.has_member('struct statfs', m, prefix: statfs_inc) ? 1 : false)
  endforeach
endif

# old glibc has a broken posix_fallocate()
conf.set('HAVE_POSIX_FALLOCATE', cc.links('''#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <fcntl.h>
#if defined(__GLIBC__) && (__GLIBC__ < 2 || __GLIBC_MINOR__ < 7)
  possibly broken posix_fallocate
#endif
int main (void) { posix_fallocate (0, 0, 0); return 0; }''', name: 'working posix_fallocate') ? 1 : false)

# lib/util.c picks the code for a 64-bit uintmax_t
conf.set('SIZEOF_UINTMAX_T', cc.sizeof('uintmax_t', prefix: '#include <stdint.h>'))

conf.set('ENABLE_CONFIGURE_ARGS', 1)
conf.set_quoted('MC_CONFIGURE_ARGS', ' '.join([
  'buildtype=' + get_option('buildtype'),
  'screen=' + get_option('screen'),
  'mclib=' + (get_option('mclib') ? 'yes' : 'no'),
  'gpm=' + (gpm.found() ? 'yes' : 'no'),
  'x11=' + (x11.found() ? 'yes' : 'no'),
  'lua=' + (lua.found() ? 'yes' : 'no'),
  'magic=' + (libmagic.found() ? 'yes' : 'no'),
  'nls=' + (have_nls ? 'yes' : 'no'),
]))

configure_file(output: 'config.h', configuration: conf)

# the full version, "git describe", made at build time so that a new commit rebuilds one file
version_h = vcs_tag(
  command: [meson.project_source_root() / 'version.sh', meson.project_source_root()],
  input: 'mc-version.h.in',
  output: 'mc-version.h',
  fallback: meson.project_version(),
)

# names of the encodings in the iconv of the system
if host_machine.system() in ['sunos', 'solaris']
  encoding_cp1251 = 'ANSI-1251'
else
  encoding_cp1251 = 'CP1251'
endif
if cc.has_header('gnu/libc-version.h')
  encoding_cp866 = 'IBM866'
  encoding_iso8859 = 'ISO-8859'
else
  encoding_cp866 = 'CP866'
  encoding_iso8859 = 'ISO8859'
endif
encoding_conf = configuration_data({
  'ENCODING_CP1251': encoding_cp1251,
  'ENCODING_CP866': encoding_cp866,
  'ENCODING_ISO8859': encoding_iso8859,
})

top_inc = include_directories('.')

# the languages the man page and the help are translated to
doc_languages = ['es', 'hu', 'it', 'pl', 'ru', 'sr']

# ---------------------------------------------------------------------------------------------

subdir('lib')
subdir('src')
subdir('contrib')
subdir('misc')
subdir('doc')
if have_nls
  subdir('po')
endif
if check.found()
  subdir('tests')
endif

meson.add_dist_script('maint/meson-dist.sh')

# maintainer targets: "meson compile -C <builddir> <name>"
run_target('indent', command: ['maint/indent.sh', meson.project_source_root()])
# the roff of the man pages, made from their markdown before a commit; needs go-md2man
run_target('man-in', command: ['maint/update-man-in.sh', meson.project_source_root()])
run_target('check-glib-api', command: ['sh', meson.project_source_root() / 'maint/check-glib-api.sh'])
run_target('buildcheck', command: [meson.project_source_root() / 'maint/buildcheck.sh'])

summary({
  'Screen library': screen_msg,
  'Mouse': gpm.found() ? 'gpm and xterm' : 'xterm only',
  'X11 events': x11.found(),
  'Shared libmc': get_option('mclib'),
  'Internal editor': get_option('internal_edit'),
  'Diff viewer': get_option('diff_viewer'),
  'Background operations': get_option('background'),
  'Virtual file system': have_vfs ? ', '.join(vfs_flags) : false,
  'ext2fs attributes': ext2fs_attr.found(),
  'libmagic': libmagic.found(),
  'Lua runtime plugin': lua.found() ? lua.version() : false,
  'Session plugin': have_session,
  'Translations': have_nls,
  'Unit tests': check.found(),
  'Assertions': get_option('assert'),
}, bool_yn: true, section: 'Core')

summary({
  'sftp': have_panel_plugin_sftp,
  'samba': smbclient.found(),
  'acl': libacl.found(),
  'shell-link': have_panel_plugin_shell_link,
  'shell-link over libssh2': have_shell_ssh2,
  'ftp': libcurl.found(),
  'arcmc': libarchive.found(),
  's3': libcurl_s3.found(),
  'mongo': mongoc.found(),
  'sqlite': sqlite3.found(),
}, bool_yn: true, section: 'Panel plugins')
