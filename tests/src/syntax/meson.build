foreach name : ['line_local', 'context_rules']
  test(name,
    executable(name, name + '.c', c_args: test_c_args, dependencies: test_src_deps),
    workdir: meson.current_build_dir(),
    suite: 'src/syntax')
endforeach
