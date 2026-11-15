#! /bin/sh
#
# Generate doc/man/roff/*.1.in from doc/man/*.md.
#
# Needs go-md2man and perl.  It is a maintainer tool: the generated files
# are in the repository, so building from a checkout needs neither.
#
# Run it from the top of the source tree, or with the tree as an argument.

set -e

top=${1-.}
MD2MAN=${MD2MAN-go-md2man}
PERL=${PERL-perl}
SED=${SED-sed}

for tool in "$MD2MAN" "$PERL"; do
    if ! command -v "$tool" > /dev/null 2>&1; then
        echo "$0: $tool not found" >&2
        exit 1
    fi
done

# Every page is of section 1 and of this manual; only the date is its own,
# and it stands in the markdown, in front of the text it belongs to.
SECTION=1
SOURCE="MCommander {{MAN_VERSION}}"
MANUAL="M-Commander"

for md in "$top"/doc/man/*.md "$top"/doc/man/*/*.md; do
    [ -f "$md" ] || continue

    name=`basename "$md" .md`
    out=`dirname "$md"`/roff/$name.$SECTION.in
    title=`echo "$name" | tr '[a-z]' '[A-Z]'`
    date=`$SED -n 's/^date: *//p' "$md" | head -n 1`

    if [ -z "$date" ]; then
        echo "$0: $md: no date in the front matter" >&2
        exit 1
    fi

    mkdir -p "`dirname \"$out\"`"
    echo ".\\\" Generated from $name.md by maint/update-man-in.sh. Do not edit." \
        > "$out.tmp"

    # go-md2man wants the title of the page in its own first line, and the
    # front matter would end up as a rule and a heading, so neither reaches it
    { echo "% $title $SECTION \"$date\" \"$SOURCE\" \"$MANUAL\""
      $SED -e '1,/^---$/d' "$md"
    } | $MD2MAN | $PERL -0777 -pe '
        # go-md2man says what it can in roff, and these five rules say the
        # rest.  Order matters: an internal link must go before rule 4, or
        # it would be turned into a url.

        # 1. .nh turns hyphenation off for the whole page, which refills
        #    every paragraph of it
        s/\A\.nh\n//;

        # 2. the anchor of a help node is html, and what it leaves behind
        #    is an empty paragraph
        s/\n\.PP\n\n+(?=\.[SU][HS])/\n/g;

        # 3. a link to a help node has no url to print, only its text; the node may
        #    live in the page of another program of the suite
        s/\n?\\\[la\](?:[A-Za-z0-9_-]+\.md)?#(?:[^\\]|\\.)*?\\\[ra\]//g;

        # 4. a link to the outside is clickable where OSC 8 is understood
        s{\n+\\\[la\](\S+)\\\[ra\]}{\n\\X'"'"'tty: link $1'"'"'$1\\X'"'"'tty: link'"'"'}g;

        # 5. a hyphen is a minus sign: it prints the same and keeps groff
        #    from breaking a word at it
        s/(?<!\\)-/\\-/g;

        # 6. roff reads an apostrophe at the start of a line as a control
        #    character
        s/^'"'"'/\\&'"'"'/gm;

        # 7. what the anchor of a heading leaves behind it
        s/[ \t]+$//gm;

        # 8. a backslash of its own is \e in these pages; \\ is what mandoc
        #    tells man pages not to write
        s/\\\\/\\e/g;

        # 9. a code block that ends with an empty line: go-md2man 2.0.3 keeps
        #    the line before .EE, 2.0.4 does not; neither does this page
        s/\n\n+(?=\.EE\n)/\n/g;
    ' >> "$out.tmp"
    mv -f "$out.tmp" "$out"
    echo "$out"
done
