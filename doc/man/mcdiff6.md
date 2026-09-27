---
date: September 2026
---

<!-- help:topics "Topics:" -->
# NAME <!-- help:skip -->

mcdiff6 - Internal diff viewer of M-Commander.

# USAGE <!-- help:skip -->

**mcdiff6**
[-bcdfhstVx?] file1 file2

# DESCRIPTION

mcdiff6 is a link to
**mcommander**,
the main M-Commander executable.  Executing
M-Commander under this name requests starting the internal diff viewer
which compares
*file1*
and
*file2*
specified on the command line.

# OPTIONS

*-b*
: Force black and white display.

*-c*
: Force color mode on terminals where
**mcdiff6**
defaults to black and white.

*-d*
: Disable mouse support.

*-h, -?, --help*
: Show the options and what they do.

*-f*
: Display the compiled-in search paths for M-Commander files.

*-S arg, --skin=arg*
: Specify a name of skin in the command line.  See the
**Skins**
section in mcommander(1) for more information.

*-s*
: Run on a slow terminal: the screen is drawn with fewer updates.

*-t*
: Used only if the code was compiled with S-Lang and terminfo: it makes
the M-Commander use the value of the
**TERMCAP**
variable for the terminal information instead of the information on
the system wide terminal database

*-V*
: Displays the version of the program.

*-x*
: Forces xterm mode.  Used when running on xterm-capable terminals (two
screen modes, and able to send mouse escape sequences).

# Internal Diff Viewer <a id="diff-viewer"></a>

The mcdiff6 is a visual diff tool. You can compare two files and edit them
in place; the difference is computed anew after every change. The git panel
plugin opens it as well, with the file as HEAD has it on one side and the
file of the working tree on the other.

Following shortcuts are available in internal diff viewer of
M-Commander.

**F1**
: Invoke the built-in hypertext help viewer.

**F2**
: Save modified files.

**F4**
: Edit file of the left panel in the internal editor.

**F14**
: Edit file of the right panel in the internal editor.

**F5**
: Merge the current hunk into the file on the right. Only the current hunk is
merged, and the difference is computed again.

**F15**
: Merge the current hunk the other way, into the file on the left.

**F7**
: Start search.

**F17**
: Continue search.

**F9**
: Open the
[options of the compare view](#diff-options).

**Alt-e**
: Choose the charset the two files are read in.

**F10, Esc, q, Q**
: Exit from diff viewer.

**Alt-s, s**
: Toggle show of hunk status.

**Alt-n, l**
: Toggle show of line numbers.

**Ctrl-s**
: Toggle syntax highlighting. The text of each line is then colored by the
syntax rules, the way the internal editor colors it, and the state of the line
is left to the background and to the marker column. Where a skin tells a
changed word from the rest of its line by the color of the text alone, the word
is underlined instead. The setting is remembered separately from the editor's
own.

**f**
: Maximize left panel.

**=**
: Make panels equal in width.

**>**
: Reduce the size of the right panel.

**<**
: Reduce the size of the left panel.

**2, 3, 4, 8**
: Set tabulation size

**C-u**
: Swap contents of diff panels.

**C-r**
: Read both files again and compute the difference anew.

**C-o**
: Toggle the terminal and show the command screen.

**Enter, Space, n**
: Find next diff hunk.

**Backspace, p**
: Find previous diff hunk.

**g, G**
: Go to line.

**Down**
: Scroll one line forward.

**Up**
: Scroll one line backward.

**PageUp**
: Move one page up.

**PageDown**
: Moves one page down.

**Left, Right**
: Scroll the text one column sideways.

**C-Left, C-Right**
: Scroll the text eight columns sideways.

**Home**
: Scroll back to the first column.

**C-Home**
: Move to the file beginning.

**C-End**
: Move to the file end.

# Diff options <a id="diff-options"></a>

The settings of the
[compare view](#diff-viewer),
as
**F9**
there and the
**Diff viewer options**
entry of the Options menu of the file manager open them. The compare view
takes them when it starts, so a view already on the screen keeps the ones it
was opened with.

*Diff algorithm.*
Normal compares the files as they are. Fastest assumes large files and
settles for a coarser result. Minimal spends more time to find a smaller set
of changes.

*Ignore case.*
Upper and lower case letters count as the same character.

*Ignore tab expansion.*
Lines that differ only in how the same indentation is written, with tabs or
with spaces, count as equal.

*Ignore space change.*
A run of whitespace counts as equal to any other run of whitespace.

*Ignore all whitespace.*
Whitespace is left out of the comparison.

*Strip trailing carriage return.*
Drop the carriage return at the end of a line, so a file with DOS line ends
compares to one with Unix line ends.

# FILES

*{{pkgdatadir}}/help/mcommander.md*
: The help file for the program.

*{{pkgdatadir}}/mc.ini*
: The default system-wide setup for M-Commander, used only if
the user's own ~/.config/mc6/ini file is missing.

*{{pkgdatadir}}/defaults.ini*
: Global settings for the M-Commander.  Settings in this file
affect all users, whether they have ~/.config/mc6/ini or not.

*~/.config/mc6/ini*
: User's own setup.  If this file is present, the setup is loaded from
here instead of the system-wide startup file.

# LICENSE <!-- help:skip -->

This program is distributed under the terms of the GNU General Public
License as published by the Free Software Foundation.  See the built-in
help of the M-Commander for details on the License and the lack
of warranty.

# AVAILABILITY

The latest version of this program can be found at
<https://github.com/blue-panels/mcommander/releases> .

# SEE ALSO

mcommander(1), mcedit6(1), mview(1)

# BUGS

Bugs should be reported to
<https://github.com/blue-panels/mcommander/issues> .
