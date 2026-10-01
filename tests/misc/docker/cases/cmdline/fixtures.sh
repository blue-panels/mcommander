#!/bin/sh
# Build the command line cases under $1.
#
# Copy and paste on the command line through the clipfile: Ctrl-Insert with
# nothing selected takes the marked files, else the line, else the file under
# the cursor; Shift-Insert pastes the clipfile as one line, with the panels
# shown or hidden, and asks first when it is over 2 KB.  The clipfile for a
# paste is written by the shell in the terminal, so every case is local.
set -e

dir="${1:-/home/mc/cases/cmdline}"
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

mkdir -p 01-clip
printf 'a file for the cursor to stand on\n' > 01-clip/alpha.txt
printf 'and a second one to mark\n' > 01-clip/bravo.txt

# The shell writes the clipfile where mc reads it; the directory may not exist yet.
clip='~/.local/share/mc6/mcedit'
# What mc keeps between runs is thrown away before every case, the directory
# of the clipfile with it, so each shell command makes it again.
m="mkdir -p $clip &&"
w="$m printf"

cat > 01-clip/cases.tsv <<EOF2
file	key	expect	why	transports
alpha.txt	key Insert,key C-Insert	clipfile: alpha.txt	Ctrl-Insert with a marked file puts its name in the clipfile	local
alpha.txt	key Insert,key Insert,key C-Insert	clipfile: alpha.txt	every marked file: the first	local
alpha.txt	key Insert,key Insert,key C-Insert	clipfile: bravo.txt	and the second, on a line of its own	local
alpha.txt	key Escape,type echo LINEMARK,key C-Insert	clipfile: LINEMARK	nothing marked: the whole command line	local
alpha.txt	key Insert,type echo LINEMARK,key C-Insert	clipfile: alpha.txt	the marked files come before the line	local
alpha.txt	key C-Insert	clipfile: alpha.txt	nothing marked and nothing typed: the file under the cursor	local
alpha.txt	key C-o,type $w 'one\\ntwo\\r\\nthree\\n' > $clip/mcedit.clip,key Enter,key C-o,key S-Insert	text: one two three	Shift-Insert pastes the clipfile as one line: line breaks become spaces	local
alpha.txt	key C-o,type $w 'foo\\rbar\\0baz' > $clip/mcedit.clip,key Enter,key C-o,key S-Insert	text: foo bar baz	a lone CR and a NUL byte are breaks too, not glue and not the end	local
alpha.txt	key C-o,type $w 'one\\ntwo\\n' > $clip/mcedit.clip,key Enter,key S-Insert	text: one two	with the panels hidden the paste goes to the shell's line	local
alpha.txt	key C-o,type $m head -c 3000 /dev/zero | tr '\\0' x > $clip/mcedit.clip,key Enter,key C-o,key S-Insert	text: 2 KB	more than 2 KB asks first	local
alpha.txt	key C-o,type $m head -c 3000 /dev/zero | tr '\\0' x > $clip/mcedit.clip,key Enter,key C-o,key S-Insert,key Escape	no text: xxxxxxxxxx	and Escape leaves the line alone	local
alpha.txt	key C-o,type $m head -c 3000 /dev/zero | tr '\\0' x > $clip/mcedit.clip,key Enter,key C-o,key S-Insert,key Enter	text: xxxxxxxxxx	Enter pastes it all the same	local
alpha.txt	key C-o,type $w 'PASTEMARK' > $clip/mcedit.clip,key Enter,key C-o,key S-Insert,type -TAIL,key C-Insert	clipfile: PASTEMARK-TAIL	Ctrl-Insert on the shell's own line copies that line, as it is now	local
EOF2

# A paste from the terminal (Shift and the mouse, Ctrl-Shift-V) comes in
# ESC[200~ ... ESC[201~.  The shell gets it as one block and runs nothing
# until Enter; echo shows whether a line ran: "42AA" is only on the screen
# when the line was run, the line itself reads "$((6*7))AA".
mkdir -p 02-paste
printf 'a file for the cursor to stand on\n' > 02-paste/alpha.txt
printf 'and a second one to find\n' > 02-paste/bravo.txt

two='echo $((6*7))AA\necho $((6*7))BB\n'

cat > 02-paste/cases.tsv <<EOF2
file	key	expect	why	transports
alpha.txt	paste $two	no text: 42AA	with the panels up a paste of two lines runs neither	local
alpha.txt	paste $two,key Enter,key C-o	text: 42BB	Enter runs both	local
alpha.txt	key C-o,paste $two	no text: 42AA	after Ctrl-O a paste runs nothing either	local
alpha.txt	key C-o,paste $two,key Enter	text: 42BB	and Enter runs both	local
alpha.txt	key C-o,type bind 'set enable-bracketed-paste off' 2>/dev/null; unset zle_bracketed_paste,key Enter,paste $two,key Enter	text: 42AA echo 42BB	a shell that did not ask for bracketed paste (bash or zsh) gets the lines as one	local
alpha.txt	key F7,paste one\\ntwo\\n,key Enter	text: one two	a dialog input takes the paste as one line and is not submitted by its line break	local
alpha.txt	key Escape,key C-s,paste bra,F3	text: second one to find	quick search takes the paste	local
EOF2

echo "cmdline cases in $dir"
