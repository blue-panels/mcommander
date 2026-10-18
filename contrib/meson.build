# data, not scripts: each one is read with "." from a shell profile and carries no shebang
foreach f : ['mc6.csh', 'mc6.fish', 'mc6.sh']
  configure_file(input: f + '.in', output: f,
                 configuration: {'pkglibexecdir': pkglibexecdir},
                 install_dir: pkglibexecdir, install_mode: 'rw-r--r--')
endforeach
foreach f : ['mc6-wrapper.csh', 'mc6-wrapper.fish', 'mc6-wrapper.sh']
  configure_file(input: f + '.in', output: f,
                 configuration: {'bindir': bindir},
                 install_dir: pkglibexecdir, install_mode: 'rw-r--r--')
endforeach
