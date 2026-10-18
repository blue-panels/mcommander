#!/bin/sh

# Midnight Commander - calculate current version
#
# Copyright (C) 2009, 2010, 2013
# The Free Software Foundation, Inc.
#
# Written by:
#  Slava Zanko <slavazanko@gmail.com>, 2009, 2010, 2013
#  Stan. S. Krupoderov <pashelper@gmail.com>, 2009
#  Sergei Trofimovich <slyfox@inbox.ru>, 2009
#  Oswald Buddenhagen <ossi@kde.org>, 2009
#
# This file is part of the Midnight Commander.
#
# The Midnight Commander is free software: you can redistribute it
# and/or modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the License,
# or (at your option) any later version.
#
# The Midnight Commander is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# usage: version.sh <toplevel-source-dir> [--short]
#
# Prints the version: "git describe" in a git checkout, the contents of
# .tarball-version in a release archive, "unknown" otherwise. --short prints
# the last tag without the distance to it, for the project version, which
# must not change with every commit.

if [ -z "$1" ]; then
    echo "usage: $0 <toplevel-source-dir> [--short]" >&2
    exit 1
fi

src_top_dir="$1"
short="$2"

if git --git-dir "${src_top_dir}/.git" rev-parse --verify -q HEAD >/dev/null 2>&1; then
    if [ "${short}" = "--short" ]; then
        version=`git --git-dir "${src_top_dir}/.git" describe --always --abbrev=0 2>/dev/null`
    else
        version=`git --git-dir "${src_top_dir}/.git" describe --always 2>/dev/null`
    fi
elif [ -r "${src_top_dir}/.tarball-version" ]; then
    version=`cat "${src_top_dir}/.tarball-version"`
fi

echo "${version:-unknown}"
