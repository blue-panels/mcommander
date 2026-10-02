#!/bin/sh
# Walk the cases.tsv files and press the keys.  Runs inside the mc container.
#
# usage: run-cases.sh [-c subject] [-w transports] [-l locale] [-o [sec.]key=val]...
#                     [-k keymap] [-r report] [-g] [-v] [case-dir...]
#
# Each row of a cases.tsv is "file, keys, expect, why, transports, shells".
# The last two are optional: a row with transports named runs only over those,
# and a row with shells named only under those (-s).  mc is
# started under tmux with the panel in that directory, the file is found by
# quick search, the keys are pressed in order and the screen is read.  A key
# is Enter, F3, F5, F6, F8, C-o, ".." (up one level), "on <name>" (the cursor goes
# there), "cd <path>" (through the Quick cd box), "type <text>",
# "key <name>" for anything tmux can send (F4, M-S, C-M-l, Escape, C-F1),
# "paste <text>" for a paste from the terminal (\n in it is a line break), or
# "width <n>" to make the terminal that many columns wide.
# Rows whose keys or expectation this script does not know are reported as
# skipped; a row with transports named runs only over those.
#
#   -c   the subject under cases/ (default archives)
#   -w   comma separated: local, sftp, ftp, smb, sh (default local)
#   -l   locale mc runs in; messages stay English (default ru_RU.UTF-8)
#   -s   the shell mc drives: bash, zsh, dash, ash, mksh, tcsh, fish.  The
#        image's own $SHELL when it is left out; a shell that is not in the
#        image stops the run rather than failing a case.
#   -o   an ini value written before mc starts, section Midnight-Commander
#        unless given: -o old_esc_mode=true -o Layout.message_visible=false
#   -k   a keymap from common/keymaps/ put in place as mc.keymap
#   -r   report directory (default /reports/<stamp>-<env>)
#   -g   run mc under valgrind memcheck: every wait is stretched, mc is asked
#        to quit instead of being killed so the summary is written, and a case
#        fails on an invalid access whatever the screen shows.  The log of
#        each case is kept next to its screen.
#
# The environment's envs/<env>/expect.tsv overrides expectations: columns
# "dir/file, key, expect, transports" (the last one optional, comma separated);
# "*" in the first two stands for any case or any key.
# An expectation "known: <why>" marks a failure that is understood: the case
# is still run, a failure is reported as "known", and a pass as "FIXED".
#
# What must come of it: "archive panel", "listing", "error dialog",
# "no process left" (mc was asked to quit and is gone),
# "nothing, no error", "the panel it came from", "extfs panel", "copy to the
# other panel" (the file is then in /tmp, the same size), "the name as
# written" (the file's name is on the screen, in the shell's output),
# "text: <what the screen must show>", "no text: <what it must not>" and
# "clipfile: <what mc copied>", which is read from the file mc keeps it in
# rather than off the screen.  mc's
# stderr is read too: an assertion or a critical warning fails the case
# whatever the screen says.
set -u

MC=/work/opt/mc/bin/mcommander
SRC=/src/tests/misc/docker
env_name=${SANDBOX_ENV:-unknown}
subject=archives
transports=local
locale=ru_RU.UTF-8
shell=
keymap=
report=
verbose=0
options=
memcheck=0
# every wait is multiplied by this: $SLOW for a slow machine, a CI runner
# among them, and six under valgrind, which makes mc some twenty times slower
# to start
slow=${SLOW:-1}

while getopts "c:w:l:s:o:k:r:vg" opt; do
    case "$opt" in
    c) subject=$OPTARG ;;
    w) transports=$OPTARG ;;
    l) locale=$OPTARG ;;
    s) shell=$OPTARG ;;
    o) options="$options
$OPTARG" ;;
    k) keymap=$OPTARG ;;
    r) report=$OPTARG ;;
    v) verbose=1 ;;
    g) memcheck=1 ;;
    *) exit 2 ;;
    esac
done
shift $((OPTIND - 1))

[ -x "$MC" ] || { echo "run-cases.sh: no mc at $MC, run build first" >&2; exit 2; }

shell_env=
shell_path=
if [ -n "$shell" ]; then
    shell_path=$(command -v "$shell" 2>/dev/null) || shell_path=
    [ -n "$shell_path" ] \
        || { echo "run-cases.sh: no $shell in this image" >&2; exit 2; }
    shell_env="SHELL=$shell_path"
fi

if [ $memcheck = 1 ]; then
    command -v valgrind >/dev/null 2>&1 \
        || { echo "run-cases.sh: -g needs valgrind in the image" >&2; exit 2; }
    slow=${SLOW:-6}
    # F10 must not stop at a question, or mc never exits and nothing is written
    options="$options
