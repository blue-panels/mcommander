#!/bin/sh
# Run the shell-link helper scripts against the sandbox SSH server.
set -eu

TOOL=/work/build-all/src/panel-plugins/shell-link/shfs-tool
SSH="sshpass -p mc ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR mc@remote"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

[ -x "$TOOL" ]

"$TOOL" --helper append --dump > "$tmp/append"
"$TOOL" --helper info --dump > "$tmp/info"

# append must add exactly the announced bytes and keep the shell protocol in
# sync with its ### status replies.
$SSH "printf prefix > /home/mc/append-test"
$SSH "cat > /tmp/mc-append-helper" < "$tmp/append"
output=$(printf ' tail' | $SSH "SHELL_FILENAME=home/mc/append-test SHELL_FILESIZE=5 SHELL_HAVE_DD=1 sh /tmp/mc-append-helper")
printf '%s\n' "$output" | grep -q '^### 001$'
printf '%s\n' "$output" | grep -q '^### 200$'
[ "$($SSH 'cat /home/mc/append-test')" = 'prefix tail' ]
echo "append-helper: ok"

# info must advertise dd support when the remote shell has it.
output=$($SSH sh -s < "$tmp/info")
bits=$(printf '%s\n' "$output" | sed -n '1p')
[ $((bits & 1024)) -ne 0 ]
printf '%s\n' "$output" | grep -q '^### 200$'
echo "info-helper: ok"
