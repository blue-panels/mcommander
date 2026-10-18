install_data('hint', install_dir: pkgdatadir / 'hints')

# the translated hints go with the message catalogues: every l10n/hint.*
if have_nls
  hint_files = run_command('sh', '-c', 'cd "$1" && ls hint.*', '--', meson.current_source_dir() / 'l10n',
                           check: true).stdout().split()
  hint_paths = []
  foreach h : hint_files
    hint_paths += 'l10n' / h
  endforeach
  install_data(hint_paths, install_dir: pkgdatadir / 'hints')
endif
