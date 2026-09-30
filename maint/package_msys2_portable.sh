#!/usr/bin/env bash
#
# Pack an MSYS2 build of M-Commander together with the MSYS2 runtime it needs,
# so that it runs on Windows without an MSYS2 installation.
# Run it in an MSYS2 shell; .github/workflows/ci-msys2.yml calls it.
#
# Usage: package_msys2_portable.sh <staged-root> <portable-root>
#   staged-root    the DESTDIR of "make install"
#   portable-root  where the portable tree is made

set -euo pipefail

staged_root=$1
portable_root=$2

mkdir -p "$portable_root/usr" "$portable_root/bin" "$portable_root/tmp"
cp -aL "$staged_root/usr/local" "$portable_root/usr/"

# Copy complete runtime packages, including the magic database, shell support,
# terminal files and licenses. Resolve package symlinks so the artifact ZIP can
# be unpacked by Windows without requiring symlink privileges.
runtime_packages=(
    msys2-runtime filesystem bash mintty glib2 ncurses file
    coreutils findutils grep sed diffutils tar gzip bzip2 xz zstd unzip zip
)

{
    for package in "${runtime_packages[@]}"; do
        pactree -u "$package"
    done
} | sort -u > "$portable_root/package-names.txt"

while IFS= read -r package; do
    pacman -Q "$package" >> "$portable_root/MSYS2-PACKAGES.txt"
    while IFS= read -r path; do
        if test -f "$path"; then
            cp -aL --parents "$path" "$portable_root"
        fi
    done < <(pacman -Qlq "$package")
done < "$portable_root/package-names.txt"

cp -aL /usr/bin/sh.exe "$portable_root/bin/sh.exe"
cp -aL /usr/bin/bash.exe "$portable_root/bin/bash.exe"

test -f "$portable_root/usr/local/bin/mcommander.exe"
test -f "$portable_root/usr/bin/msys-2.0.dll"
test -f "$portable_root/usr/bin/msys-glib-2.0-0.dll"
test -f "$portable_root/usr/bin/msys-magic-1.dll"
test -f "$portable_root/usr/bin/mintty.exe"
test -f "$portable_root/usr/share/misc/magic.mgc"

printf '%s\r\n' \
    '@echo off' \
    'setlocal' \
    'set "PATH=%~dp0usr\bin;%PATH%"' \
    'set "MSYSTEM=MSYS"' \
    'set "TERM=xterm"' \
    'if /I "%~1"=="--version" (' \
    '  "%~dp0usr\local\bin\mcommander.exe" --version' \
    '  exit /b %ERRORLEVEL%' \
    ')' \
    '"%~dp0usr\bin\mintty.exe" -e /usr/local/bin/mcommander.exe' \
    > "$portable_root/run-mcommander.cmd"

cat > "$portable_root/README-PORTABLE.txt" <<'EOF'
M-Commander for Windows (MSYS2 portable bundle)

Extract this archive to any directory on a local drive. No separate MSYS2
installation is needed. Double-click run-mcommander.cmd to start the program.
Keep the usr, bin, and tmp directories together with the launcher.

This is an MSYS2 runtime bundle, not a native Windows build. It includes
Mintty and Bash for the terminal and shell features. Some optional external
helpers may still need additional programs. The Lua plugin is disabled.

MSYS2 package versions are listed in MSYS2-PACKAGES.txt. Package licenses
are included in usr/share/licenses. Source packages are available from
https://packages.msys2.org/.
EOF
