#!/bin/sh
# Exercise the SFTP panel authentication paths in the sandbox container.
set -eu

MC=/work/opt/mc/bin/mcommander
CONFIG=${XDG_CONFIG_HOME:-$HOME/.config}/mc6
TMUX="tmux -L sftp-auth"

cleanup ()
{
    $TMUX kill-server 2>/dev/null || true
}
trap cleanup EXIT

mkdir -p "$HOME/.ssh" "$CONFIG"
chmod 700 "$HOME/.ssh"

authorize ()
{
    sshpass -p mc ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
        -o LogLevel=ERROR mc@remote \
        "mkdir -p .ssh && cat >> .ssh/authorized_keys && chmod 700 .ssh && chmod 600 .ssh/authorized_keys" \
        < "$1"
}

screen () { $TMUX capture-pane -t mc -p; }
wait_for ()
{
    n=150
    while [ "$n" -gt 0 ]; do
        screen | grep -qF -- "$1" && return 0
        sleep 0.2
        n=$((n - 1))
    done
    echo "sftp-auth: did not see: $1" >&2
    screen >&2
    return 1
}

start_mc ()
{
    $TMUX kill-server 2>/dev/null || true
    rm -f /tmp/sftp-auth.err
    $TMUX new-session -d -s mc -x 120 -y 40 \
        "TERM=xterm-256color LANG=en_US.UTF-8 LC_MESSAGES=en_US.UTF-8 $MC -S default sftp: /tmp 2>/tmp/sftp-auth.err"
    wait_for sandbox
    $TMUX send-keys -t mc C-s
    sleep 0.2
    $TMUX send-keys -t mc -l sandbox
    sleep 0.2
    $TMUX send-keys -t mc Enter
}

assert_connected ()
{
    wait_for "sftp:remote:/home/mc/cases/panel"
}

# 1. An unconfigured connection uses ~/.ssh/id_ed25519 automatically.
ssh-keygen -q -t ed25519 -N '' -f "$HOME/.ssh/id_ed25519"
authorize "$HOME/.ssh/id_ed25519.pub"
cat > "$CONFIG/sftp-connections.ini" <<'EOF'
[sandbox]
host=remote
user=mc
path=/home/mc/cases/panel
use_agent=false
EOF
start_mc
assert_connected
echo "default-key-auth: ok"

# 2. Keyboard-interactive asks for the server challenge and connects.
cat > "$CONFIG/sftp-connections.ini" <<'EOF'
[sandbox]
host=remote
port=2222
user=mc
path=/home/mc/cases/panel
use_agent=false
EOF
start_mc
wait_for "SFTP authentication"
wait_for "Password:"
$TMUX send-keys -t mc -l mc
$TMUX send-keys -t mc Enter
assert_connected
echo "keyboard-interactive-auth: ok"

# 3. An encrypted default key asks for its passphrase every time.  The second
# connection proves the one-time response was not persisted in the config.
rm -f "$HOME/.ssh/id_ed25519" "$HOME/.ssh/id_ed25519.pub"
ssh-keygen -q -t ed25519 -N test-passphrase -f "$HOME/.ssh/id_ed25519"
authorize "$HOME/.ssh/id_ed25519.pub"
cat > "$CONFIG/sftp-connections.ini" <<'EOF'
[sandbox]
host=remote
user=mc
path=/home/mc/cases/panel
use_agent=false
EOF
start_mc
wait_for "SFTP key passphrase"
$TMUX send-keys -t mc -l test-passphrase
$TMUX send-keys -t mc Enter
assert_connected

start_mc
wait_for "SFTP key passphrase"
echo "passphrase-not-saved: ok"

