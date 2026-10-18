# the markdown viewer of ext.d on recorded input
test('ext.d/markdown', find_program('ext.d/markdown/test_markdown.sh'),
  args: [
    '--data-dir', meson.current_source_dir() / 'ext.d' / 'markdown' / 'data',
    '--scripts-dir', meson.project_source_root() / 'misc' / 'ext.d',
  ],
  workdir: meson.current_build_dir(),
  suite: 'misc')

# mcommander in pseudo-terminals: a session outlives its terminal and is taken by another. It
# takes minutes, so it is not in the default suite: "meson test -C <builddir> --suite session".
# CI runs it for a pull request with the label 'sandbox', as the cases of docker/.
if have_session and python3.found()
  test('session', python3,
    args: [meson.current_source_dir() / 'session.py'],
    env: {
      'MC_BIN': mcommander.full_path(),
      'SESSION_PLUGIN': meson.project_build_root() / 'src' / 'session' / 'session.so',
      'MC_SESSION_HELPER': meson.project_build_root() / 'src' / 'session' / 'session_helper',
      'MC_DATADIR': meson.project_source_root() / 'misc',
    },
    depends: mcommander,
    timeout: 1800,
    is_parallel: false,
    suite: 'session')
endif