confirm_exit=false"
fi
[ "$slow" -ge 1 ] 2>/dev/null || slow=1

# a wait written for a plain run, stretched for a run under valgrind
nap ()
{
    if [ "$slow" = 1 ]; then
        sleep "$1"
    else
        sleep "$(awk -v a="$1" -v s="$slow" 'BEGIN { printf "%.2f", a * s }')"
    fi
}
# A subject the last build did not know about, or one built by an older build:
# its files are made here rather than asking for a rebuild.
if [ ! -d "/work/local/$subject" ] && [ -f "$SRC/cases/$subject/fixtures.sh" ]; then
    sh "$SRC/cases/$subject/fixtures.sh" "/work/local/$subject" >/dev/null
fi
[ -d "/work/local/$subject" ] || { echo "run-cases.sh: no local fixtures for $subject" >&2; exit 2; }

stamp=$(date +%Y-%m-%dT%H-%M-%S)
[ -n "$report" ] || report=/reports/$stamp-$env_name
mkdir -p "$report"
index=$report/index.md
expect_file=$SRC/envs/$env_name/expect.tsv

# ------------------------------------------------------------- mc's config ---

config=${XDG_CONFIG_HOME:-$HOME/.config}/mc6
mkdir -p "$config"

# where a copy lands: the same file the editor and the terminal write
clipfile=${XDG_DATA_HOME:-$HOME/.local/share}/mc6/mcedit/mcedit.clip

if [ -n "$keymap" ]; then
    cp "$SRC/common/keymaps/$keymap.keymap" "$config/mc.keymap" \
        || { echo "run-cases.sh: no keymap $keymap in common/keymaps" >&2; exit 2; }
fi

