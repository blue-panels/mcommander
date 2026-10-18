# The roff under roff/ is made from the markdown by maint/update-man-in.sh
# before a commit; the build only puts the paths of this installation in.
sed = find_program('sed')

# the version without the distance to the tag
man_version = meson.project_version().split('-')[0]
man_sed_args = [
  '-e', 's|{{MAN_VERSION}}|@0@|g'.format(man_version),
  '-e', 's|{{sysconfdir}}|@0@|g'.format(sysconfdir),
  '-e', 's|{{libexecdir}}|@0@|g'.format(libexecdir),
  '-e', 's|{{pkglibexecdir}}|@0@|g'.format(pkglibexecdir),
  '-e', 's|{{pkgdatadir}}|@0@|g'.format(pkgdatadir),
  '-e', 's|{{bindir}}|@0@|g'.format(bindir),
  '-e', 's|{{panel_plugins_dir}}|@0@|g'.format(panel_plugins_dir),
]

man_pages = {
  'mcommander.1': 'roff/mcommander.1.in',
  'mc6.1': 'mc6.1.in',
  'mview.1': 'roff/mview.1.in',
  'mctree.1': 'roff/mctree.1.in',
  'mcstruct.1': 'roff/mcstruct.1.in',
}
if get_option('internal_edit')
  man_pages += {'mcedit6.1': 'roff/mcedit6.1.in'}
endif
if get_option('diff_viewer')
  man_pages += {'mcdiff6.1': 'roff/mcdiff6.1.in'}
endif

foreach page, input : man_pages
  custom_target(page,
    input: input,
    output: page,
    command: [sed, man_sed_args, '@INPUT@'],
    capture: true,
    install: true,
    install_dir: mandir / 'man1',
  )
endforeach

# the translated pages, each in a directory of its own for the same names
foreach lang : doc_linguas
  subdir(lang)
endforeach
