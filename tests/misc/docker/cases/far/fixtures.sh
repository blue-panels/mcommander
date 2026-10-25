#!/bin/sh
# Build the Far mode cases under $1.
#
# The fast find of Far: in Far mode, with the fast find on (config/ini), Alt
# with a character starts the quick search and types the character into it.
# Each case ends the quick search the run started with (Esc), types with Alt,
# and opens the file the cursor found in the viewer (F3), whose text says which
# file it is.  The names in Cyrillic are written in UTF-8 and in the 8-bit
# Russian charsets, a directory each, and a run in a locale (-l) takes the directory of its
# charset: that is the charset the terminal sends the keys in.
set -e

dir="${1:-/home/mc/cases/far}"
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

mkdir -p 01-ascii/delta 01-ascii/gamma
printf 'this is alpha\n'   > 01-ascii/alpha.txt
printf 'this is Apricot\n' > 01-ascii/Apricot.txt
printf 'this is beta\n'    > 01-ascii/beta.txt
printf 'this is bravo\n'   > 01-ascii/bravo.txt
printf 'this is cherry\n'  > 01-ascii/cherry.txt
printf 'inside\n'          > 01-ascii/delta/inside-delta.txt
printf 'inside\n'          > 01-ascii/gamma/inside-gamma.txt

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
cases.tsv	key Escape,type echo,alt al,F3	text: this is alpha	with text on the command line an Alt key it has no action for still goes to the fast find	local
cases.tsv	key Escape,type echo,alt ga,Enter	text: inside-gamma.txt	Enter enters the directory found and does not run the command line	local
cases.tsv	key Escape,type ech,alt b,type X	text: Xech	with text on the command line its own Alt keys stay its own: Alt-b is a word left	local
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
    printf 'this is yolka\n'   > "$1/$(printf 'ёлка.txt' | iconv -f UTF-8 -t "$2")"
    printf 'this is apple\n'   > "$1/$(printf 'яблоко.txt' | iconv -f UTF-8 -t "$2")"

    cat > "$1/cases.tsv" <<CASES
file	key	expect	why	transports
cases.tsv	key Escape,alt аль,F3	text: this is alfa	Alt with Cyrillic letters in $2	local
cases.tsv	key Escape,alt бе,F3	text: this is beta	the second letter picks between two names	local
cases.tsv	key Escape,alt бо,F3	text: this is borscht	the case of a Cyrillic name does not count	local
cases.tsv	key Escape,alt альz,F3	text: this is alfa	a letter that matches nothing is not taken	local
cases.tsv	key Escape,alt ёл,F3	text: this is yolka	the letter yo, which lies apart from the rest of the alphabet	local
cases.tsv	key Escape,alt ЁЛ,F3	text: this is yolka	Alt-Shift with yo: the capital finds the small one	local
cases.tsv	key Escape,alt яб,F3	text: this is apple	the letter ya, the last byte of CP1251	local
cases.tsv	key Escape,alt ЯБ,F3	text: this is apple	the capital of ya finds the small one	local
CASES
}

cyrillic 02-utf8 UTF-8
cyrillic 03-koi8r KOI8-R
cyrillic 04-cp866 CP866
cyrillic 06-cp1251 CP1251
cyrillic 07-iso88595 ISO-8859-5
cyrillic 08-koi8u KOI8-U

# A plugin panel that turns a key down: the Git panel runs coole on its hotkey,
# Alt-q here (config/panels.git.ini), and with no coole in the image it says so
# in a message and answers that the user turned it down (MC_PPR_SKIPPED).  For
# a key outside the keymap of the panels that is no answer, as on master: once
# the message is gone the key goes on to the fast find.  The Git panel needs
# git, which the remote host has not, and the cases are local only.
if command -v git >/dev/null 2>&1; then
    mkdir -p 05-plugin
    printf 'this is apple\n'  > 05-plugin/apple.txt
    printf 'this is quince\n' > 05-plugin/quince.txt
    git -C 05-plugin init -q
    git -C 05-plugin add apple.txt quince.txt

    cat > 05-plugin/cases.tsv <<CASES
file	key	expect	why	transports
cases.tsv	key Escape,type cd git:$dir/05-plugin,Enter,alt q	text: Cannot find coole executable	the Git panel takes Alt-q and puts up its message	local
cases.tsv	key Escape,type cd git:$dir/05-plugin,Enter,alt q,key Escape,type u	text: /qu	the plugin turned the key down, so it goes on to the fast find and the letters after it too	local
CASES
fi

echo "far cases in $dir"