# What mc calls a learned key: this terminal sends a function key with a
# modifier as CSI 1;<mod><letter> for F1 to F4 and CSI <n>;<mod>~ above that,
# which the built-in table does not have.  mod 2 is shift, 3 alt, 5 ctrl.
# Without these, ctrl-f1 and its like reach mc as nothing at all.
term_keys ()
{
    mkdir -p "$config/term"
    {
        echo "[keys]"
        for mod in 2:shift 3:alt 5:ctrl; do
            n=${mod%%:*}
            name=${mod#*:}
            i=1
            for letter in P Q R S; do
                # printf, not echo: the escape has to reach the file as \e
                printf '%s-f%d=\\e[1;%s%s\n' "$name" "$i" "$n" "$letter"
                i=$((i + 1))
            done
            i=5
            for code in 15 17 18 19 20 21 23 24; do
                printf '%s-f%d=\\e[%s;%s~\n' "$name" "$i" "$code" "$n"
                i=$((i + 1))
            done
            # the editing keys carry the modifier the same way: insert is what
            # the terminal's own copy and paste hang on
            printf '%s-insert=\\e[2;%s~\n' "$name" "$n"
            printf '%s-delete=\\e[3;%s~\n' "$name" "$n"
            printf '%s-pgup=\\e[5;%s~\n' "$name" "$n"
            printf '%s-pgdn=\\e[6;%s~\n' "$name" "$n"
            for pair in A:up B:down C:right D:left H:home F:end; do
                printf '%s-%s=\\e[1;%s%s\n' "$name" "${pair#*:}" "$n" "${pair%%:*}"
            done
        done
    } > "$config/term/xterm-256color"
}

term_keys

# A subject may carry configuration of its own in cases/<name>/config: what is
# there is copied over mc's, which is how a format known only to arcmc.ini or a
# rule known only to magic.ini gets in front of mc without touching the build.
if [ -d "$SRC/cases/$subject/config" ]; then
    cp -r "$SRC/cases/$subject/config/." "$config/"
fi

# One value in mc's ini, section $1, key $2, value $3: what the subject brought
# stays, and what -o says wins over it.
ini_set ()
{
    awk -v sec="$1" -v key="$2" -v val="$3" '
    /^\[.*\]$/ {
        if (cur == sec && !done) { print key "=" val; done = 1 }
        cur = substr($0, 2, length($0) - 2)
    }
    cur == sec && index($0, key "=") == 1 { next }
    { print }
    END {
        if (!done) {
            if (cur != sec) print "[" sec "]"
            print key "=" val
        }
    }' "$config/ini" > "$config/ini.new"
    mv "$config/ini.new" "$config/ini"
}

# -o section.key=value, the section Midnight-Commander when it is left out
if [ -n "$options" ]; then
    [ -f "$config/ini" ] || : > "$config/ini"
    printf '%s\n' "$options" | grep . | while IFS= read -r opt; do
        key=${opt%%=*}
        val=${opt#*=}
        sec=Midnight-Commander
        case "$key" in
        *.*)
            sec=${key%%.*}
            key=${key#*.}
            ;;
        esac
        ini_set "$sec" "$key" "$val"
    done
fi

# the connection each plugin reads on start; plain passwords are accepted
printf '[sandbox]\nhost=remote\nuser=mc\npassword=mc\npath=/home/mc/cases/%s\nuse_agent=false\n\n[wrong-password]\nhost=remote\nuser=mc\npassword=wrong\nuse_agent=false\n\n[no-such-host]\nhost=no-such-host.invalid\nuser=mc\npassword=mc\nuse_agent=false\n' "$subject" \
    > "$config/sftp-connections.ini"
printf '[sandbox]\nhost=remote\nuser=mc\npassword=mc\npath=/home/mc/cases/%s\n' "$subject" \
    > "$config/shell-connections.ini"
# and two that must not come up, for the cases about why
printf '[sandbox]\nhost=remote\nuser=mc\npassword=mc\npath=/cases/%s\n\n[wrong-password]\nhost=remote\nuser=mc\npassword=wrong\n\n[no-such-host]\nhost=no-such-host.invalid\nuser=mc\n' "$subject" \
    > "$config/ftp-connections.ini"
printf '[sandbox]\nserver=remote\nshare=cases\nusername=mc\npassword=mc\n' \
    > "$config/smb-connections.ini"
# shell-link's libssh2 transport insists on a known_hosts file, and a host it
# does not know is a question; the answer is put there beforehand
mkdir -p "$HOME/.ssh" && chmod 700 "$HOME/.ssh"
case ",$transports," in
*,sh,*) ssh-keyscan remote > "$HOME/.ssh/known_hosts" 2>/dev/null ;;
esac

# ------------------------------------------------------------------ tmux ---

T="tmux -L cases"
screen ()
{
    $T capture-pane -t mc -p
}

# wait until the screen shows $1, up to $2 seconds of a plain run
wait_for ()
{
    n=$(( ${2:-10} * 5 * slow ))
    while [ "$n" -gt 0 ]; do
        screen | grep -qF -- "$1" && return 0
        sleep 0.2
        n=$((n - 1))
    done
    return 1
}

# Under memcheck the panel is drawn while mc is still coming up: the shell in
# the terminal has not printed its prompt yet, and a key pressed before that is
# dropped.  A plain run is past this by the time the panel shows.
settle ()
{
    [ $memcheck = 1 ] || return 0
    nap 2
}

# quick search puts the cursor on a name; the key that follows ends the search.
# While it is on, the mini status of the panel reads "/pattern", which is how
# a search that was not taken is told from one that was.
select_entry ()
{
    n=2
    while [ "$n" -gt 0 ]; do
        $T send-keys -t mc C-s
        nap 0.2
        $T send-keys -t mc -l -- "$1"
        nap 0.3
        screen | grep -qF "/$1" && return 0
        $T send-keys -t mc Escape
        nap 0.5
        n=$((n - 1))
    done
    echo "        quick search did not take: $1" >&2
    return 1
}

# the memcheck run, written to $vg_log; empty when -g was not given
#
# --child-silent-after-fork: mc forks a shell and the helpers of the plugins,
# and their logs would land in the same file.  --keep-debuginfo: a panel plugin
# is dlclosed before mc exits, and without it its frames are addresses.
vg_log=
vg_prefix ()
{
    [ $memcheck = 1 ] || return 0
    printf '%s' "valgrind --tool=memcheck --log-file='$vg_log' \
        --suppressions=$SRC/common/valgrind.supp \
        --child-silent-after-fork=yes --keep-debuginfo=yes \
        --leak-check=full --show-leak-kinds=definite --errors-for-leak-kinds=none \
        --track-origins=yes --num-callers=30 --error-limit=no "
}

# The files a case works on, made again when the last case changed them: a
# copy, a move or a delete leaves the tree as it is not, and the next case,
# like the next run, is meant to start where this one did.  Building them is
# cheaper than it looks, and a find over the tree is cheaper still, so it is
# only done when something moved.  The stamp lives outside the tree: a file in
# it would show up in the panel.
refresh_fixtures ()
{
    [ -f "$SRC/cases/$subject/fixtures.sh" ] || return 0

    # The local tree holds the cases themselves, whatever the transport is, and
    # for a case that copies into the remote host it holds what is copied.
    tree=/work/local/$subject
    stamp=/work/local/.stamp-$subject
    if [ ! -f "$stamp" ] || [ "$SRC/cases/$subject/fixtures.sh" -nt "$stamp" ] \
        || [ -n "$(find "$tree" -newer "$stamp" -print -quit 2>/dev/null)" ]; then
        sh "$SRC/cases/$subject/fixtures.sh" "$tree" >/dev/null
        touch "$stamp"
    fi

    [ "$1" = local ] && return 0

    # The remote tree is built when the image is, so what a case writes to it
    # would be handed to the next one.  A subject that says its fixtures can be
    # built again -- a rebuild-remote file next to them -- gets them built again
    # over ssh before each case.
    [ -f "$SRC/cases/$subject/rebuild-remote" ] || return 0
    if ! sshpass -p mc ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
        -o LogLevel=ERROR mc@remote "sh -s /home/mc/cases/$subject" \
        < "$SRC/cases/$subject/fixtures.sh" >/dev/null 2>&1; then
        # said out loud: a case that runs against what the last one left there
        # can pass without proving anything
        echo "  $subject: could not build the fixtures again on the remote host" >&2
        return 1
    fi
    return 0
}

# start mc with the panel in case directory $1 over transport $2; stderr to $3
start_mc ()
{
    $T kill-server 2>/dev/null
    # What mc keeps between runs is state one case would hand the next: the
    # cursor position per file, the history of every dialog.  A killed mc never
    # wrote any of it; one that is asked to quit, as it is under memcheck, does.
    rm -rf "${XDG_DATA_HOME:-$HOME/.local/share}/mc6"
    case "$2" in
    local) open_path=/work/local/$subject/$1 ;;
    *) open_path="$2:" ;;
    esac
    # messages in English so that the screen can be read, the charset as asked
    $T new-session -d -s mc -x 120 -y 40 \
        "env -u LC_ALL LANG=$locale LC_CTYPE=$locale LC_MESSAGES=en_US.UTF-8 TERM=xterm-256color \
         $shell_env \
         $(vg_prefix)$MC -S default '$open_path' /tmp 2>'$3'"
    case "$2" in
    local)
        wait_for cases.tsv 10 || return 1
        settle
        ;;
    *)
        # the plugin lists its connections; Enter on ours opens the subject
        # a listing shows up before the panel takes keys again, hence the sleeps
        wait_for sandbox 10 || return 1
        settle
        select_entry sandbox
        $T send-keys -t mc Enter
        wait_for "$subject" 30 || return 1
        nap 1
        # the samba share is the whole cases tree; the others land in the subject
        if [ "$2" = smb ]; then
            select_entry "$subject"
            $T send-keys -t mc Enter
            wait_for "cases/$subject" 30 || return 1
            nap 1
        fi
        select_entry "$1"
        $T send-keys -t mc Enter
        wait_for cases.tsv 30 || return 1
        nap 1
        settle
        ;;
    esac
    return 0
}

