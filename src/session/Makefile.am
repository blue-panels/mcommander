runtime_pluginsdir = $(runtime_plugins_dir)
runtime_plugins_LTLIBRARIES = session.la
pkglibexec_PROGRAMS = session_helper

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	$(GLIB_CFLAGS) \
	-DMC_SESSION_HELPER=\"$(pkglibexecdir)/session_helper\" \
	-DLOCALEDIR=\""$(localedir)"\"

session_la_SOURCES = \
	session.c \
	session_proto.c session_proto.h
session_la_LDFLAGS = -module -avoid-version
session_la_LIBADD = $(GLIB_LIBS)

session_helper_SOURCES = \
	session_helper.c \
	session_proto.c session_proto.h
session_helper_LDADD = $(GLIB_LIBS) $(LIBINTL)
# its own flags: the objects of session_proto.c differ from the ones of the plugin
session_helper_CPPFLAGS = $(AM_CPPFLAGS)
