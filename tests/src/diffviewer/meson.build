foreach name : ['align3', 'compare_files']
  test(name,
    executable(name, name + '.c', c_args: test_c_args, dependencies: test_src_deps),
    workdir: meson.current_build_dir(),
    suite: 'src/diffviewer')
endforeach
