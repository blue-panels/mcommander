#!/bin/bash
# Install the x86_64 package in a Termux of its own and run it.
#
# usage: packaging/termux/smoke.sh DIRECTORY
#   DIRECTORY holds mcommander_*_x86_64.deb, as made by
#   packaging/termux/build-deb.sh x86_64
#
# Termux for x86_64 runs in Docker (termux/termux-docker), with the Android
# linker and the libc of Termux: what the package needs is installed by apt from
# the Termux repository, as a user would get it.  Then the program is run in
# pseudo-terminals by tests/misc/resurrect.py: it starts, draws its panel, and
# takes over a terminal that another one has lost.
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)

dir=${1:?usage: packaging/termux/smoke.sh DIRECTORY}
dir=$(CDPATH= cd -- "$dir" && pwd)
ls "$dir"/mcommander_*_x86_64.deb >/dev/null

docker run --rm --volume "$dir:/debs:ro" \
    --volume "$root/tests/misc/resurrect.py:/resurrect.py:ro" \
    termux/termux-docker:x86_64 bash -c '
    set -e
    apt-get update
    apt-get install -y /debs/mcommander_*_x86_64.deb
    mcommander --version
    mcommander --datadir-info
    MC_BIN="$PREFIX/bin/mcommander" python /resurrect.py
'
