noinst_LTLIBRARIES = libxdiff.la

# The xdiff directory of git v2.56.0, unchanged; git-compat-util.h is ours.
libxdiff_la_SOURCES = \
	git-compat-util.h \
	xdiff.h \
	xdiffi.c xdiffi.h \
	xemit.c xemit.h \
	xhistogram.c \
	xinclude.h \
	xmacros.h \
	xmerge.c \
	xpatience.c \
	xprepare.c xprepare.h \
	xtypes.h \
	xutils.c xutils.h

AM_CPPFLAGS = -I$(srcdir) $(GLIB_CFLAGS)

EXTRA_DIST = .clang-format README