# A killed process writes no valgrind summary, so mc is asked to leave: Esc
# closes whatever dialog the case put up, F10 quits (confirm_exit is off).  If
# it is still there, SIGTERM still gets the summary out; SIGKILL never does.
# keep the screen of a case, unless it was kept before mc was asked to quit
save_screen ()
{
    [ -f "$out/$slug.screen" ] || screen > "$out/$slug.screen"
}

stop_mc ()
{
    if [ $memcheck = 0 ]; then
        $T kill-server 2>/dev/null
        return
    fi
    $T has-session -t mc 2>/dev/null || return
    # a case may have left the editor, the viewer or a dialog on screen, and
    # each of them takes an F10 of its own before the file manager sees one
    try=3
    while [ "$try" -gt 0 ] && $T has-session -t mc 2>/dev/null; do
        $T send-keys -t mc Escape
        nap 0.3
        $T send-keys -t mc Escape
        nap 0.3
        $T send-keys -t mc F10
        n=$(( 14 * slow ))
        while [ "$n" -gt 0 ] && $T has-session -t mc 2>/dev/null; do
            sleep 0.5
            n=$((n - 1))
        done
        try=$((try - 1))
    done
    if $T has-session -t mc 2>/dev/null; then
        pkill -TERM -f 'memcheck-.*-linux' 2>/dev/null
        n=$(( 20 * slow ))
        while [ "$n" -gt 0 ] && $T has-session -t mc 2>/dev/null; do
            sleep 0.5
            n=$((n - 1))
        done
    fi
    $T kill-server 2>/dev/null
}

