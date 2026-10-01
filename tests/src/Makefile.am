PACKAGE_STRING = "/src"

SUBDIRS = . syntax filemanager vfs panel-plugins viewer

if USE_INTERNAL_EDIT
SUBDIRS += editor
endif

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib/vfs \
	-DTEST_SHARE_DIR=\"$(abs_srcdir)/fixtures\" \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

EXTRA_DIST = \
	fixtures/charsets \
	fixtures/hints/hint \
	fixtures/viewertest.txt \
	execute__common.c

TESTS = \
	clipboard_info \
	input_clip_text \
	tty_far2l_clipboard \
	execute__execute_with_vfs_arg \
	execute__execute_external_editor_or_viewer \
	execute__execute_get_external_cmd_opts_from_config \
	file_history \
	keymap_reload \
	runtime_host__syntax \
	learn_modifiers \
	mctree_model \
	mctree_resolver \
	mctree_view \
	usermenu_ini \
	term_key_merge \
	tty_enter \
	tty_far2l_input \
	tty_kitty_keyboard \
	tty_win32_input \
	usermenu__test_condition \
	tty_paste \
	usermenu__test_expand_format

if ENABLE_MCTERM
TESTS += \
	mcterm_far2l \
	mcterm_key_encode \
	mcterm_osc7_parse \
	mcterm_paste \
	mcterm_protocol_parse \
	mcterm_pty_send \
	mcterm_select
endif

check_PROGRAMS = $(TESTS)

clipboard_info_SOURCES = \
	clipboard_info.c

input_clip_text_SOURCES = \
	input_clip_text.c

tty_far2l_clipboard_SOURCES = \
	tty_far2l_clipboard.c

execute__execute_with_vfs_arg_SOURCES = \
	execute__execute_with_vfs_arg.c


execute__execute_external_editor_or_viewer_SOURCES = \
	execute__execute_external_editor_or_viewer.c


execute__execute_get_external_cmd_opts_from_config_SOURCES = \
	execute__execute_get_external_cmd_opts_from_config.c

file_history_SOURCES = \
	file_history.c

keymap_reload_SOURCES = \
	keymap_reload.c

runtime_host__syntax_SOURCES = \
	runtime_host__syntax.c

learn_modifiers_SOURCES = \
	learn_modifiers.c

mcterm_far2l_SOURCES = \
	mcterm_far2l.c

mcterm_key_encode_SOURCES = \
	mcterm_key_encode.c

mcterm_osc7_parse_SOURCES = \
	mcterm_osc7_parse.c

mcterm_paste_SOURCES = \
	mcterm_paste.c

mcterm_protocol_parse_SOURCES = \
	mcterm_protocol_parse.c

mcterm_pty_send_SOURCES = \
	mcterm_pty_send.c

mcterm_select_SOURCES = \
	mcterm_select.c

mctree_model_SOURCES = \
	mctree_model.c

mctree_resolver_SOURCES = \
	mctree_resolver.c

mctree_view_SOURCES = \
	mctree_view.c

usermenu_ini_SOURCES = \
	usermenu_ini.c

term_key_merge_SOURCES = \
	term_key_merge.c

tty_enter_SOURCES = \
	tty_enter.c

tty_far2l_input_SOURCES = \
	tty_far2l_input.c

tty_kitty_keyboard_SOURCES = \
	tty_kitty_keyboard.c

tty_win32_input_SOURCES = \
	tty_win32_input.c

usermenu__test_condition_SOURCES = \
	usermenu__test_condition.c

tty_paste_SOURCES = \
	tty_paste.c

usermenu__test_expand_format_SOURCES = \
	usermenu__test_expand_format.c