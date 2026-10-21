#!/bin/sh
# Build the Far mode cases under $1.
#
# The fast find of Far: in Far mode, with the fast find on (config/ini), Alt
# with a character starts the quick search and types the character into it.
# Each case ends the quick search the run started with (Esc), types with Alt,
# and opens the file the cursor found in the viewer (F3), whose text says which
# file it is.  The names in Cyrillic are written in UTF-8, KOI8-R and CP866,
# a directory each, and a run in a locale (-l) takes the directory of its
# charset: that is the charset the terminal sends the keys in.
set -e

dir="${1:-/home/mc/cases/far}"
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

mkdir -p 01-ascii/delta
printf 'this is alpha\n'   > 01-ascii/alpha.txt
printf 'this is Apricot\n' > 01-ascii/Apricot.txt
printf 'this is beta\n'    > 01-ascii/beta.txt
printf 'this is bravo\n'   > 01-ascii/bravo.txt
printf 'this is cherry\n'  > 01-ascii/cherry.txt
printf 'inside\n'          > 01-ascii/delta/inside-delta.txt

cat > 01-ascii/cases.tsv <<'CASES'
file	key	expect	why	transports
cases.tsv	key Escape,alt al,F3	text: this is alpha	Alt with letters finds the file whose name begins with them	local
cases.tsv	key Escape,alt ap,F3	text: this is Apricot	the case of the name does not count, as in Far	local
cases.tsv	key Escape,key M-B,key M-R,F3	text: this is bravo	Alt-Shift types too	local
cases.tsv	key Escape,alt b,type r,F3	text: this is bravo	once the search runs the letters go on without Alt	local
cases.tsv	key Escape,alt alz,F3	text: this is alpha	a letter that matches nothing is not taken	local
cases.tsv	key Escape,alt ch,F3	text: this is cherry	Alt-c is no longer quick cd: it is the fast find	local
cases.tsv	key Escape,alt c	no text: cd:	and the quick cd line does not open	local
cases.tsv	key Escape,alt de,Enter	text: inside-delta.txt	Enter ends the search and enters the directory found	local
cases.tsv	key Escape,type echo,alt al,F3	text: this is alpha	with text on the command line Alt still goes to the fast find	local
cases.tsv	key Escape,type echo,alt de,Enter	text: inside-delta.txt	Enter enters the directory found and does not run the command line	local
cases.tsv	key Escape,alt al,key Escape,alt be,F3	text: this is beta	Esc ends the search and the next Alt starts a new one	local
CASES

# The same three names in each charset: the bytes of the charset go after the
# Esc of Alt, and a UTF-8 character is the rest of its bytes after that.
cyrillic ()
{
    mkdir -p "$1"
    printf '%s\n' "$2" > "$1/charset"
    printf 'this is alfa\n'    > "$1/$(printf 'альфа.txt' | iconv -f UTF-8 -t "$2")"
    printf 'this is beta\n'    > "$1/$(printf 'бета.txt' | iconv -f UTF-8 -t "$2")"
    printf 'this is borscht\n' > "$1/$(printf 'Борщ.txt' | iconv -f UTF-8 -t "$2")"

    cat > "$1/cases.tsv" <<CASES
file	key	expect	why	transports
cases.tsv	key Escape,alt аль,F3	text: this is alfa	Alt with Cyrillic letters in $2	local
cases.tsv	key Escape,alt бе,F3	text: this is beta	the second letter picks between two names	local
cases.tsv	key Escape,alt бо,F3	text: this is borscht	the case of a Cyrillic name does not count	local
cases.tsv	key Escape,alt альz,F3	text: this is alfa	a letter that matches nothing is not taken	local
CASES
}

cyrillic 02-utf8 UTF-8
cyrillic 03-koi8r KOI8-R
cyrillic 04-cp866 CP866

echo "far cases in $dir"