# what the screen must show for an expectation
check ()
{
    case "$1" in
    "archive panel")
        screen | grep -q "Arcmc:" && ! screen | grep -q " Error "
        ;;
    "listing")
        screen | grep -q "4Hex" && ! screen | grep -q " Error "
        ;;
    "nothing, no error")
        ! screen | grep -q " Error " && ! screen | grep -q "Arcmc:" && screen | grep -qF "cases.tsv"
        ;;
    "error dialog")
        screen | grep -q " Error " && ! screen | grep -q "Arcmc:"
        ;;
    "the panel it came from")
        ! screen | grep -q " Error " && ! screen | grep -q "Arcmc:\|uzip://" && screen | grep -qF "cases.tsv"
        ;;
    "extfs panel")
        screen | grep -q "uzip://" && ! screen | grep -q " Error "
        ;;
    "copy to the other panel")
        # the copy lands in /tmp, the other panel, and must be as big as the
        # size mc shows for the source; give it a moment to finish
        want=$(screen | grep -F " $2 " | head -1 | awk -F'│' '{gsub(/ /, "", $3); print $3}')
        n=$(( 150 * slow ))
        while [ "$n" -gt 0 ]; do
            have=$(stat -c %s "/tmp/$2" 2>/dev/null)
            case "$want" in
            *[0-9]) [ "$have" = "$want" ] && break ;;
            *) [ -n "$have" ] && [ "$have" != 0 ] && [ "$have" = "$last" ] && break ;;
            esac
            last=$have
            sleep 0.2
            n=$((n - 1))
        done
        [ "$n" -gt 0 ] && ! screen | grep -q " Error "
        ;;
    "the name as written")
        screen | grep -qF -- "$2"
        ;;
    "no process left")
        # the case quit mc; neither it nor the shell it drove may still be here
        ! pgrep -x mc >/dev/null
        ;;
    "text: "*)
        screen | grep -qF -- "${1#text: }"
        ;;
    "no text: "*)
        ! screen | grep -qF -- "${1#no text: }"
        ;;
    "clipfile: "*)
        grep -qF -- "${1#clipfile: }" "$clipfile" 2>/dev/null
        ;;
    *)
        return 2
        ;;
    esac
}

# can every step of $1 be pressed?
steps_known ()
{
    printf '%s\n' "$1" | tr ',' '\n' | while read -r step; do
        case "$step" in
        Enter | F3 | F5 | F6 | F8 | C-o | .. | "on "* | "cd "* | "type "* | "key "* | "paste "* | "width "*) ;;
        *) exit 1 ;;
        esac
    done
}

press ()
{
    printf '%s\n' "$1" | tr ',' '\n' | while read -r step; do
        case "$step" in
        F5)
            $T send-keys -t mc F5
            # the copy box takes its time over a remote panel
            wait_for " Copy " 15 >/dev/null
            ;;
        F6)
            $T send-keys -t mc F6
            wait_for " Move " 15 >/dev/null
            ;;
        F8)
            # the question it opens is drawn at once; the nap below is the wait
            $T send-keys -t mc F8
            ;;
        Enter | F3 | C-o)
            $T send-keys -t mc "$step"
            ;;
        ..)
            # the first entry of a listing is ".."
            $T send-keys -t mc Home
            nap 0.3
            $T send-keys -t mc Enter
            ;;
        "on "*)
            select_entry "${step#on }"
            ;;
        "cd "*)
            $T send-keys -t mc M-c
            wait_for "cd:" 5 >/dev/null
            $T send-keys -t mc -l -- "${step#cd }"
            nap 0.3
            $T send-keys -t mc Enter
            ;;
        "type "*)
            # -- or a text that starts with a dash is read as tmux's own flags
            $T send-keys -t mc -l -- "${step#type }"
            ;;
        # anything tmux has a name for: F4, M-S, C-M-l, Escape, C-F1
        "key "*)
            $T send-keys -t mc "${step#key }"
            ;;
        # tmux sends it in ESC[200~ ... ESC[201~ when mc asked for bracketed paste;
        # the dot keeps the line breaks at the end from $(...)
        "paste "*)
            buf=$(printf '%b.' "${step#paste }")
            $T set-buffer -b case -- "${buf%.}"
            $T paste-buffer -p -d -b case -t mc
            ;;
        # a narrower terminal, for what has to be laid out again
        "width "*)
            $T resize-window -t mc -x "${step#width }" -y 40
            ;;
        esac
        nap 1.5
    done
}

