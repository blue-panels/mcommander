#!/bin/sh
#
# M-Commander - check the GLib 2.58 API baseline.
#
# Copyright (C) 2026
# Ilia Maslakov <il.smind@gmail.com>
#
# This file is part of M-Commander.
#
# M-Commander is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# M-Commander is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#
# Usage: sh maint/check-glib-api.sh [configure options...]
# Requires Docker. GLIB_API_CHECK_JOBS sets parallelism (default: 4).
# Logs and build files are kept in the printed temporary directory.

set -eu

srcdir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
builddir=$(mktemp -d "${TMPDIR:-/tmp}/mc-glib-api.XXXXXX")
printf 'Checking against GLib 2.58; build and logs: %s\n' "$builddir"

if ! docker build -t mc-glib-api-258 -f "$srcdir/tests/misc/docker/glib-api/Dockerfile" \
    "$srcdir/tests/misc/docker/glib-api" > "$builddir/image.log" 2>&1; then
    tail -n 60 "$builddir/image.log"
    exit 1
fi
docker run --rm --user "$(id -u):$(id -g)" \
    -e GLIB_API_CHECK_JOBS="${GLIB_API_CHECK_JOBS:-4}" \
    -v "$srcdir:/src:ro" -v "$builddir:/work" \
    mc-glib-api-258 sh -eu -c '
        version=$(pkg-config --modversion glib-2.0)
        case "$version" in
            2.58.*) printf "GLib: %s\n" "$version" ;;
            *) printf "Expected GLib 2.58, found %s\n" "$version" >&2; exit 1 ;;
        esac
        mkdir /work/src /work/build
        tar -C /src --exclude=.git --exclude=.claude --exclude=.agents --exclude=.codex -cf - . | tar -C /work/src -xf -
        cd /work/src
        if ! ./autogen.sh > /work/autogen.log 2>&1; then
            tail -n 40 /work/autogen.log; exit 1
        fi
        cd /work/build
        if ! ../src/configure --enable-tests \
            CFLAGS="-O2 -g -Werror=implicit-function-declaration" "$@" > /work/configure.log 2>&1; then
            tail -n 40 /work/configure.log; exit 1
        fi
        if ! make -j"$GLIB_API_CHECK_JOBS" > /work/build.log 2>&1; then
            tail -n 60 /work/build.log; exit 1
        fi
        if ! make -j"$GLIB_API_CHECK_JOBS" check > /work/check.log 2>&1; then
            tail -n 60 /work/check.log; exit 1
        fi
        printf "PASS: build and tests with GLib %s.\n" "$version"
    ' sh "$@"
