#!/bin/sh
# Writes shelldef.h: every helper script of the shell link plugin as a C string.
# usage: gen-shelldef.sh <helpers-dir> <output> <helper>...

helpers_dir="$1"
out="$2"
shift 2

{
    echo '/* Generated from helpers/. Do not edit. */'
    echo
    echo '#ifndef MC__SHELLDEF_H'
    echo '#define MC__SHELLDEF_H'
    echo
    for h in "$@"; do
        # the newline: the sed of Solaris drops a last line without one
        n=`printf '%s\n' "$h" | sed \
            -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/' \
            -e 's/^FEXISTS$/EXISTS/'`
        printf '#define VFS_SHELL_%s_DEF_CONTENT ( \\\n' "$n" || exit 1
        sed -e 's/[\\"]/\\&/g' -e 's/.*/    "&\\n" \\/' "$helpers_dir/$h" || exit 1
        printf ')\n\n'
    done
    echo '#endif'
} > "$out-t" && mv -f "$out-t" "$out"
