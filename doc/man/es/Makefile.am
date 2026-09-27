LANG=es
mandir = @mandir@/$(LANG)

# Everything under roff/ is made from the *.md files by
# maint/update-man-in.sh and is not to be edited by hand.
EXTRA_DIST = mcommander.md roff/mcommander.1.in \
	mview.md roff/mview.1.in \
	mcedit6.md roff/mcedit6.1.in \
	mcdiff6.md roff/mcdiff6.1.in

man_MANS = mcommander.1 mview.1

if USE_INTERNAL_EDIT
man_MANS += mcedit6.1
endif

if USE_DIFF
man_MANS += mcdiff6.1
endif

CLEANFILES = $(man_MANS)

include ../man-include.am

mcommander.1: $(srcdir)/roff/mcommander.1.in
	$(SED) $(SED_PARAMETERS) '$(srcdir)/roff/mcommander.1.in' > '$@'

mview.1: $(srcdir)/roff/mview.1.in
	$(SED) $(SED_PARAMETERS) '$(srcdir)/roff/mview.1.in' > '$@'

if USE_INTERNAL_EDIT
mcedit6.1: $(srcdir)/roff/mcedit6.1.in
	$(SED) $(SED_PARAMETERS) '$(srcdir)/roff/mcedit6.1.in' > '$@'
endif

if USE_DIFF
mcdiff6.1: $(srcdir)/roff/mcdiff6.1.in
	$(SED) $(SED_PARAMETERS) '$(srcdir)/roff/mcdiff6.1.in' > '$@'
endif
