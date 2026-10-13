PACKAGE_STRING = "/src/diffviewer"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

TESTS = \
	compare_files

check_PROGRAMS = $(TESTS)

compare_files_SOURCES = \
	compare_files.c
