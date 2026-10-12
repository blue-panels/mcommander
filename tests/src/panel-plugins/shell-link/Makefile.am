PACKAGE_STRING = "/src/panel-plugins/shell-link"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/src/panel-plugins/shell-link/libshfs.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

if ENABLE_SHELL_SSH2
LIBS += $(LIBSSH2_LIBS)
endif

TESTS = \
	shfs_parse_ls \
	helpers_ls.sh \
	helpers_get.sh \
	helpers_transfer.sh \
	helpers_builtin.sh

check_PROGRAMS = shfs_parse_ls builtin_helper

shfs_parse_ls_SOURCES = \
	shfs_parse_ls.c

# prints a script built into the plugin, for the helpers_*.sh tests
builtin_helper_SOURCES = \
	builtin_helper.c

# the library comes in through LIBS, which automake does not turn into a
# dependency; without this a rebuilt libshfs leaves a stale test binary
SHFS_LIB = $(top_builddir)/src/panel-plugins/shell-link/libshfs.la
shfs_parse_ls_DEPENDENCIES = $(SHFS_LIB)
builtin_helper_DEPENDENCIES = $(SHFS_LIB)

TEST_EXTENSIONS = .sh
SH_LOG_COMPILER = $(SHELL)
AM_TESTS_ENVIRONMENT = srcdir='$(srcdir)'; export srcdir;

EXTRA_DIST = helpers_ls.sh helpers_get.sh helpers_transfer.sh helpers_builtin.sh
