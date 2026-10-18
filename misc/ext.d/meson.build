extd_conf = configuration_data({
  'EXTFSHELPERSDIR': extfshelpersdir,
  'PERL': perl_path,
  'MANDOC': mandoc,
  'MAN_FLAGS': man_flags,
  'NROFF_UTF8': nroff_utf8,
  'NROFF_8BIT': nroff_8bit,
})
foreach f : ['misc.sh', 'text.sh']
  configure_file(
    input: f + '.in',
    output: f,
    configuration: extd_conf,
    install_dir: exthelpersdir,
    install_mode: 'rwxr-xr-x',
  )
endforeach

install_data(
  'archive.sh',
  'doc.sh',
  'image.sh',
  'markdown.sh',
  'package.sh',
  'sound.sh',
  'video.sh',
  'web.sh',
  install_dir: exthelpersdir,
  install_mode: 'rwxr-xr-x',
)

install_data(
  'markdown_common.awk',
  'markdown_render.awk',
  'markdown_table.awk',
  install_dir: exthelpersdir,
)
