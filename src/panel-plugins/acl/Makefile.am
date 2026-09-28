acldir = $(panel_plugins_dir)/acl
acl_LTLIBRARIES = mc-panel-acl.la
dist_acl_DATA = acl_panel.md

mc_panel_acl_la_SOURCES = \
	acl.c \
	acl-dialog.c \
	acl-dialog.h \
	acl-model.c \
	acl-model.h \
	acl-nt.c \
	acl-nt.h \
	acl-nt-dialog.c
mc_panel_acl_la_CPPFLAGS = $(GLIB_CFLAGS) $(LIBACL_CFLAGS) -I$(top_srcdir) \
	-DMC_PLUGIN_DIR=\""$(panel_plugins_dir)/acl"\"
mc_panel_acl_la_LDFLAGS = -module -avoid-version
mc_panel_acl_la_LIBADD = $(GLIB_LIBS) $(LIBACL_LIBS)
