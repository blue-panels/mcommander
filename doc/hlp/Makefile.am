DIST_SUBDIRS = es hu it pl ru sr
if USE_NLS
SUBDIRS = $(DOC_LINGUAS)
endif

hlpdir = $(pkgdatadir)/help
# The help is split by program: a node the manual of the file manager does not
# have is looked for in these files.
hlp_DATA = mcommander.md mview.md mcedit6.md mcdiff6.md
# the license is a source file: it is installed as it stands, and make clean
# has no business with it
dist_hlp_DATA = gnu-gpl-v3.0.md

EXTRA_DIST = xnc.md
CLEANFILES = $(hlp_DATA)

# The help window reads the markdown of the man page, with the screens that
# belong to the program itself, and to nothing else, appended to it.
mcommander.md: $(top_srcdir)/doc/man/mcommander.md $(srcdir)/xnc.md
	cat $(top_srcdir)/doc/man/mcommander.md $(srcdir)/xnc.md > $@

mview.md: $(top_srcdir)/doc/man/mview.md
	cat $(top_srcdir)/doc/man/mview.md > $@

mcedit6.md: $(top_srcdir)/doc/man/mcedit6.md
	cat $(top_srcdir)/doc/man/mcedit6.md > $@

mcdiff6.md: $(top_srcdir)/doc/man/mcdiff6.md
	cat $(top_srcdir)/doc/man/mcdiff6.md > $@
