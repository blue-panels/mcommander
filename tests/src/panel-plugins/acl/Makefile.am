PACKAGE_STRING = "/src/panel-plugins/acl"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(LIBACL_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(LIBACL_LIBS) \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

TESTS = \
	acl_model \
	acl_nt

check_PROGRAMS = $(TESTS)

acl_model_SOURCES = \
	acl_model.c \
	$(top_srcdir)/src/panel-plugins/acl/acl-model.c

acl_nt_SOURCES = \
	acl_nt.c \
	$(top_srcdir)/src/panel-plugins/acl/acl-nt.c
