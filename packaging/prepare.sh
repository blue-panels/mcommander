#!/bin/sh
# Materialise the packaging recipes for one version.
#
# usage: packaging/prepare.sh VERSION [ARCHIVE]
#   packaging/prepare.sh 6.0.3
#   packaging/prepare.sh 6.1.0 dist/mcommander-6.1.0.tar.gz
#
# The recipes in this repository carry no version of their own: it comes from
# the release tag, so nothing has to be edited after tagging. This script
# writes the files that name a version, from the committed templates:
#
#   debian/changelog                    from the v* tags
#   packaging/rpm/mcommander.spec        from mcommander.spec.in
#   packaging/arch/PKGBUILD             from PKGBUILD.in
#   packaging/gentoo/mcommander-VERSION.ebuild from mcommander.ebuild.in
#   packaging/termux/build.sh           from build.sh.in
#
# Both changelogs are meant for a real upload, so their entries come from the
# annotated tag messages and their dates from the tags themselves: the same
# tag always yields the same source package. A version with no tag of its own
# is a test build and is marked UNRELEASED, which stops dput from taking it.
#
# Environment:
#   DEB_DISTRIBUTION   target suite of the leading entry (default: unstable;
#                      a PPA wants the series, e.g. noble)
#   DEB_VERSION_SUFFIX appended to the Debian revision of the leading entry,
#                      e.g. ~ubuntu24.04.1 for 3:6.0.3-1~ubuntu24.04.1
#
# Given the release archive, the Arch checksum is computed from it. Without
# one the recipe keeps SKIP, and makepkg then says so instead of failing on a
# stale checksum.
set -eu

die() {
    echo "$*" >&2
    exit 1
}

version=${1:?usage: packaging/prepare.sh VERSION [ARCHIVE]}
archive=${2:-}
distribution=${DEB_DISTRIBUTION:-unstable}
suffix=${DEB_VERSION_SUFFIX:-}

# Only release versions. A pre-release has to sort below the release it leads
# to, and the only character that does that is a tilde, which Git does not
# allow in a tag name. Mapping 6.0.2-rc1 to 6.0.2~rc1 is not enough either: the
# orig tarball and its directory would have to be renamed to match, and Arch
# allows neither the tilde nor the hyphen in pkgver. Until that is built, a
# name like 6.0.2rc1 must not pass: it would quietly outrank 6.0.2.
case "$version" in
    *[!0-9.]* | '' | .* | *. | *..*)
        die "not a release version: $version (expected something like 6.0.3)"
        ;;
esac

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

maintainer=$(sed -n 's/^Maintainer: *//p' debian/control)
test -n "$maintainer" || die "cannot read Maintainer from debian/control"


if test -n "$archive"; then
    test -f "$archive" || die "no such archive: $archive"
    checksum=$(b2sum "$archive" | cut -d' ' -f1)
    sha256=$(sha256sum "$archive" | cut -d' ' -f1)
else
    checksum=SKIP
    sha256=SKIP_CHECKSUM
fi

tags=$(git tag --list 'v*' --sort=-version:refname 2>/dev/null || true)

# Only what came before: a source package for 6.0.1 must not lead with the
# changelog of a release made after it, and dpkg takes the version it is
# building from the top of that file.
tags=$(for tag in $tags; do
    newest=$(printf '%s\n%s\n' "${tag#v}" "$version" | sort -Vr | head -n 1)
    if test "$newest" = "${tag#v}" && test "${tag#v}" != "$version"; then
        continue
    fi
    printf '%s\n' "$tag"
done)

# What a release says is written once and read from there by both changelogs:
# from CHANGELOG.md when it has a section for the version, and from the tag
# message otherwise -- which is all the releases made before that file existed
# have to offer.
tag_body() {
    git tag --list --format='%(contents:subject)%0a%(contents:body)' "$1" 2>/dev/null |
        sed -e '/^[[:space:]]*$/d'
}

# One entry per line: an item of CHANGELOG.md is wrapped there, and the lines
# that continue it belong to the entry rather than starting one of their own.
changelog_body() {
    test -f CHANGELOG.md || return 0
    awk -v version="$1" '
        function flush() { if (item != "") { print item; item = "" } }
        $0 ~ "^## " version "([^0-9.]|$)" { inside = 1; next }
        inside && /^## / { exit }
        inside && /^- / {
            flush()
            item = $0
            sub(/^-[[:space:]]*/, "", item)
            next
        }
        inside && item != "" && /^[[:space:]]+[^[:space:]]/ {
            continuation = $0
            sub(/^[[:space:]]+/, "", continuation)
            item = item " " continuation
            next
        }
        inside { flush() }
        END { flush() }
    ' CHANGELOG.md
}

