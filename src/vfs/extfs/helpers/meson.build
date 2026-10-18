# The extfs helper scripts. Those made from *.in get the paths of the tools of
# this system.

# what AC_PROG_AWK finds; the scripts get its name, not its path
awk_prog = find_program('gawk', 'mawk', 'nawk', 'awk')
zip_prog = find_program('zip', required: false)
unzip_prog = find_program('unzip', required: false)
unzip_path = unzip_prog.found() ? unzip_prog.full_path() : '/usr/bin/unzip'
# the zipinfo code in unzip: "unzip -Z"
have_zipinfo = unzip_prog.found() and \
  run_command('sh', '-c', '"$0" -Z </dev/null >/dev/null 2>&1', unzip_path, check: false).returncode() == 0

# sed, as config.status does: configure_file() of meson would also turn "\@" into "@",
# and mailfs needs the backslash in its regular expressions
extfs_subst = []
foreach var, value : {
    'AWK': fs.name(awk_prog.full_path()),
    'PERL': perl_path,
    'PYTHON': python_path,
    'ZIP': zip_prog.found() ? zip_prog.full_path() : '/usr/bin/zip',
    'UNZIP': unzip_path,
    'HAVE_ZIPINFO': have_zipinfo ? '1' : '0',
  }
  extfs_subst += ['-e', 's|@' + var + '@|' + value + '|g']
endforeach

foreach name : ['a+', 'apt+', 'audio', 'brotli', 'deb', 'deba', 'debd', 'dpkg+', 'iso9660',
                'hp48+', 'lslR', 'mailfs', 'patchfs', 'rpms+', 's3+', 'torrent', 'uace', 'ualz',
                'uar', 'uarc', 'uarj', 'uc1541', 'ucab', 'uha', 'uinno', 'ulha', 'ulib', 'unar',
                'urar', 'usqfs', 'uwim', 'uzip', 'uzoo']
  configure_file(
    input: name + '.in',
    output: name,
    command: ['sed', extfs_subst, '@INPUT@'],
    capture: true,
    install: true,
    install_dir: extfshelpersdir,
    install_mode: 'rwxr-xr-x',
  )
endforeach

install_data('changesetfs', 'gitfs+', 'patchsetfs', 'rpm', 'trpm', 'u7z',
  install_dir: extfshelpersdir,
  install_mode: 'rwxr-xr-x',
)
install_data('README', 'README.extfs', install_dir: extfshelpersdir)
