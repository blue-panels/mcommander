#!/bin/sh
# Every skin key the code reads must have a row in the label table.

top="${srcdir:-.}/../../../.."
tmp="${TMPDIR:-/tmp}/skinedit_table.$$"
trap 'rm -f "$tmp.code" "$tmp.table"' EXIT

{
    grep -rhoE 'mc_skin_color_get \("[^"]+", "[^"]+"\)' "$top/src" "$top/lib" \
        | sed -E 's/.*\("([^"]+)", "([^"]+)"\)/\1 \2/'
    grep -rhoE 'mc_skin_get \("[^"]+", "[^"]+"' "$top/src" "$top/lib" \
        | sed -E 's/.*\("([^"]+)", "([^"]+)"/\1 \2/'
    grep -rhoE 'skin_get_char \(mc_skin, "[^"]+", "[^"]+"' "$top/lib/skin" \
        | sed -E 's/.*\("?[^"]*"?, "([^"]+)", "([^"]+)"/\1 \2/'
} | grep -v '^skin ' | sort -u > "$tmp.code"

./skinedit_table_dump | sort -u > "$tmp.table"

missing=$(comm -23 "$tmp.code" "$tmp.table")
if [ -n "$missing" ]; then
    echo "keys read by the code but absent from the table:"
    echo "$missing"
    exit 1
fi
exit 0
