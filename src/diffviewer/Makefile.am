SUBDIRS = xdiff

noinst_LTLIBRARIES = libdiffviewer.la

libdiffviewer_la_SOURCES = \
	internal.h \
	search.c \
	syntax.c \
	ydiff.c ydiff.h

libdiffviewer_la_LIBADD = xdiff/libxdiff.la

AM_CPPFLAGS = -I$(top_srcdir) $(GLIB_CFLAGS)
