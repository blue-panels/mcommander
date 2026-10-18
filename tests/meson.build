# Unit tests on the check framework: "meson test -C <builddir>".
# Every test runs in the build directory of its own subdirectory, where the
# files it reads are made.
test_c_args = []
test_lib_deps = [libmc_dep, check]
test_src_deps = [libinternal_dep, check]

subdir('lib')
subdir('src')
subdir('misc')

# the session test takes minutes: "meson test" leaves it out, "--suite session" runs it
add_test_setup('default', exclude_suites: ['session'], is_default: true)
