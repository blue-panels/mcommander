# Find duplicates

The Command menu entry **Find duplicates...** looks for files that are copies
of each other and shows them in a panel, in groups. A group is two or more
files that match by everything you ask for: the same content, the same size or
the same name. With *Similar content* a group can also hold pictures that look
the same and texts that differ only in line endings and spaces.

**What is searched**

*Start at*
: The directory to search. An empty field is the directory of the current
panel.

*Also in the other panel*
: Search the directory of the other panel too, when it shows a local
directory.

*The other panel is a reference: never move or delete there*
: The files of the other panel are kept: the result shows them with *ref*,
**F8** does not delete them and **F6** does not move them. A symbolic link from
the start directory into the reference one does not lift this. The start
directory cannot be the reference directory itself.

*Enable ignore directories*
: The directories not to look into, separated by `:`. A name such as `.git`
skips every directory of that name; an absolute path skips that directory
only. A `/` at the end does not matter.

*File mask*
: The names of the files to compare. Several masks are
separated by `;` or `,`: `*.jpg;*.png` or `*.jpg, *.png`. A comma inside
braces or brackets is a part of the mask: `*.{jpg,png}`, `[a,b].txt`. With
*Using shell patterns* off, the field is one regular expression. An empty
field takes every file.

*Size from (e.g. 10M)*, *to*
: The limits of the size. `K`, `M`, `G` and `T` are powers of 1024, as are
`KiB`, `MiB` and the like; `KB`, `MB`, `GB` are powers of 1000. An empty field
is no limit. Empty files are never compared.

*Modified from (e.g. 2024-01-01 or 30d)*, *to*
: The limits of the modification time: a date, or a time back from now in
days, weeks, months or years: `30d`, `2w`, `6m`, `1y`. A date in *to* takes
the whole of that day.

*Skip hidden*, *Find recursively*, *Follow symlinks*
: As in Find File. A file reached through a link is shown under the path of
the link.

**What the files have in common**

*Same size*, *Same name*, *Name case sensitive*
: Files of one size, or of one name. Without *Same content* nothing is read,
so such files may differ.

*Same content*
: Files with the same bytes. They are compared in three steps: by size, then
by a hash of the first and the last 4 KiB, then by a hash of the whole file.
A file that no other one matches at a step is not read further.

*Similar content, not only the same bytes*
: With *Same content*: files that are alike, not only equal. The size and the
name do not count then.

*By file type*
: Pictures by what they show, texts without their line endings and spaces at
the end of the lines, all other files byte by byte. A file is a text when its
first 8000 bytes hold no zero byte.

*Images: perceptual hash*
: Pictures only. JPEG and PNG are read when the program is built with libjpeg
and libpng. A smaller copy, a copy saved with another quality or in the other
format, a lighter or a blurred one is found. The picture with the most pixels
is the one the others of its group are compared with. A grey copy of a color
picture is also found, as the hash looks at the brightness only.

*Text: ignore line endings and spaces*
: Texts only: CR LF, CR and LF are one, spaces and tabs at the end of a line
and empty lines at the end of the file do not count.

*Similarity (e.g. 90%)*
: How alike two pictures must be. 100% takes only pictures with the same hash;
90% also takes a copy saved at a low quality.

**The hashes are kept**

The hashes of the files read are kept in *~/.cache/mc6/duplicates-v1.cache*,
so a second search reads only the files that changed since. A file is known by
its device, inode, size and times; an entry not used for 90 days goes.

**While it searches**

The window shows the directory being read, then the files being hashed, and
the groups found so far.

*Show found*
: Opens the panel with the groups found so far. The search goes on, and the
panel takes the new groups as they come.

*Cancel*
: Stops the search and shows nothing.

When the search is over with no group, the window says how many files it took.
It also says how many files and directories could not be read: they may hold
duplicates too.

# Duplicates panel

The panel lists the groups, first those whose copies take the most space. The
names are below the directory in the title of the panel.

*Grp*
: The number of the group.

*Ref*
: *ref* for a file of the reference directory.

*Sim*
: How alike the file is to the first one of its group, for similar content.

The line at the bottom says how many groups there are and how much space
deleting the extra copies frees: all the files of a group but one, or all but
those of the reference directory. While the search goes on, it shows how far
it is; *unread* counts the files and the directories that could not be read.

**Keys**

**F8**
: Delete the marked files, or the file under the cursor, from the disk. A file
of the reference directory is kept, and so is a file that changed since the
search: it may be no duplicate now. When a group would lose its last copy, a
question comes first; its default answer keeps the files.

**F6**, **Shift-F6**
: Move or rename a file. A file of the reference directory is not moved.

**F3**, **F4**, **F5**, **Ctrl-x q**
: View, edit, copy and the quick view, as on any panel.

**Ctrl-R**
: Drop from the list the files that are no longer there. A file moved or
deleted in any other way is dropped too, with its group when one file is left.

**Enter** on *..*
: Close the panel. The search goes on; **Alt-F1** or **Alt-F2**, *Duplicates*,
opens it again.

*Command* menu, *Stop the search for duplicates*
: Stop a search that goes on. The groups found so far stay.

A new **Find duplicates...** while a search goes on asks to stop it first.
