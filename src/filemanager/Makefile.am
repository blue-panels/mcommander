
noinst_LTLIBRARIES = libmcfilemanager.la

libmcfilemanager_la_SOURCES = \
	achown.c \
	boxes.c boxes.h \
	cd.c cd.h \
	chmod.c \
	chown.c \
	cmd.c cmd.h \
	command.c command.h \
	wprompt.c wprompt.h \
	dir.c dir.h \
	dir_watch.c dir_watch.h \
	ext.c ext.h \
	file.c file.h \
	filegui.c filegui.h \
	dnd.c dnd.h \
	filemanager.h filemanager.c \
	filenot.c filenot.h \
	find.c \
	hotlist.c hotlist.h \
	info.c info.h \
	ioblksize.h \
	layout.c layout.h \
	mcmagic.c mcmagic.h \
	mcterm_overlay.c mcterm_overlay.h \
	mountlist.c mountlist.h \
	panel.c panel.h \
	panel-quick-filter.h \
	panel_modes.c panel_modes.h \
	panel_plugin_ops.c panel_plugin_ops.h \
	panel_plugin_ui.c \
	tree.c tree.h \
	treestore.c treestore.h

AM_CPPFLAGS = -I$(top_srcdir) $(GLIB_CFLAGS) $(LIBMAGIC_CFLAGS)
libmcfilemanager_la_LIBADD = $(LIBMAGIC_LIBS)

if ENABLE_EXT2FS_ATTR
libmcfilemanager_la_SOURCES += \
	chattr.c
endif
