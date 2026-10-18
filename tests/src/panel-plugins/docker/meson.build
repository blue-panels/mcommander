docker_tests = {
  'docker_tar_test': [pp_src / 'docker' / 'docker-tar.c'],
  'docker_cp_stream_test': [pp_src / 'docker' / 'docker-cp-stream.c'],
  'docker_domains_test': [],
  'docker_paths_test': [],
  'docker_links_test': [pp_src / 'docker' / 'docker-tar.c', pp_src / 'docker' / 'docker-cp-stream.c'],
}

foreach name, extra : docker_tests
  test(name,
    executable(name, name + '.c', extra,
      c_args: test_c_args + ['-DMC_PLUGIN_DIR="/tmp"'], dependencies: test_lib_deps),
    env: {'CK_FORK': 'no'},
    workdir: meson.current_build_dir(),
    suite: 'src/panel-plugins/docker')
endforeach
