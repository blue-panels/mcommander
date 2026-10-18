#!/bin/sh
# Build the tree mounted at /src and install it under /work/opt.  The sources
# are copied first, so the working tree stays as it was and root-owned build
# output stays in the container volume.
#
# usage: build-mc.sh [-f profiles]
#
# Profiles come from features.ini; each set of them gets its own build and
# install directory, and /work/opt/mc points at the one built last, which is
# the one run-cases.sh runs.
set -e

features=${FEATURES:-all}
while getopts "f:" opt; do
    case "$opt" in
    f) features=$OPTARG ;;
    *) exit 2 ;;
    esac
done

ini=/usr/local/bin/features.ini

# "a,b,-c": the flags of a and b, then those of c taken out.  A -Dfeature=...
# flag taken out becomes -Dfeature=disabled, since its own default is auto.
profile_flags ()
{
    key=$1
    shift
    for name in $(echo "$features" | tr ',' ' '); do
        case "$name" in
        -*) continue ;;
        esac
        sed -n "/^\[$name\]/,/^\[/p" "$ini" | sed -n "s/^$key *= *//p"
    done | tr ' ' '\n' | grep . | sort -u > /tmp/flags.$$
    for name in $(echo "$features" | tr ',' ' '); do
        case "$name" in
        -*)
            for flag in $(sed -n "/^\[${name#-}\]/,/^\[/p" "$ini" | sed -n "s/^$key *= *//p"); do
                base=${flag%%=*}
                grep -vE "^$base(=.*)?\$" /tmp/flags.$$ > /tmp/flags2.$$ || true
                mv /tmp/flags2.$$ /tmp/flags.$$
                case "$base" in
                -Dpanel_plugin_* | -Dlua) echo "$base=disabled" >> /tmp/flags.$$ ;;
                esac
            done
            ;;
        esac
    done
    tr '\n' ' ' < /tmp/flags.$$
    rm -f /tmp/flags.$$
}

meson_flags=$(profile_flags meson)
cflags=$(profile_flags cflags)
tag=$(echo "$features" | tr ',' '+')

copy_sources ()
{
    rsync -a --delete \
        --exclude '.git' \
        --exclude 'tests/misc/docker/' \
        /src/ /work/src/
}

echo "== copying sources =="
mkdir -p /work/src
copy_sources

[ -f /work/src/meson.build ] || { echo "no meson.build in /work/src: is the tree mounted at /src?" >&2; exit 1; }

# Everything lives in the volume: the container that builds mc is not the one
# that runs it.  One tree per set of features.
PREFIX=/work/opt/mc-$tag
BUILD=/work/build-$tag

# meson remakes its own files when a meson.build changes; other flags need a new setup
if [ ! -f "$BUILD/build.ninja" ] || [ "$(cat "$BUILD.flags" 2>/dev/null)" != "$meson_flags|$cflags" ]; then
    echo "== meson setup [$features] =="
    echo "   $meson_flags"
    [ -n "$cflags" ] && echo "   CFLAGS: $cflags"
    rm -rf "$BUILD"
    # shellcheck disable=SC2086
    CFLAGS="$cflags" meson setup "$BUILD" /work/src --prefix="$PREFIX" $meson_flags
    echo "$meson_flags|$cflags" > "$BUILD.flags"
fi

echo "== build =="
meson compile -C "$BUILD"
meson install -C "$BUILD" --quiet

# a volume from before the profiles has a real directory here
if [ -d /work/opt/mc ] && [ ! -L /work/opt/mc ]; then
    mv /work/opt/mc /work/opt/mc-before-profiles
fi
rm -f /work/opt/mc
ln -s "$PREFIX" /work/opt/mc

# The same cases on a local panel, for what does not need a server.  An
# environment that exists to check the build may not have an archiver at all.
# Made again on every build: cases.tsv comes from here, and it is cheap.
for f in /src/tests/misc/docker/cases/*/fixtures.sh; do
    subject=$(basename "$(dirname "$f")")
    if command -v bsdtar >/dev/null; then
        echo "== local fixtures: $subject =="
        sh "$f" "/work/local/$subject" >/dev/null
    else
        echo "== local fixtures skipped: no bsdtar in this image =="
    fi
done

echo
echo "mc installed:      $PREFIX/bin/mcommander  (features: $features)"
echo "panel plugins in:  $(ls -d "$PREFIX"/lib/mcommander/panel-plugins/* 2>/dev/null | xargs -n1 basename 2>/dev/null | tr '\n' ' ')"