# What memcheck says about the log $1, as "errors invalid opaque lost".
#
# An error whose whole stack is addresses without a name is code no one can
# point at: pcre2 compiles a regex to machine code of its own, and reads the
# subject a word at a time past its end, which memcheck sees as a jump on
# uninitialised bytes.  It cannot be suppressed by name either, since a
# suppression that matches an unnamed frame matches every error there is.  So
# it is counted apart: "opaque" is written down, "invalid" fails the case.
#
# A leak is written down as well and left to a person: mc frees little on the
# way out by design.
vg_counts ()
{
    [ -s "$1" ] || { echo "- - - -"; return; }
    awk '
        function flush(  ) {
            if (in_error) { if (named) invalid++; else opaque++ }
            in_error = 0
            in_head = 0
            named = 0
        }
        # the head of an error: one space after the pid, then what it is
        /^==[0-9]+== (Invalid read|Invalid write|Invalid free|Mismatched free|Conditional jump|Use of uninitialised|Jump to the invalid|Syscall param|Source and destination overlap|Process terminating)/ {
            flush()
            in_error = 1
            in_head = 1
            next
        }
        # a frame of the stack; "???" is an address with no name to it
        /^==[0-9]+==    (at|by) 0x/ {
            if (in_head && $0 !~ /: \?\?\?[[:space:]]*$/) named = 1
            next
        }
        # two spaces: where the value came from, whose frames are not the error
        /^==[0-9]+==  [A-Z]/ { in_head = 0; next }
        /^==[0-9]+==[[:space:]]*$/ { flush(); next }
        /^==[0-9]+== ERROR SUMMARY:/ { errs = $4 }
        /^==[0-9]+==[[:space:]]*definitely lost:/ { lost = $4; gsub(/,/, "", lost) }
        END {
            flush()
            printf "%s %d %d %s", (errs == "" ? "-" : errs), invalid + 0, opaque + 0, (lost == "" ? "-" : lost)
        }
    ' "$1"
}

# the environment's own expectation for dir/file + key over transport $3
override ()
{
    [ -f "$expect_file" ] || return 1
    awk -F'\t' -v f="$1" -v k="$2" -v w="$3" '
        ($1 == f || $1 == "*") && ($2 == k || $2 == "*") && ($4 == "" || index("," $4 ",", "," w ",") > 0) { print $3; found = 1 }
        END { exit found ? 0 : 1 }' "$expect_file"
}

now_ms ()
{
    date +%s%3N
}

# The file name a case's screen, stderr and memcheck log are kept under.  The
# report is uploaded as a CI artifact, which refuses " : < > | * ? and line
# breaks in a name; a key such as "cd uzip://" has a colon in it.
case_slug ()
{
    printf '%s' "$1" | tr '/ :"<>|*?\r\n' '...........'
}

# ------------------------------------------------------------------- run ---

