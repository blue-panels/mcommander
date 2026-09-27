DIST_SUBDIRS = es hu it pl ru sr
if USE_NLS
SUBDIRS = $(DOC_LINGUAS)
endif

man_MANS = mcommander.1 mc6.1 mview.1 mctree.1 mcstruct.1

if USE_INTERNAL_EDIT
man_MANS += mcedit6.1
endif

if USE_DIFF
man_MANS += mcdiff6.1
endif

CLEANFILES = $(man_MANS)

# Everything under roff/ is made from the *.md files by
# maint/update-man-in.sh and is not to be edited by hand.
EXTRA_DIST = \
	man-include.am \
	mc6.1.in \
	mcommander.md \
	roff/mcommander.1.in \
	mcedit6.md \
	roff/mcedit6.1.in \
	mview.md \
	roff/mview.1.in \
	mctree.md \
	roff/mctree.1.in \
	mcstruct.md \
	roff/mcstruct.1.in \
	mcdiff6.md \
	roff/mcdiff6.1.in

include man-include.am

mcommander.1: $(srcdir)/roff/mcommander.1.in
	$(SED) $(SED_PARAMETERS) '$(srcdir)/roff/mcommander.1.in' > '$@'

mc6.1: $(srcdir)/mc6.1.in
	$(SED) $(SED_PARAMETERS) '$(srcdir)/mc6.1.in' > '$@'

mview.1: $(srcdir)/roff/mview.1.in
	$(SED) $(SED_PARAMETERS) '$(srcdir)/roff/mview.1.in' > '$@'

mctree.1: $(srcdir)/roff/mctree.1.in
	$(SED) $(SED_PARAMETERS) '$(srcdir)/roff/mctree.1.in' > '$@'

mcstruct.1: $(srcdir)/roff/mcstruct.1.in
	$(SED) $(SED_PARAMETERS) '$(srcdir)/roff/mcstruct.1.in' > '$@'

if USE_INTERNAL_EDIT
mcedit6.1: $(srcdir)/roff/mcedit6.1.in
	$(SED) $(SED_PARAMETERS) '$(srcdir)/roff/mcedit6.1.in' > '$@'
endif

if USE_DIFF
mcdiff6.1: $(srcdir)/roff/mcdiff6.1.in
	$(SED) $(SED_PARAMETERS) '$(srcdir)/roff/mcdiff6.1.in' > '$@'
endif
