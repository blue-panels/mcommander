man_pages_ru = {
  'mcommander.1': 'roff/mcommander.1.in',
  'mview.1': 'roff/mview.1.in',
}
if get_option('internal_edit')
  man_pages_ru += {'mcedit6.1': 'roff/mcedit6.1.in'}
endif
if get_option('diff_viewer')
  man_pages_ru += {'mcdiff6.1': 'roff/mcdiff6.1.in'}
endif

foreach page, input : man_pages_ru
  custom_target(page + '-ru',
    input: input,
    output: page,
    command: [sed, man_sed_args, '@INPUT@'],
    capture: true,
    install: true,
    install_dir: mandir / 'ru' / 'man1',
  )
endforeach