if [ $# -eq 0 ]; then
    set -- $(cd "/work/local/$subject" && ls -d */ | sed 's,/$,,')
fi
for d in "$@"; do
    [ -f "/work/local/$subject/${d%/}/cases.tsv" ] && continue
    echo "run-cases.sh: no cases.tsv in $subject/${d%/}; have: $(cd "/work/local/$subject" && ls -d */ | tr -d / | tr '\n' ' ')" >&2
    exit 2
done
total_run=0

{
    echo "# $env_name: $subject"
    echo
    echo "- started: $stamp"
    echo "- locale: $locale"
    [ -n "$options" ] && echo "- ini: $(echo "$options" | grep . | tr '\n' ' ')"
    [ -n "$keymap" ] && echo "- keymap: $keymap"
    echo "- mc: $(readlink /work/opt/mc)"
    [ -n "$shell" ] && echo "- shell: $shell ($shell_path)"
    [ $memcheck = 1 ] && echo "- valgrind: memcheck, waits x$slow"
    echo
    echo "| transport | passed | failed | known | fixed | skipped |"
    echo "|-----------|--------|--------|-------|-------|---------|"
} > "$index"

total_fail=0
for where in $(echo "$transports" | tr ',' ' '); do
    out=$report/$where
    mkdir -p "$out"
    results=$out/results.tsv
    printf 'case\tkey\texpect\tverdict\tms\twhy\n' > "$results"
    memory=$out/valgrind.tsv
    [ $memcheck = 1 ] && printf 'case\tkey\terrors\tinvalid\topaque\tlost\tlog\n' > "$memory"

    echo "cases: $subject over $where"
    for d in "$@"; do
        d=${d%/}
        # the list of cases is read once per directory, so the files it comes
        # from are made again before that, not only before each case
        refresh_fixtures "$where"
        tsv=/work/local/$subject/$d/cases.tsv
        [ -f "$tsv" ] || { echo "  $d: no cases.tsv"; continue; }

        tail -n +2 "$tsv" | while IFS="$(printf '\t')" read -r file key expect why only shells; do
            [ -n "$file" ] || continue
            rm -f "/tmp/$file"
            # a row for some transports only
            if [ -n "$only" ] && ! echo ",$only," | grep -qF ",$where,"; then
                continue
            fi
            # a row for some shells only, when the run named one
            if [ -n "${shells:-}" ] && [ -n "$shell" ] \
                && ! echo ",$shells," | grep -qF ",$shell,"; then
                continue
            fi
            name="$d/$file"
            # the case before may have moved or deleted what this one works on
            refresh_fixtures "$where"
            # a row that names a situation rather than a file is for a person.
            # -e says no to a link that points at nothing, which is a file a
            # case may well be about.
            if [ ! -e "/work/local/$subject/$d/$file" ] \
                && [ ! -L "/work/local/$subject/$d/$file" ]; then
                printf '  skip  %-30s %-8s %s\n' "$name" "$key" "$why"
                printf '%s\t%s\t%s\tskip\t\t%s\n' "$name" "$key" "$expect" "$why" >> "$results"
                continue
            fi
            if ! steps_known "$key"; then
                printf '  skip  %-30s %-8s %s\n' "$name" "$key" "$why"
                printf '%s\t%s\t%s\tskip\t\t%s\n' "$name" "$key" "$expect" "$why" >> "$results"
                continue
            fi
            known=
            if o=$(override "$name" "$key" "$where"); then
                case "$o" in
                known:*)
                    known=$(echo "$o" | sed 's/^known: *//')
                    ;;
                *)
                    expect=$o
                    why="$why (expected by $env_name)"
                    ;;
                esac
            fi
            case "$expect" in
            "archive panel" | listing | "nothing, no error" | "error dialog" | "the panel it came from" \
                | "extfs panel" | "copy to the other panel" | "the name as written" \
                | "no process left" \
                | "text: "* | "no text: "* | "clipfile: "*) ;;
            *)
                printf '  skip  %-30s %-8s %s\n' "$name" "$key" "$expect"
                printf '%s\t%s\t%s\tskip\t\t%s\n' "$name" "$key" "$expect" "$why" >> "$results"
                continue
                ;;
            esac

            slug=$(case_slug "$name.$key")
            stderr=$out/$slug.stderr
            vg_log=
            [ $memcheck = 1 ] && vg_log=$out/$slug.valgrind
            rm -f "$vg_log"
            t0=$(now_ms)
            if ! start_mc "$d" "$where" "$stderr"; then
                screen > "$out/$slug.screen"
                if [ -n "$known" ]; then
                    printf '  known %-30s %-8s %s\n' "$name" "$key" "$known"
                    printf '%s\t%s\t%s\tknown\t%s\t%s\n' "$name" "$key" "$expect" "$(( $(now_ms) - t0 ))" "$known" >> "$results"
                else
                    printf '  FAIL  %-30s %-8s could not open %s over %s\n' "$name" "$key" "$d" "$where"
                    printf '%s\t%s\t%s\tFAIL\t%s\tcould not open the directory\n' "$name" "$key" "$expect" "$(( $(now_ms) - t0 ))" >> "$results"
                fi
                stop_mc
                continue
            fi
            select_entry "$file"
            press "$key"
            sleep 1
            # a remote file takes a moment to arrive, and under memcheck so does
            # a local one: wait for what is expected before reading the screen
            if [ "$where" != local ] || [ $memcheck = 1 ]; then
                case "$expect" in
                "archive panel") wait_for "Arcmc:" 20 >/dev/null ;;
                listing) wait_for "4Hex" 20 >/dev/null ;;
                "error dialog") wait_for " Error " 20 >/dev/null ;;
                "the panel it came from") wait_for "cases.tsv" 20 >/dev/null ;;
                "text: "*) wait_for "${expect#text: }" 20 >/dev/null ;;
                esac
            fi
            ms=$(( $(now_ms) - t0 ))
            verdict=ok
            fatal=0
            check "$expect" "$file" "$d" || verdict=FAIL
            if grep -qE "assert|CRITICAL|Segmentation|AddressSanitizer|runtime error" "$stderr" 2>/dev/null; then
                verdict=FAIL
                fatal=1
                why="$why; stderr: $(grep -m1 -E 'assert|CRITICAL|Segmentation|AddressSanitizer|runtime error' "$stderr")"
            fi
            # mc is asked to quit here, so that the summary is in the log
            if [ $memcheck = 1 ]; then
                # the screen goes first: quitting takes it away
                screen > "$out/$slug.screen"
                stop_mc
                vg_out=$(vg_counts "$vg_log")
                vg_errs=$(echo "$vg_out" | cut -d' ' -f1)
                vg_bad=$(echo "$vg_out" | cut -d' ' -f2)
                vg_opaque=$(echo "$vg_out" | cut -d' ' -f3)
                vg_lost=$(echo "$vg_out" | cut -d' ' -f4)
                printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
                    "$name" "$key" "$vg_errs" "$vg_bad" "$vg_opaque" "$vg_lost" "$slug.valgrind" >> "$memory"
                if [ "$vg_errs" = "-" ]; then
                    why="$why; valgrind wrote no summary (mc did not quit)"
                elif [ "$vg_bad" != 0 ]; then
                    verdict=FAIL
                    fatal=1
                    why="$why; valgrind: $vg_bad invalid access(es), $vg_errs errors"
                elif [ "$vg_lost" != "-" ] && [ "$vg_lost" != 0 ]; then
                    why="$why; valgrind: $vg_lost bytes definitely lost"
                fi
            fi
            # a known failure is one that was understood; a crash never is
            if [ -n "$known" ] && [ $fatal = 0 ]; then
                if [ $verdict = ok ]; then
                    verdict=FIXED
                    why="$why; was known to fail: $known"
                else
                    verdict=known
                    why="$known"
                fi
            fi
            case $verdict in
            ok)
                printf '  ok    %-30s %-8s %s  %sms\n' "$name" "$key" "$expect" "$ms"
                if [ $verbose = 1 ]; then
                    save_screen
                else
                    rm -f "$out/$slug.screen"
                fi
                ;;
            known)
                printf '  known %-30s %-8s %s\n' "$name" "$key" "$known"
                save_screen
                ;;
            FIXED)
                printf '  FIXED %-30s %-8s passes now, drop it from expect.tsv: %s\n' "$name" "$key" "$known"
                ;;
            *)
                printf '  FAIL  %-30s %-8s expected: %s\n' "$name" "$key" "$expect"
                save_screen
                sed 's/^/        | /' "$out/$slug.screen"
                ;;
            esac
            printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$name" "$key" "$expect" "$verdict" "$ms" "$why" >> "$results"
            stop_mc
        done
    done

    pass=$(grep -c "$(printf "\tok\t")" "$results")
    fail=$(grep -c "$(printf "\tFAIL\t")" "$results")
    skip=$(grep -c "$(printf "\tskip\t")" "$results")
    knownc=$(grep -c "$(printf "\tknown\t")" "$results")
    fixed=$(grep -c "$(printf "\tFIXED\t")" "$results")
    total_fail=$((total_fail + fail))
    total_run=$((total_run + pass + fail + knownc + fixed))
    echo "  $where: passed $pass, failed $fail, known $knownc, fixed $fixed, skipped $skip (for a person)"
    echo "| $where | $pass | $fail | $knownc | $fixed | $skip |" >> "$index"
