#! /bin/sh
#
# Format lib/, src/ and tests/ with clang-format.
#
# Only one version of clang-format formats this tree: another one moves code
# back and forth and the CI then fails on files nobody touched.  The wanted
# version is below; it is the one the CI installs (CLANG_FORMAT_VERSION in
# .github/workflows/ci-fedora.yml).  The script takes the first candidate that
# reports it, so a distribution clang-format of another version in PATH does no
# harm.  CLANG_FORMAT=<path> picks one by hand.
#
# Run it from the top of the source tree, or with the tree as an argument.

set -e

WANTED=20.1.3

top=${1-.}

version_of ()
{
    "$1" --version 2>/dev/null | sed -e 's/.*version //' -e 's/[^0-9.].*//'
}

if [ -n "$CLANG_FORMAT" ]; then
    got=$(version_of "$CLANG_FORMAT")
    if [ "$got" != "$WANTED" ]; then
        echo "$0: $CLANG_FORMAT is version ${got:-unknown}, this tree is formatted with $WANTED" >&2
        exit 1
    fi
else
    for candidate in clang-format clang-format-20 "$HOME/.local/bin/clang-format"; do
        if [ "$(version_of "$candidate")" = "$WANTED" ]; then
            CLANG_FORMAT=$candidate
            break
        fi
    done

    if [ -z "$CLANG_FORMAT" ]; then
        echo "$0: clang-format $WANTED not found." >&2
        echo "Install it with 'pip install clang-format==$WANTED'," >&2
        echo "or name the binary: make indent CLANG_FORMAT=/path/to/clang-format" >&2
        exit 1
    fi
fi

echo "$CLANG_FORMAT $WANTED"

for directory in "$top/lib" "$top/src" "$top/tests"; do
    find "$directory" -name '*.[ch]' -print0 | xargs -0 "$CLANG_FORMAT" -i
done
