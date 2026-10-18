man_pages_sr = {
  'mcommander.1': 'roff/mcommander.1.in',
  'mview.1': 'roff/mview.1.in',
}
if get_option('internal_edit')
  man_pages_sr += {'mcedit6.1': 'roff/mcedit6.1.in'}
endif
if get_option('diff_viewer')
  man_pages_sr += {'mcdiff6.1': 'roff/mcdiff6.1.in'}
endif

foreach page, input : man_pages_sr
  custom_target(page + '-sr',
    input: input,
    output: page,
    command: [sed, man_sed_args, '@INPUT@'],
    capture: true,
    install: true,
    install_dir: mandir / 'sr' / 'man1',
  )
endforeach
