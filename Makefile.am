## Process this file with automake to create Makefile.in.
AUTOMAKE_OPTIONS = 1.14

SUBDIRS = po lib src doc contrib misc

if HAVE_TESTS
    SUBDIRS += tests
endif

EXTRA_DIST = maint/generate-lua-api.py maint/check-glib-api.sh

dist_noinst_SCRIPTS = \
    build-glib2.sh \
    version.sh

dist_noinst_HEADERS = $(top_srcdir)/mc-version.h

ACLOCAL_AMFLAGS = -I m4

CONFIG_STATUS_DEPENDENCIES = $(top_srcdir)/mc-version.h

.PHONY: update-version man-in indent

update-version:
	@if test -x $(top_srcdir)/version.sh; then \
	    $(top_srcdir)/version.sh "$(top_srcdir)" 2>&1 >/dev/null; \
	else \
	    if test ! -e $(top_srcdir)/mc-version.h; then \
	        echo "File not found: $(top_srcdir)/version.sh"; \
	        exit 1; \
	    fi; \
	fi

$(top_srcdir)/mc-version.h: update-version

# The roff of the man pages, made from their markdown before a commit.
# Needs go-md2man; the build itself does not.
man-in:
	$(top_srcdir)/maint/update-man-in.sh $(top_srcdir)

# Only the clang-format version named in the script formats this tree; see
# there.  CLANG_FORMAT=<path> names the binary to use.
indent:
	$(top_srcdir)/maint/indent.sh $(top_srcdir)

.PHONY: buildcheck check-glib-api

check-glib-api:
	$(SHELL) $(top_srcdir)/maint/check-glib-api.sh

# What the ubuntu job of the CI does, in a container, here. REF names what to
# build (default HEAD), BUILDCHECK_IMAGE the image to build it in.
buildcheck:
	$(top_srcdir)/maint/buildcheck.sh $(REF)
