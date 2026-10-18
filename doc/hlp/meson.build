# The help is split by program: a node the manual of the file manager does not
# have is looked for in the file of the program it belongs to. The help window
# reads the markdown of the man page; the screens that belong to the file
# manager alone (xnc.md) are appended to its file.
cat = find_program('cat')

custom_target('mcommander.md',
  input: ['../man/mcommander.md', 'xnc.md'],
  output: 'mcommander.md',
  command: [cat, '@INPUT@'],
  capture: true,
  install: true,
  install_dir: pkgdatadir / 'help',
)
foreach prog : ['mview', 'mcedit6', 'mcdiff6']
  custom_target(prog + '.md',
    input: '../man' / prog + '.md',
    output: prog + '.md',
    command: [cat, '@INPUT@'],
    capture: true,
    install: true,
    install_dir: pkgdatadir / 'help',
  )
endforeach

install_data('gnu-gpl-v3.0.md', install_dir: pkgdatadir / 'help')

foreach lang : doc_linguas
  custom_target('mcommander.@0@.md'.format(lang),
    input: ['../man' / lang / 'mcommander.md', lang / 'xnc.md'],
    output: 'mcommander.@0@.md'.format(lang),
    command: [cat, '@INPUT@'],
    capture: true,
    install: true,
    install_dir: pkgdatadir / 'help',
  )
  foreach prog : ['mview', 'mcedit6', 'mcdiff6']
    custom_target('@0@.@1@.md'.format(prog, lang),
      input: '../man' / lang / prog + '.md',
      output: '@0@.@1@.md'.format(prog, lang),
      command: [cat, '@INPUT@'],
      capture: true,
      install: true,
      install_dir: pkgdatadir / 'help',
    )
  endforeach
endforeach
