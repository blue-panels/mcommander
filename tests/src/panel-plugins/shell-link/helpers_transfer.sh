#!/bin/sh
# Test error replies from the helpers which change a remote file.

set -eu

helpers="${srcdir:-.}/../../../../src/panel-plugins/shell-link/helpers"
tmp=$(mktemp -d) || exit 99
trap 'rm -rf "$tmp"' EXIT

fail=0

expect_error() {
    case "$1" in
        *'### 500'*) ;;
        *)
            echo "expected ### 500, got: $1" >&2
            fail=1
            ;;
    esac
}

expect_success() {
    case "$1" in
        *'### 001'*'### 200'*) ;;
        *)
            echo "expected ### 001 followed by ### 200, got: $1" >&2
            fail=1
            ;;
    esac
}

# rm -f used to claim success here. A missing path must be reported to the panel.
missing="$tmp/missing"
output=$(SHELL_FILENAME="${missing#/}" /bin/sh "$helpers/unlink" 2>&1)
expect_error "$output"

for helper in send append; do
    target="$tmp/${helper}-success"
    expected=hello
    if [ "$helper" = append ]; then
        printf old > "$target"
        expected=oldhello
    fi
    output=$(printf hello | SHELL_FILENAME="${target#/}" SHELL_FILESIZE=5 SHELL_HAVE_DD=1 \
        /bin/sh "$helpers/$helper" 2>&1)
    expect_success "$output"
    if [ "$(cat "$target")" != "$expected" ]; then
        echo "$helper did not write the expected data" >&2
        fail=1
    fi
done

# tee must keep forwarding data after its write fails, then the helper drains the announced
# payload and returns an error. This mock has tee's required behaviour for that situation.
mkdir "$tmp/bin"
cat > "$tmp/bin/tee" <<'EOF'
#!/bin/sh
cat
echo 'mock tee: write failed' >&2
exit 1
EOF
chmod +x "$tmp/bin/tee"

for helper in send append; do
    target="$tmp/$helper"
    output=$(printf 'hello' | PATH="$tmp/bin:$PATH" SHELL_FILENAME="${target#/}" \
        SHELL_FILESIZE=5 SHELL_HAVE_DD=1 /bin/sh "$helpers/$helper" 2>&1)
    expect_error "$output"
done

exit "$fail"