done

{
    echo
    if [ "$total_fail" = 0 ]; then
        echo "No failures."
    else
        echo "## Failures"
        echo
        for where in $(echo "$transports" | tr ',' ' '); do
            grep "$(printf "\tFAIL\t")" "$report/$where/results.tsv" | while IFS="$(printf '\t')" read -r name key expect verdict ms why; do
                slug=$(case_slug "$name.$key")
                echo "- $where: $name $key, expected $expect ($why) - [screen]($where/$slug.screen)"
            done
        done
    fi
    if [ $memcheck = 1 ]; then
        echo
        echo "## Memory"
        echo
        echo "| transport | case | key | errors | invalid | opaque | definitely lost |"
        echo "|-----------|------|-----|--------|---------|--------|-----------------|"
        for where in $(echo "$transports" | tr ',' ' '); do
            [ -f "$report/$where/valgrind.tsv" ] || continue
            tail -n +2 "$report/$where/valgrind.tsv" \
            | while IFS="$(printf '\t')" read -r name key errs bad opaque lost log; do
                # a case with nothing to say is not worth a row
                [ "$errs" = 0 ] && [ "$lost" = 0 ] && continue
                echo "| $where | [$name]($where/$log) | $key | $errs | $bad | $opaque | $lost |"
            done
        done
        echo
        echo "opaque: an error with no named frame in its stack, which is the"
        echo "regex engine reading its subject a word at a time; it fails nothing."
    fi
    if grep -q "$(printf "\tknown\t\|\tFIXED\t")" "$report"/*/results.tsv; then
        echo
        echo "## Known"
        echo
        for where in $(echo "$transports" | tr ',' ' '); do
            grep "$(printf "\tknown\t\|\tFIXED\t")" "$report/$where/results.tsv" | while IFS="$(printf '\t')" read -r name key expect verdict ms why; do
                echo "- $where: $name $key: $verdict, $why"
            done
        done
    fi
    echo
    echo "- finished: $(date +%Y-%m-%dT%H-%M-%S)"
} >> "$index"

echo "report: $report"
if [ "$total_run" = 0 ]; then
    echo "run-cases.sh: nothing was run" >&2
    exit 1
fi
[ "$total_fail" = 0 ]
