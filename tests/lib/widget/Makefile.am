PACKAGE_STRING = "/lib/widget"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib/vfs \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

TESTS = \
	buttonbar_modifiers \
	complete_engine \
	filechooser \
	hotkey_equal \
	group_init_destroy \
	group_set_state \
	hexstrip \
	paste \
	table_datasource \
	table_quick_search \
	textarea \
	widget_find_by_id \
	widget_make_global_local

check_PROGRAMS = $(TESTS)

buttonbar_modifiers_SOURCES = \
	buttonbar_modifiers.c

complete_engine_SOURCES = \
	complete_engine.c

filechooser_SOURCES = \
	filechooser.c

# the local file system it includes needs the flags of ext2
filechooser_LDADD = @E2P_LIBS@

hotkey_equal_SOURCES = \
	hotkey_equal.c

group_init_destroy_SOURCES = \
	group_init_destroy.c

group_set_state_SOURCES = \
	group_set_state.c

table_datasource_SOURCES = \
	table_datasource.c

table_quick_search_SOURCES = \
	table_quick_search.c

textarea_SOURCES = \
	textarea.c

hexstrip_SOURCES = \
	hexstrip.c

paste_SOURCES = \
	paste.c

widget_find_by_id_SOURCES = \
	widget_find_by_id.c

widget_make_global_local_SOURCES = \
	widget_make_global_local.c
