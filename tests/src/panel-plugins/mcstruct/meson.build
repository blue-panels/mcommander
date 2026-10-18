mcstruct_test_args = test_c_args + [
  '-DMCSTRUCT_DATA_DIR="@0@"'.format(pp_src / 'mcstruct' / 'data'),
]
mcstruct_test_deps = [test_lib_deps, zlib, cc.find_library('m', required: false)]

foreach name : ['slv_parse', 'slv_eval', 'slv_reader', 'slv_edit', 'slv_ext', 'slv_defs']
  test(name,
    executable(name, name + '.c',
      c_args: mcstruct_test_args, link_with: libmcslv, dependencies: mcstruct_test_deps),
    workdir: meson.current_build_dir(),
    suite: 'src/panel-plugins/mcstruct')
endforeach

# dumps what a definition makes of a file, by hand
executable('mc_slv_dump', 'mc_slv_dump.c',
  c_args: mcstruct_test_args, link_with: libmcslv, dependencies: mcstruct_test_deps)
