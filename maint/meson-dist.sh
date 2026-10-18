#!/bin/sh
# Run by "meson dist" in the tree it is about to pack: a release archive has no
# git history, so it keeps its version in a file of its own.
set -e
"${MESON_SOURCE_ROOT}/version.sh" "${MESON_SOURCE_ROOT}" > "${MESON_DIST_ROOT}/.tarball-version"
