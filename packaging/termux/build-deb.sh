#!/bin/bash
# Build the Termux package of M-Commander with the package builder of
# termux-packages, the way the Termux repository builds its own.
#
# usage: packaging/termux/build-deb.sh ARCH [ARCHIVE]
#   packaging/termux/build-deb.sh aarch64
#   packaging/termux/build-deb.sh arm dist/mcommander-6.1.0.tar.gz
#
#   ARCH     aarch64, arm, x86_64 or i686: the names Termux gives its targets
#   ARCHIVE  the release archive, made by packaging/release-source.sh. Without
#            one the checked-out commit is turned into an archive of the same
#            kind, with a version of its own (0.0.0+ci.<commit>).
#
# Needs Docker, git and the tools release-source.sh needs (autoconf, automake,
# libtool, gettext, autopoint) when it has to make the archive. The package
# builder is the image of termux-packages, which brings the Android NDK with
# it; the libraries mcommander needs are downloaded as Termux packages.
#
# Environment:
#   TERMUX_OUTPUT         where the .deb goes (default: termux-out)
#   TERMUX_WORKDIR        the clone of termux-packages and the archive
#   TERMUX_PACKAGES_REF   branch or tag of termux-packages (default: master)
#
# The .deb is named mcommander_VERSION_ARCH.deb and is installed in Termux with
# `pkg install ./mcommander_VERSION_ARCH.deb`.
set -euo pipefail

die() {
    echo "$*" >&2
    exit 1
}

arch=${1:?usage: packaging/termux/build-deb.sh ARCH [ARCHIVE]}
archive=${2:-}

case "$arch" in
    aarch64 | arm | x86_64 | i686) ;;
    *) die "unknown architecture: $arch (aarch64, arm, x86_64 or i686)" ;;
esac

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
work=${TERMUX_WORKDIR:-${RUNNER_TEMP:-/tmp}/mcommander-termux}
out=${TERMUX_OUTPUT:-$PWD/termux-out}
packages_ref=${TERMUX_PACKAGES_REF:-master}

mkdir -p "$work" "$out"

if [ -z "$archive" ]; then
    version="0.0.0+ci.$(git -C "$root" rev-parse --short HEAD)"
    archive="$work/mcommander-$version.tar.gz"
    rm -f "$archive"
    "$root/packaging/release-source.sh" "$version" HEAD "$archive"
else
    name=$(basename -- "$archive")
    version=${name#mcommander-}
    version=${version%.tar.gz}
fi
test -f "$archive" || die "no such archive: $archive"

tp="$work/termux-packages"
if [ ! -d "$tp" ]; then
    git clone --quiet --depth 1 --branch "$packages_ref" \
        https://github.com/termux/termux-packages "$tp"
fi

# The builder sees the clone as /home/builder/termux-packages, and the archive
# is fetched from there: the source of the package is this commit, not a
# download.
cp -- "$archive" "$tp/mcommander-source.tar.gz"
mkdir -p "$tp/packages/mcommander"
sed -e "s|@VERSION@|$version|g" \
    -e "s|@SRCURL@|file:///home/builder/termux-packages/mcommander-source.tar.gz|" \
    -e "s|@SHA256@|SKIP_CHECKSUM|" \
    "$root/packaging/termux/build.sh.in" > "$tp/packages/mcommander/build.sh"

(
    cd "$tp"
    ./scripts/run-docker.sh ./build-package.sh -I -C -a "$arch" mcommander
)

cp -- "$tp"/output/mcommander_*.deb "$out/"
ls -l "$out"
