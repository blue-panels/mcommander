i18n = import('i18n')

# the catalogues of po/LINGUAS, or only those also named in LINGUAS of the
# environment, as the po/Makefile of gettext did
po_all = []
foreach line : fs.read('LINGUAS').split('\n')
  line = line.strip()
  if line != '' and not line.startswith('#')
    po_all += line.split()
  endif
endforeach
env_linguas = run_command('sh', '-c', 'echo "$LINGUAS"', check: false).stdout().strip().split()
po_languages = []
foreach lang : po_all
  if env_linguas.length() == 0 or lang in env_linguas
    po_languages += lang
  endif
endforeach

# "ninja mcommander-update-po" merges the .po files with the template
i18n.gettext(package,
  languages: po_languages,
  args: [
    '--keyword=_', '--keyword=N_', '--keyword=Q_',
    '--add-comments=TRANSLATORS:',
    '--msgid-bugs-address=' + bugreport,
    '--copyright-holder=Free Software Foundation, Inc.',
  ],
)

# "ninja pot" remakes po/POTFILES.in and po/mcommander.pot, as the l10n-pot workflow does
run_target('pot', command: [files('update-pot.sh'), meson.project_source_root()])
