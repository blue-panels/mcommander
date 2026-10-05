#! /usr/bin/perl
#
# Check doc/man/*.md against what the man page pipeline can carry.
#
# go-md2man is quiet about what it cannot do: a table needs a preprocessor
# the pages do not ask for, a backslash inside a link label eats the rest
# of the label, two emphasis runs that touch print their own asterisks.
# This says so instead.
#
# Usage: maint/check-man.pl [tree]

use strict;
use warnings;

my $top = shift // '.';
my @files = sort (glob("$top/doc/man/*.md"), glob("$top/doc/man/*/*.md"));

die "$0: no markdown man pages under $top/doc/man\n" unless @files;

# what the build substitutes; anything else is a typo
my %placeholder = map { $_ => 1 } qw(
    MAN_VERSION sysconfdir libexecdir pkglibexecdir pkgdatadir
    panel_plugins_dir bindir
);

my %marker = map { $_ => 1 } qw(skip notitle topics break);

my ($errors, $warnings) = (0, 0);

sub err  { my ($f, $n, $m) = @_; print "$f:$n: error: $m\n";   $errors++ }
sub warn_ { my ($f, $n, $m) = @_; print "$f:$n: warning: $m\n"; $warnings++ }

sub slug
{
    my ($t) = @_;
    $t =~ s/[*`]|\\(?=.)//g;
    $t = lc $t;
    $t =~ s/[^\w\s-]//g;
    $t =~ s/^\s+|\s+$//g;
    $t =~ s/\s+/-/g;
    return $t;
}

for my $file (@files)
{
    open my $fh, '<:encoding(UTF-8)', $file or die "$0: $file: $!\n";
    my @lines = <$fh>;
    close $fh;
    chomp @lines;

    my (%anchor, @links, $fenced, $front);

    # the help window shows a page together with the template of its language,
    # and builds the contents page itself
    $anchor{'contents'} = 1;
    if ($file =~ m{^(.*)/doc/man/(?:([a-z]+)/)?mcommander\.md$})
    {
        my $tmpl = $2 ? "$1/doc/hlp/$2/xnc.md" : "$1/doc/hlp/xnc.md";

        if (open my $th, '<:encoding(UTF-8)', $tmpl)
        {
            while (<$th>)
            {
                next unless /^#+\s+(.*)$/;

                my $t = $1;

                $anchor{$1} = 1 while $t =~ s/<a id="([^"]*)"><\/a>//;
                $t =~ s/<!-- help:\w+ -->//g;
                $anchor{slug($t)} = 1;
            }
            close $th;
        }
    }

    if (!@lines || $lines[0] ne '---' || !grep { /^date: *\S/ } @lines[1 .. 4])
    {
        err($file, 1, 'the page must open with its front matter: '
                    . '--- / date: <month year> / ---');
    }

    for my $i (0 .. $#lines)
    {
        my $n = $i + 1;
        local $_ = $lines[$i];

        # the front matter is not markdown and is not checked as such
        if ($i == 0 && $_ eq '---')
        {
            $front = 1;
            next;
        }
        if ($front)
        {
            $front = 0 if $_ eq '---';
            next;
        }

        # a block is closed by a fence no shorter than the one that opened it
        if (!$fenced && /^(`{3,})/)
        {
            $fenced = length $1;
            next;
        }
        if ($fenced && /^(`+)\s*$/ && length $1 >= $fenced)
        {
            $fenced = 0;
            next;
        }
        next if $fenced;

        # headings and the node names links point at; the anchor and the
        # markers of a heading stand behind its title
        if (/^(#+)\s+(.*)$/)
        {
            my ($level, $title) = (length $1, $2);

            $anchor{$1} = 1 while $title =~ s/<a id="([^"]*)"><\/a>//;
            $title =~ s/<!-- help:\w+ -->//g;
            $title =~ s/\s+$//;
            err($file, $n, "heading level $level: go-md2man prints level 3 "
                         . 'and deeper the same way') if $level > 3;
            err($file, $n, 'empty heading') if $title !~ /\S/;
            $anchor{slug($title)} = 1;
        }
        # markers of the help file
        if (/<!--\s*help:(\w+)/)
        {
            err($file, $n, "unknown marker help:$1") unless $marker{$1};
        }

        # html that is neither a comment nor a node anchor
        my $rest = $_;
        $rest =~ s/<!--.*?-->//g;
        $rest =~ s/<a id="[^"]*"><\/a>//g;
        err($file, $n, "raw html <$1> is dropped by go-md2man")
            if $rest =~ /(?<!\\)<(\/?[a-zA-Z][\w-]*)(?:\s[^>]*)?>/;

        # go-md2man reads a one line paragraph before a quote as a heading
        if (/^>/ && $i > 1 && $lines[$i - 1] eq '' && $lines[$i - 2] =~ /\S/
            && $lines[$i - 2] !~ /^[>#`:]/ && ($i < 3 || $lines[$i - 3] !~ /\S/))
        {
            err($file, $n, 'quote under a one line paragraph: go-md2man turns '
                         . 'that paragraph into a section heading');
        }

        # constructs the pipeline has no roff for
        err($file, $n, 'table: go-md2man emits tbl markup that the pages '
                     . 'are not preprocessed for')
            if /^\s*\|/ || /^\s*[:-]+\s*\|\s*[:-]+/;
        err($file, $n, 'image') if /!\[[^\]]*\]\(/;
        err($file, $n, 'footnote') if /\[\^[^\]]+\]/;
        err($file, $n, 'strikethrough') if /(?<!\\)~~/;
        err($file, $n, 'task list') if /^\s*[-*+]\s+\[[ xX]\]/;
        err($file, $n, 'setext heading: use # instead')
            if $i > 0 && /^(=+|-{2,})\s*$/ && $lines[$i - 1] =~ /\S/
               && $lines[$i - 1] !~ /^\s*[-*+#>]/;

        # an anchor is read from the end of a heading or from a line of its
        # own; anywhere else its line would be taken for an anchor and the
        # text on it lost
        if (/<a id="[^"]*"><\/a>/ && !/^#/ && !/^<a id="[^"]*"><\/a>\s*$/)
        {
            err($file, $n, 'an anchor stands at the end of a heading or alone '
                         . 'on its line');
        }

        # go-md2man reads a block opening with a per cent sign as a title
        err($file, $n, 'a block may not open with a per cent sign: go-md2man '
                     . 'reads it as a title block')
            if /^%/ && $lines[$i - 1] !~ /\S/;

        # roff reads a dot at the start of a line as a control character;
        # an apostrophe there is settled by maint/update-man-in.sh
        err($file, $n, 'a line opening with a dot must be escaped as \\.')
            if /^\.(?!$)/;

        # an escaped character is no markup: hide the escapes, keeping a
        # mark of their own for a literal backslash
        my $m = $_;
        $m =~ s/\\\\/\x01/g;
        $m =~ s/\\./\x02/g;

        # blackfriday gives up on a backslash inside a label, and on one
        # right before the mark that closes emphasis
        err($file, $n, 'backslash inside a link label: the label is cut '
                     . 'short there') if $m =~ /\[[^\]]*\x01[^\]]*\]\(/;
        err($file, $n, 'emphasis ending in a backslash: write it as a code '
                     . 'span') if $m =~ /(?<!\*)(\*{1,2})[^*\n]*\x01\1/;

        # *a*b*c*: the runs are read as one and print their own asterisks
        err($file, $n, 'emphasis runs that touch: markdown reads them as one')
            if $m =~ /(?<![*\w])\*[^*\s][^*]*\*\w[^*]*\*[^*\s][^*]*\*(?![*\w])/;

        # a code span must be fenced wider than what it holds
        while (/(?<!`)(`+)([^`].*?)\1(?!`)/g)
        {
            my ($fence, $body) = ($1, $2);
            err($file, $n, 'code span holds a backtick run as long as its '
                         . 'fence') if $body =~ /`{@{[length $fence]},}/;
        }

        # only the placeholders the build knows
        while (/\{\{([A-Za-z_]+)(?::\d+)?\}\}/g)
        {
            err($file, $n, "unknown placeholder {{$1}}")
                unless $placeholder{$1} || $1 eq 'MC_VERSION';
        }

        push @links, [$n, $1] while /\]\(#([^)]*)\)/g;
    }

    err($file, scalar @lines, 'unterminated code fence') if $fenced;

    for my $link (@links)
    {
        my ($n, $target) = @$link;
        warn_($file, $n, "link to \"$target\", which is no node of this page")
            unless $anchor{$target};
    }
}

printf "%d file%s checked, %d error%s, %d warning%s\n",
    scalar @files, @files == 1 ? '' : 's',
    $errors, $errors == 1 ? '' : 's',
    $warnings, $warnings == 1 ? '' : 's';

exit($errors ? 1 : 0);
