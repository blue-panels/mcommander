#!/bin/sh
# Build the editor cases under $1.
#
# What the editor got in this milestone: a line filter in the Search dialog
# (F7 -> Filter, Alt-S to lift), Alt-Shift-S to filter by the word under the
# cursor at once, the Search dialog opening with the selected text, 8-bit
# files drawn as text rather than as dots, and a paste taken as one block.
set -e

dir="${1:-/home/mc/cases/editor}"
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

# ------------------------------------------------------------------ filter ---

mkdir -p 01-filter

# The word to filter by is the first one in the file, so the cursor is on it
# the moment the editor opens.  Every third line carries it; the others carry
# a word of their own, so that a filtered view can be told from a whole one.
{
    printf 'needle at the top of the file\n'
    i=1
    while [ "$i" -le 30 ]; do
        if [ $((i % 3)) = 0 ]; then
            printf 'line %02d with a needle in it\n' "$i"
        else
            printf 'line %02d plain, chaff only\n' "$i"
        fi
        i=$((i + 1))
    done
} > 01-filter/log.txt

cat > 01-filter/cases.tsv <<'EOF'
file	key	expect	why	transports
log.txt	key F4	text: 2Save	F4 opens the editor and its button bar	local
log.txt	key F4,key M-S	no text: chaff only	Alt-Shift-S filters by the word under the cursor	local
log.txt	key F4,key M-S,key M-s	text: chaff only	Alt-S lifts the filter	local
log.txt	key F4,key F7,type needle,key Enter	text: needle	the search dialog finds the word	local
log.txt	key F4,key F3,key Down,key F3,key F7	text: line 01	the search dialog opens with the selected text	local
log.txt	key F4,key F3,key End,key F3,key C-Insert	clipfile: needle at the top	Ctrl-Insert copies the marked text into the clipfile	local
EOF

# ---------------------------------------------------------------- prefill ---

mkdir -p 03-search

# The word is on the first line and again far below the last row of the
# screen, so a search that starts from the selected text is told from one that
# does not by whether the view moved.
{
    printf 'zebra\n'
    i=1
    while [ "$i" -le 60 ]; do
        printf 'filler line %02d\n' "$i"
        i=$((i + 1))
    done
    printf 'zebra ENDMARK\n'
} > 03-search/zebra.txt

cat > 03-search/cases.tsv <<'EOF'
file	key	expect	why	transports
zebra.txt	key F4,key F3,key End,key F3,key F7,key Enter	text: ENDMARK	the search dialog opens with the marked word and finds it below	local
zebra.txt	key F4	no text: ENDMARK	without the search it is off the screen, which is what the case rests on	local
EOF

# ------------------------------------------------------------------- paste ---

# A bracketed paste goes into the editor as one block: one step for Undo, no
# auto indent, and a control byte in it is text or nothing, never a command.
mkdir -p 04-paste
: > 04-paste/empty.txt

cat > 04-paste/cases.tsv <<'EOF'
file	key	expect	why	transports
empty.txt	key F4,paste alpha\nbravo\ncharlie	text: charlie	a paste from the terminal goes into the text	local
empty.txt	key F4,paste alpha\nbravo\ncharlie,key C-u	no text: alpha	one Undo takes the whole paste back, not its last character	local
empty.txt	key F4,paste keep1\nkeep2\0031tail\nkeep3	text: keep2tail	a control byte in the paste is dropped: Ctrl-Y there deletes no line	local
empty.txt	key F4,paste if x:\n    y	no text:         y	a pasted line keeps its own indent and gets no auto indent on top of it	local
EOF

# ------------------------------------------------------------- 8-bit files ---

mkdir -p 02-charset

# The same sentence in three encodings.  A file in an 8-bit encoding comes out
# as dots until its encoding is picked, which is a dialog and a list to walk,
# so these rows are for a person to press: open each one, pick the encoding
# with Ctrl-T, and read what is drawn.
printf 'кириллица в utf-8\nвторая строка\n' > 02-charset/utf8.txt
printf 'кириллица в utf-8\nвторая строка\n' | iconv -f UTF-8 -t CP1251 > 02-charset/cp1251.txt
printf 'кириллица в utf-8\nвторая строка\n' | iconv -f UTF-8 -t KOI8-R > 02-charset/koi8r.txt

# Every byte CP866 draws, a row per high nibble.  The row that matters is 80:
# that is where CP866 keeps А to П and where the ISO 8859 codepages have
# control characters instead, so a viewer that decides what is printable from
# the terminal rather than from the codepage draws the first two rows of
# Cyrillic as dots and the rest of the alphabet, at E0, as letters.  The bytes
# are written one by one: iconv would need a table of the characters, and this
# needs the codepage to say what they are.
{
    hi=32
    while [ "$hi" -lt 256 ]; do
        printf '%02X ' "$hi"
        lo=0
        while [ "$lo" -lt 16 ]; do
            printf ' '
            printf "\\$(printf '%03o' $((hi + lo)))"
            lo=$((lo + 1))
        done
        printf '\n'
        hi=$((hi + 16))
    done
} > 02-charset/cp866-table.txt

cat > 02-charset/cases.tsv <<'EOF'
file	key	expect	why	transports
utf8.txt	key F4	text: кириллица	a UTF-8 file on a UTF-8 terminal	local
cp1251.txt	key F4	the text, not dots, once Ctrl-T picks CP1251	for a person: the encoding is picked from a list	local
koi8r.txt	key F4	the text, not dots, once Ctrl-T picks KOI8-R	for a person: same, in the other encoding	local
cp866-table.txt	key F3,key M-e,type 9	text: IBM866	CP 866 is entry 9 of the codepage list, and the viewer names the codepage it was given	local
cp866-table.txt	key F3,key M-e,type 9	no text: 80  .	the 80 row is Cyrillic, and a viewer that reads it as the ISO 8859 control range draws dots	local
cp866-table.txt	key F3,key M-e,type 9	the whole table, with letters at 80, 90, A0, E0 and F0	for a person: tmux keeps no 8-bit byte, so only a terminal shows the rows themselves	local
EOF

echo "editor cases in $dir"
