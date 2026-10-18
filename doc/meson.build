# The help translations: the languages of doc_languages that are also built
# as message catalogues (LINGUAS of the environment, else po/LINGUAS).
doc_linguas = []
if have_nls
  env_linguas = run_command('sh', '-c', 'echo "$LINGUAS"', check: false).stdout().strip()
  if env_linguas != ''
    po_linguas = env_linguas.split()
  else
    po_linguas = []
    foreach line : fs.read(meson.project_source_root() / 'po' / 'LINGUAS').split('\n')
      line = line.strip()
      if line != '' and not line.startswith('#')
        po_linguas += line.split()
      endif
    endforeach
  endif
  foreach lang : doc_languages
    if lang in po_linguas
      doc_linguas += lang
    endif
  endforeach
endif

subdir('hints')
subdir('man')
subdir('hlp')

install_data('LUA_PLUGINS.md', 'LUA_API_REFERENCE.md', 'lua-api.json', install_dir: docdir)
