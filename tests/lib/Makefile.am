PACKAGE_STRING = "/lib"

SUBDIRS = . mcconfig search strutil vfs widget

AM_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) @CHECK_CFLAGS@

AM_CPPFLAGS += -DTEST_RUNTIME_PLUGIN_DIR=\"$(abs_builddir)/.libs\"
AM_CPPFLAGS += -DTEST_LUA_RUNTIME_DIR=\"$(abs_top_builddir)/src/lua/.libs\"
AM_CPPFLAGS += \
	-DTEST_LUA_JAVA_CLASS_DIR=\"$(abs_top_srcdir)/src/lua/scripts/mc/lua-java-class\" \
	-DTEST_LUA_MARKDOWN_DIR=\"$(abs_top_srcdir)/src/lua/scripts/mc/lua-markdown\" \
	-DTEST_LUA_MARKDOWN_DATA_DIR=\"$(abs_srcdir)/lua_markdown\" \
	-DTEST_LUA_READELF_DIR=\"$(abs_top_srcdir)/src/lua/scripts/mc/lua-readelf\" \
	-DTEST_LUA_SIXEL_DIR=\"$(abs_top_srcdir)/src/lua/scripts/mc/lua-sixel\"

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS) \
	@E2P_LIBS@
endif

EXTRA_DIST = utilunix__my_system-common.c \
	lua_markdown/code.md \
	lua_markdown/code.md.out \
	lua_markdown/code_syntax.md \
	lua_markdown/code_syntax.md.out \
	lua_markdown/definitions.md \
	lua_markdown/definitions.md.out \
	lua_markdown/footnotes.md \
	lua_markdown/footnotes.md.out \
	lua_markdown/headings.md \
	lua_markdown/headings.md.out \
	lua_markdown/html.md \
	lua_markdown/html.md.out \
	lua_markdown/images.md \
	lua_markdown/images.md.out \
	lua_markdown/inline.md \
	lua_markdown/inline.md.out \
	lua_markdown/latex.md \
	lua_markdown/latex.md.out \
	lua_markdown/latex_geometry.md \
	lua_markdown/latex_geometry.md.out \
	lua_markdown/latex_matrix.md \
	lua_markdown/latex_more.md \
	lua_markdown/latex_nested.md \
	lua_markdown/latex_nested.md.out \
	lua_markdown/latex_more.md.out \
	lua_markdown/latex_matrix.md.out \
	lua_markdown/links.md \
	lua_markdown/links.md.out \
	lua_markdown/lists.md \
	lua_markdown/lists.md.out \
	lua_markdown/lists_nested.md \
	lua_markdown/lists_nested.md.out \
	lua_markdown/mermaid.md \
	lua_markdown/mermaid.md.out \
	lua_markdown/mermaid_class.md \
	lua_markdown/mermaid_class.md.out \
	lua_markdown/mermaid_class_many.md \
	lua_markdown/mermaid_class_many.md.out \
	lua_markdown/mermaid_class_two.md \
	lua_markdown/mermaid_class_two.md.out \
	lua_markdown/mermaid_circle.md \
	lua_markdown/mermaid_circle.md.out \
	lua_markdown/mermaid_pie.md \
	lua_markdown/mermaid_pie.md.out \
	lua_markdown/mermaid_shapes.md \
	lua_markdown/mermaid_shapes.md.out \
	lua_markdown/mixed.md \
	lua_markdown/mixed.md.out \
	lua_markdown/sgr.md \
	lua_markdown/sgr.md.out \
	lua_markdown/strike.md \
	lua_markdown/strike.md.out \
	lua_markdown/task_list.md \
	lua_markdown/task_list.md.out \
	lua_markdown/table.md \
	lua_markdown/wide.md \
	lua_markdown/wide.md.out \
	lua_markdown/table.md.out \
	lua_markdown/table_br.md \
	lua_markdown/table_br.md.out

TESTS = \
	key \
	library_independ \
	mc_build_filename \
	name_quote \
	serialize \
	runtime_events \
	terminal \
	tty \
	tty_color \
	utilunix__mc_pread_signal \
	utilunix__mc_pstream_get_string \
	utilunix__my_system_fork_fail \
	utilunix__my_system_fork_child_shell \
	utilunix__my_system_fork_child \
	x_basename

TESTS += mc_realpath

if HAVE_RUNTIME_MODULES
TESTS += extension_runtime
noinst_LTLIBRARIES = runtime-plugin-fixture.la

runtime_plugin_fixture_la_SOURCES = \
	runtime_plugin_fixture.c
runtime_plugin_fixture_la_LDFLAGS = -module -avoid-version -rpath $(abs_builddir)/.libs
runtime_plugin_fixture_la_LIBADD = $(GLIB_LIBS)
endif

if HAVE_RUNTIME_MODULES
if ENABLE_LUA_PLUGIN
TESTS += lua_runtime
endif
endif

check_PROGRAMS = $(TESTS)

key_SOURCES = \
	key.c

library_independ_SOURCES = \
	library_independ.c

mc_build_filename_SOURCES = \
	mc_build_filename.c

mc_realpath_SOURCES = \
	mc_realpath.c

name_quote_SOURCES = \
	name_quote.c

serialize_SOURCES = \
	serialize.c

runtime_events_SOURCES = \
	runtime_events.c

extension_runtime_SOURCES = \
	extension_runtime.c
extension_runtime_DEPENDENCIES = runtime-plugin-fixture.la
extension_runtime_LDADD = $(GMODULE_LIBS)

lua_runtime_SOURCES = \
	lua_runtime.c
lua_runtime_DEPENDENCIES = $(top_builddir)/src/lua/mc-lua.la
lua_runtime_LDADD = $(GMODULE_LIBS)

terminal_SOURCES = \
	terminal.c

tty_SOURCES = \
	tty.c

tty_color_SOURCES = \
	tty_color.c

utilunix__mc_pread_signal_SOURCES = \
	utilunix__mc_pread_signal.c

utilunix__mc_pstream_get_string_SOURCES = \
	utilunix__mc_pstream_get_string.c

utilunix__my_system_fork_fail_SOURCES = \
	utilunix__my_system-fork_fail.c

utilunix__my_system_fork_child_shell_SOURCES = \
	utilunix__my_system-fork_child_shell.c

utilunix__my_system_fork_child_SOURCES = \
	utilunix__my_system-fork_child.c

x_basename_SOURCES = \
	x_basename.c
