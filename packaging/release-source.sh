#!/bin/sh
# Create the release source archive consumed by every packaging recipe.
#
# The archive is the tree of the tag, as "git archive" gives it, with one file
# added: .tarball-version, where version.sh finds the version outside a Git
# checkout. It holds no generated build files; the recipes run meson on it.
# The same tag always gives the same bytes.
set -eu

version=${1:?usage: packaging/release-source.sh VERSION [TAG [OUTPUT]]}
tag=${2:-v${version}}
output=${3:-dist/mcommander-${version}.tar.gz}

case "$version" in
    *[!0-9A-Za-z.+~:-]* | '')
        echo "invalid version: $version" >&2
        exit 2
        ;;
esac

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output_dir=$(dirname -- "$output")

git -C "$root" rev-parse --verify --quiet "${tag}^{commit}" >/dev/null
commit_time=$(git -C "$root" show -s --format=%ct "${tag}^{commit}")
if [ -e "$output" ]; then
    echo "refusing to overwrite existing archive: $output" >&2
    exit 1
fi

mkdir -p "$output_dir"
stage=$(mktemp -d "${TMPDIR:-/tmp}/mcommander-release.XXXXXX")
trap 'rm -rf "$stage"' EXIT HUP INT TERM

git -C "$root" archive --format=tar --prefix="mcommander-${version}/" "$tag" |
    tar -xf - -C "$stage"

# outside a Git checkout version.sh has nothing else to go by; the tags are v<version>
printf '%s\n' "v${version}" > "$stage/mcommander-${version}/.tarball-version"

# one timestamp and one owner for everything: the same tag, the same bytes
tar --sort=name --mtime="@${commit_time}" --owner=0 --group=0 --numeric-owner \
    -C "$stage" -cf - "mcommander-${version}" | gzip -n > "$output"
printf '%s\n' "created $output"
