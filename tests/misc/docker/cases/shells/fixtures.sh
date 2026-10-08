#!/bin/sh
# Build the shell cases under $1.
#
# The same questions under every shell mc can be told to drive: does it start,
# does Ctrl-O give a shell that runs a command, does a cd in that shell move
# the panel, is anything of mc left when it quits, and does mc quit when the
# shell does.  Which shell a run
# uses is the -s of run-cases.sh; the list of them is in shells.txt next to
# this script, and the last column of a row names the shells it is for.
#
# The panel follows a cd only where the terminal can teach the shell to report
# its directory: bash, zsh, fish, dash and the ash of busybox have a prompt
# hook or a PS1 it can drive, mksh and tcsh have neither, and there the panel
# is meant to stay where it was.
set -e

dir="${1:-/home/mc/cases/shells}"
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

mkdir -p 01-subshell/elsewhere
printf 'a file for the cursor to stand on\n' > 01-subshell/alpha.txt
printf 'the file the panel shows after a cd\n' > 01-subshell/elsewhere/moved-here.txt

cat > 01-subshell/cases.tsv <<'EOF'
file	key	expect	why	transports
alpha.txt	key Escape	text: alpha.txt	mc starts and draws the listing	local
alpha.txt	key C-o	text: 6ClrAll	Ctrl-O takes the panels away and leaves the shell, which has a key bar of its own	local
alpha.txt	key C-o,type echo SHELLMARK,key Enter	text: SHELLMARK	the shell behind the panels runs a command	local
alpha.txt	key C-o,type echo SHELLMARK,key Enter,key C-o	text: alpha.txt	and Ctrl-O brings the panels back	local
alpha.txt	key C-o,type cd elsewhere,key Enter,key C-o	text: moved-here.txt	a cd in the shell moves the panel with it	local	sh,bash,zsh,dash,ash,fish
alpha.txt	key C-o,type cd elsewhere,key Enter,key C-o	no text: moved-here.txt	mksh and tcsh have no prompt hook to hang the protocol on, so the panel stays where it was	local	mksh,tcsh
alpha.txt	key F10	no process left	quitting mc leaves nothing running	local
alpha.txt	key C-o,type exit,key Enter	no process left	the shell behind the panels leaves, and mc goes with it	local
alpha.txt	key C-o,type cat,key Enter,key C-d	text: 6ClrAll	Ctrl-D ends what reads it, and mc stays	local
alpha.txt	key C-o,key C-d	text: 6ClrAll	at the command line Ctrl-D deletes a character and does not end the shell	local	sh,bash,zsh,dash,ash,fish
alpha.txt	key C-o,key C-d	no process left	mksh and tcsh have no command line of mc's, they read Ctrl-D themselves and end, and mc with them	local	mksh,tcsh
EOF

echo "shells cases in $dir"