# An entry is one line by then, which reads badly in a changelog. Break it at
# the width below, with the marker on the first line and the rest under it.
wrap_entries() {
    awk -v marker="$1" -v indent="$2" -v width=76 '
        {
            prefix = marker
            line = ""
            count = split($0, word, /[[:space:]]+/)
            for (i = 1; i <= count; i++) {
                if (line == "") {
                    line = prefix word[i]
                } else if (length(line) + 1 + length(word[i]) <= width) {
                    line = line " " word[i]
                } else {
                    print line
                    prefix = indent
                    line = prefix word[i]
                }
            }
            if (line != "")
                print line
        }'
}

entry_body() {
    body=$(changelog_body "$1")
    test -n "$body" || body=$(tag_body "$2")
    printf '%s' "$body"
}

deb_entry() {
    # The suffix belongs to the Debian revision, not to the upstream version:
    # 6.1.0-1~ubuntu24.04.1 keeps the orig tarball named
    # mcommander_6.1.0.orig.tar.gz, so every series of a release shares one.
    printf 'mcommander (%s-1%s) %s; urgency=medium\n\n' "$1" "$4" "$3"
    if test -n "$5"; then
        printf '%s\n' "$5" | wrap_entries '  * ' '    '
    else
        printf '  * Release v%s.\n' "$1"
    fi
    printf '\n -- %s  %s\n\n' "$maintainer" "$2"
}

rpm_entry() {
    printf '* %s %s - %s-1\n' \
        "$(LC_ALL=C date -d "$2" '+%a %b %d %Y')" "$maintainer" "$1"
    if test -n "$3"; then
        printf '%s\n' "$3" | wrap_entries '- ' '  '
    else
        printf -- '- Release v%s.\n' "$1"
    fi
    printf '\n'
}

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/mcommander-prepare.XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

untagged=yes
printf '%s\n' $tags | grep -qx "v$version" && untagged=no

{
    test "$untagged" = no ||
        deb_entry "$version" "$(date -R)" UNRELEASED "$suffix" \
            "$(entry_body "$version" "v$version")"

    for tag in $tags; do
        tag_date=$(git for-each-ref --format='%(creatordate:rfc2822)' "refs/tags/$tag")
        test -n "$tag_date" || tag_date=$(date -R)

        if test "$tag" = "v$version"; then
            deb_entry "$version" "$tag_date" "$distribution" "$suffix" \
                "$(entry_body "$version" "$tag")"
        else
            # Older releases keep the plain revision: only the entry being
            # prepared is aimed at a series.
            deb_entry "${tag#v}" "$tag_date" unstable '' "$(entry_body "${tag#v}" "$tag")"
        fi
    done
} > debian/changelog

test -s debian/changelog || die "generated an empty debian/changelog"

{
    test "$untagged" = no ||
        rpm_entry "$version" "$(date -R)" "$(entry_body "$version" "v$version")"

    for tag in $tags; do
        tag_date=$(git for-each-ref --format='%(creatordate:rfc2822)' "refs/tags/$tag")
        test -n "$tag_date" || tag_date=$(date -R)
        rpm_entry "${tag#v}" "$tag_date" "$(entry_body "${tag#v}" "$tag")"
    done
} > "$tmpdir/rpm-changelog"

sed -e "s/@VERSION@/$version/g" packaging/rpm/mcommander.spec.in |
    sed -e "/^@CHANGELOG@\$/r $tmpdir/rpm-changelog" -e "/^@CHANGELOG@\$/d" \
        > packaging/rpm/mcommander.spec

sed -e "s/@VERSION@/$version/g" -e "s/@B2SUMS@/$checksum/g" \
    packaging/arch/PKGBUILD.in > packaging/arch/PKGBUILD

# Termux wants a SHA-256, and the source where a user of the repository gets it.
sed -e "s|@VERSION@|$version|g" \
    -e "s|@SRCURL@|https://github.com/blue-panels/mcommander/releases/download/v$version/mcommander-$version.tar.gz|" \
    -e "s|@SHA256@|$sha256|g" \
    packaging/termux/build.sh.in > packaging/termux/build.sh

rm -f packaging/gentoo/mcommander-*.ebuild
cp packaging/gentoo/mcommander.ebuild.in "packaging/gentoo/mcommander-$version.ebuild"

echo "prepared $version"
echo "  debian/changelog ($distribution)"
echo "  packaging/rpm/mcommander.spec"
echo "  packaging/arch/PKGBUILD (b2sums: $checksum)"
echo "  packaging/gentoo/mcommander-$version.ebuild"
echo "  packaging/termux/build.sh (sha256: $sha256)"
