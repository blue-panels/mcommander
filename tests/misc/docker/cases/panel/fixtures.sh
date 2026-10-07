#!/bin/sh
# Build the panel cases under $1.
#
# What the panel got in this milestone: a quick filter that hides what does
# not match and shares its pattern with quick search, a quick cd that is an
# input line in the panel rather than a dialog, and a permission field with a
# colour per bit.
set -e

dir="${1:-/home/mc/cases/panel}"
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

mkdir -p 01-filter/sub
printf 'first\n'  > 01-filter/alpha-one.txt
printf 'second\n' > 01-filter/alpha-two.txt
printf 'third\n'  > 01-filter/bravo-one.txt
printf 'fourth\n' > 01-filter/bravo-two.txt
printf 'inside the subdirectory\n' > 01-filter/sub/inside.txt

cat > 01-filter/cases.tsv <<'EOF'
file	key	expect	why	transports
alpha-one.txt	key Escape,key M-S,type alpha	no text: bravo-one.txt	the quick filter hides what does not match	local
alpha-one.txt	key Escape,key M-S,type alpha	text: alpha-two.txt	and keeps what does	local
alpha-one.txt	key Escape,key M-S,type alpha,key C-g	text: bravo-one.txt	Ctrl-G brings the whole listing back	local
alpha-one.txt	key M-c,type sub,key Enter	text: inside.txt	quick cd is an input line in the panel	local
alpha-one.txt	key M-?	text: Find File	the find dialog is reachable from the panel	local
EOF

# The permission field takes a colour per bit, which a captured screen has no
# way of showing: the text is the same either way.
mkdir -p 02-permissions
printf 'plain\n'      > 02-permissions/plain.txt
printf 'executable\n' > 02-permissions/runnable.sh
chmod 755 02-permissions/runnable.sh
printf 'setuid\n'     > 02-permissions/setuid.bin
chmod 4755 02-permissions/setuid.bin
printf 'nobody\n'     > 02-permissions/locked.txt
chmod 400 02-permissions/locked.txt

cat > 02-permissions/cases.tsv <<'EOF'
file	key	expect	why	transports
runnable.sh	key F9	r, w, x, s and - each in a colour of its own	for a person: turn Permission colors on in the panel options; a captured screen carries no colour	local
setuid.bin	key F9	the s bit in the colour of a special bit	for a person: same, and the marked triplet keeps its colour on top	local
EOF

# A connection that does not come up: the status window keeps what happened,
# the reason among it, until it is closed, and closing it does not try again.
# The connections come from run-cases.sh; ".." goes from here back to the list
# of them: four times over ftp, where the path is /cases/panel/..., and six
# times over sftp, where it is /home/mc/cases/panel/...
mkdir -p 03-plugin-connect

cat > 03-plugin-connect/cases.tsv <<'EOF'
file	key	expect	why	transports
cases.tsv	..,..,..,..,on wrong-password,Enter	text: 530 Login incorrect.	a refused login shows what the server said	ftp
cases.tsv	..,..,..,..,on wrong-password,Enter	text: Authentication with remote:21 failed	and what that means	ftp
cases.tsv	..,..,..,..,on wrong-password,Enter,Enter,width 120,width 120	no text: 530 Login incorrect.	Close ends it: the connection is not tried a second time	ftp
cases.tsv	..,..,..,..,on no-such-host,Enter	text: Could not resolve host: no-such-host.invalid	a host that is not there says so, and asks for no password	ftp
cases.tsv	..,..,..,..,..,..,on no-such-host,Enter	text: Name or service not known	a host that is not there says why	sftp
cases.tsv	..,..,..,..,..,..,on no-such-host,Enter	text: Connecting to no-such-host.invalid:22...	and the steps before it stay on screen	sftp
cases.tsv	..,..,..,..,..,..,on no-such-host,Enter,Enter,width 120,width 120	no text: Cannot connect to no-such-host.invalid:22	Close ends it: the connection is not tried a second time	sftp
cases.tsv	..,..,..,..,..,..,on wrong-password,Enter,type stillwrong,Enter	text: Authentication failed (username/password)	a refused password says so	sftp
cases.tsv	..,..,..,..,..,..,on wrong-password,Enter,type stillwrong,Enter	text: Authenticating with password...	under the steps that led to it	sftp
EOF

# Tagging files: every key that tags, one file or a run of them, on an empty
# file each, so the panel says "0 B in N files" for exactly the files tagged.
# They sit in a directory of their own: the cases.tsv beside them would be
# tagged too, with a size of its own.
mkdir -p 04-mark/files
for n in $(seq -w 1 60); do
    : > "04-mark/files/a$n"
done

cat > 04-mark/cases.tsv <<'EOF'
file	key	expect	why	transports
files	Enter,on a05,key IC	text: 0 B in 1 file	Insert tags the file under the bar	local
files	Enter,on a05,key IC,key Up,key IC	no text: 0 B in	Insert on a tagged file untags it	local
files	Enter,on a05,key C-t	text: 0 B in 1 file	Ctrl-T does what Insert does	local
files	Enter,on a05,key S-Down,key S-Down,key S-Down	text: 0 B in 3 files	Shift-Down tags a file and moves on	local
files	Enter,on a10,key S-Up,key S-Up,key S-Up	text: 0 B in 3 files	Shift-Up does it upwards	local
files	Enter,on a05,key S-End	text: 0 B in 56 files	Shift-End tags down to the last file	local
files	Enter,on a10,key S-Home	text: 0 B in 10 files	Shift-Home tags up to the first one	local
files	Enter,on a01,key S-NPage	text: 0 B in 31 files	Shift-PgDn tags as far as PgDn goes	local
files	Enter,on a60,key S-PPage	text: 0 B in 31 files	Shift-PgUp tags as far as PgUp goes	local
files	Enter,on a01,key S-Right	text: 0 B in 32 files	Shift-Right tags a column down	local
files	Enter,on a60,key S-Left	text: 0 B in 32 files	Shift-Left tags a column up	local
files	Enter,on a30,key S-End,key S-Home	text: 0 B in 60 files	a run of these keys goes on tagging, the way its first file went	local
files	Enter,on a01,key S-End,key Up,key S-Home	text: 0 B in 1 file	a run that starts on a tagged file untags	local
files	Enter,on a60,key S-Home,key IC,key S-End	no text: 0 B in	Insert ends a run: the next one starts on a tagged file and untags	local
files	Enter,on a05,key Escape,key M-S,type a1,key S-End	text: 0 B in 10 files	under the quick filter only the files it shows are tagged	local
files	Enter,on a05,key Escape,key M-S,type a1,key S-End	text: Filter: a1	and the filter stays on	local
files	Enter,on a05,key Escape,key M-S,type a1,key End,key S-Home	text: 0 B in 10 files	the same upwards, from the last file it shows	local
files	Enter,on a05,key Escape,type abc,key S-End,key S-Home,key S-Left,key S-Right	no text: 0 B in	with text on the command line these keys are the line's	local
files	Enter,on a05,key Escape,type abc,key S-End	no text: [1;2F	and the shell does not print them	local
files	Enter,on a05,key Escape,type abc,key S-Down	text: 0 B in 1 file	Shift-Down still tags, the line has no use for it	local
EOF

echo "panel cases in $dir"
