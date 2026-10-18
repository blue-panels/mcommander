PACKAGE_STRING = "/src/panel-plugins/git"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-DMC_PLUGIN_DIR=\"/tmp\" \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

TESTS = \
	git_range

check_PROGRAMS = $(TESTS)

git_range_SOURCES = \
	git_range.c
