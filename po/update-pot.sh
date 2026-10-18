#!/bin/sh
# Remake po/POTFILES.in and po/mcommander.pot from the sources.
# usage: update-pot.sh <toplevel-source-dir>
# Needs no configured build tree: the l10n-pot workflow runs it as it is.

set -e

cd "${1:?usage: $0 <toplevel-source-dir>}"

xgettext --keyword=_ --keyword=N_ --keyword=Q_ --output=- \
    `find lib maint src tests -name '*.[ch]'` \
    | sed -ne '/^#:/{s/#://;s/:[0-9]*/\
/g;s/ //g;p;}' \
    | grep -v '^$' | LC_ALL=C sort | uniq > po/POTFILES.in

xgettext --default-domain=mcommander --directory=. \
    --add-comments=TRANSLATORS: --keyword=_ --keyword=N_ --keyword=Q_ \
    --files-from=po/POTFILES.in \
    --copyright-holder='Free Software Foundation, Inc.' \
    --package-name='mcommander' \
    --package-version="`./version.sh .`" \
    --msgid-bugs-address='https://github.com/blue-panels/mcommander/issues' \
    --output=po/mcommander.pot
