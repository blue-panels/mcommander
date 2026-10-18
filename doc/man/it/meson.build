man_pages_it = {
  'mcommander.1': 'roff/mcommander.1.in',
  'mview.1': 'roff/mview.1.in',
  # this translation always ships the page of the diff viewer
  'mcdiff6.1': 'roff/mcdiff6.1.in',
}
if get_option('internal_edit')
  man_pages_it += {'mcedit6.1': 'roff/mcedit6.1.in'}
endif

foreach page, input : man_pages_it
  custom_target(page + '-it',
    input: input,
    output: page,
    command: [sed, man_sed_args, '@INPUT@'],
    capture: true,
    install: true,
    install_dir: mandir / 'it' / 'man1',
  )
endforeach
