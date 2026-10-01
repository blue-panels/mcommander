---
date: September 2026
---

<!-- help:topics "Topics:" -->
# NAME <!-- help:skip -->

mcommander - twin-panel text-mode file manager

# SYNOPSIS <!-- help:skip -->

**mcommander**
[-abcCdfhPstuUVx] [-l log] [dir1 [dir2]] [-e [file] ...] [-v file]

# DESCRIPTION

M-Commander is a twin-panel text-mode file manager based on GNU Midnight
Commander. Its architecture is built around a compact core and dynamically
loaded panel plugins. The plugins provide a uniform panel interface for
archives, remote file systems, repositories, and other data sources. Commands
are run in a built-in terminal. M-Commander also includes a text editor with
syntax highlighting and a viewer that supports both text and binary formats.


# OPTIONS

*-a, --stickchars*
: Disable usage of graphic characters for line drawing.

*-b, --nocolor*
: Force black and white display.

*-c, --color*
: Force color mode, please check the section
[Colors](#colors)
for more information.

*--configure-options*
: Display configure options.

*-d, --nomouse*
: Disable mouse support.

*-e [file], --edit[=file]*
: Start the internal editor.  If the file is specified, open it on
startup.  See also
**mcedit6(1)**.

*-f, --datadir*
: Display the compiled-in search paths for M-Commander files.

*-F, --datadir-info*
: Display extended info about compiled-in paths for
M-Commander.

*-g, --oldmouse*
: Force a "normal tracking" mouse mode. Used when running on
xterm-capable terminals (tmux/screen).

*-K file, --keymap=file*
: Specify a name of keymap file in the command line.

*--nokeymap*
: Don't load key bindings from any file, use default hardcoded keys.

*--mortal*
: Exit when the terminal is lost, instead of waiting for a new one. See
[Lost terminal](#lost-terminal).

*-P file, --printwd=file*
: Print the last working directory to the specified file.  This option is
not meant to be used directly.  Instead, it's used from a special shell
script that automatically changes the current directory of the shell to
the last directory M-Commander was in. Source the file
**{{pkglibexecdir}}/mc6.sh**
(bash and zsh users),
**{{pkglibexecdir}}/mc6.csh**
(tcsh users), or
**{{pkglibexecdir}}/mc6.fish**
(fish users) respectively to define
**mcommander**
as an alias to the appropriate shell script.

*-s, --slow*
: Turn on the slow terminal mode, in this mode the program will not draw
expensive line drawing characters and will toggle verbose mode off.

*-S arg, --skin=arg*
: Specify a name of skin in the command line. Technology of skins is
documented in the
[Skins](#skins)
section.

*-t, --termcap*
: Used only if the code was compiled with S-Lang and terminfo: it makes
M-Commander use the value of the
**TERMCAP**
variable for the terminal information instead of the information on
the system wide terminal database

*-v file, --view=file*
: Start the internal viewer to view the specified file.  See also
**mview(1)**.

*-V, --version*
: Display the version of the program.

*-x, --xterm*
: Force xterm mode.  Used when running on xterm-capable terminals (two
screen modes, and able to send mouse escape sequences).

*-X, --no-x11*
: Do not use X11 to get the state of modifiers Alt, Ctrl, Shift

If both paths are specified, the first path name is the directory to show
in the active panel; the second path name is the directory to be shown in
the other panel.

If one path is specified, the path name is the directory to show
in the active panel; value of "other_dir" from panels.ini is the directory
to be shown in the passive panel.

If no paths are specified, current directory is shown in the active panel;
value of "other_dir" from panels.ini is the directory to be shown in
the passive panel.

# Overview

The screen of M-Commander is divided into four parts.
Almost all of the screen space is taken up by two directory panels.
By default, the second line from the bottom of the screen is the
shell command line, and the bottom line shows the function key labels.
The topmost line is the
[menu bar line](#menu-bar).
The menu bar line may not be visible, but appears if you click the
topmost line with the mouse or press the F9 key.

M-Commander provides a view of two directories at the same
time. One of the panels is the current panel (a selection bar is in
the current panel). Almost all operations take place on the current
panel. Some file operations like Rename and Copy by default use the
directory of the unselected panel as a destination (don't worry, they
always ask you for confirmation first). For more information, see the
sections on the
[Directory Panels](#directory-panels),
the
[Left and Right Menus](#left-and-right-menus)
and the
[File Menu](#file-menu).

You can execute system commands from M-Commander by simply
typing them. Everything you type will appear on the shell command line,
and when you press Enter, M-Commander will execute the
command line you typed; read the
[Shell Command Line](#shell-command-line)
and
[Input Line Keys](#input-line-keys)
sections to learn more about the command line.

# Mouse Support

M-Commander comes with mouse support. It is activated
whenever you are running on an
**xterm(1)**
terminal (it even works if you take a telnet, ssh or rlogin connection to
another machine from the xterm) or if you are running on a Linux
console and have the
**gpm**
mouse server running.

When you left click on a file in the directory panels, that file is
selected; if you click with the right button, the file is marked (or
unmarked, depending on the previous state).

Double-clicking on a file will try to execute the command if it is
an executable program; and if the
[extension file](#edit-extension-file)
has a program specified for the file's extension, the specified
program is executed.

Also, it is possible to execute the commands assigned to the function
key labels by clicking on them.

The default auto repeat rate for the mouse buttons is 400
milliseconds. This may be changed to other values by editing the
[~/.config/mc6/ini](#save-setup)
file and changing the
*mouse_repeat_rate*
parameter.

If you are running M-Commander with the mouse support, you
can get the default mouse behavior (cutting and pasting text) by holding
down the Shift key.

<!-- help:break -->

# Lost terminal <a id="lost-terminal"></a>

When the terminal M-Commander runs in is lost (the window is closed, the
connection over SSH drops), M-Commander does not exit. It waits, with
everything it was doing, for a new terminal.

Start
**mcommander**
in another terminal with the same
**TERM**
and it lists the M-Commanders that wait, each with the directory it was in.
The number of one of them brings it to the new terminal, where it goes on from
the screen it had; Enter starts a new M-Commander as usual.
The command that took the old one over stays until it finishes, passes the
size changes of the window and the interrupt key to it, and exits with its
exit code. The terminal is asked what it can do again, as at the start.

A waiting M-Commander is an ordinary process; stop it with
**kill**
if it is not needed. Its files are in
*$XDG_RUNTIME_DIR/mc-resurrect*, or, with no such variable, in
*mc-resurrect-UID*
of the temporary directory, and are only open to the user.

The option
**--mortal**,
or
*immortal=false*
in the
*Midnight-Commander*
section of the ini file, turns this off: M-Commander exits with its terminal.
It is also off on the Linux console, in the terminal of another M-Commander,
and in the editor and the viewer started alone.

<!-- help:break -->

# Keys

Some commands in M-Commander involve the use of the
*Control*
(sometimes labeled CTRL or CTL) and the
*Meta*
(sometimes labeled ALT or even Compose) keys. In this manual we will
use the following abbreviations:

**C-\<chr>**
: means hold the Control key while typing the character \<chr>.
Thus C-f would be: hold the Control key and type f.

**Alt-\<chr>**
: means hold the Meta or Alt key down while typing \<chr>.
If there is no Meta or Alt key, type
*Esc,*
release it, then type the character \<chr>.

**S-\<chr>**
: means hold the Shift key down while typing \<chr>.

All input lines in M-Commander use an approximation to
the GNU Emacs editor's key bindings (default).

You may redefine key bindings. See
[redefine hotkey bindings](#keys_redefine)

for more info. All other key bindings (described in this manual) are relative
to default behavior.

There are many sections which tell about the keys. The following are
the most important.

The
[File Menu](#file-menu)
section documents the keyboard shortcuts for the commands appearing in
the File menu. This section includes the function keys. Most of these
commands perform some action, usually on the selected file or the
tagged files.

The
[Directory Panels](#directory-panels)
section documents the keys which select a file or tag files as a
target for a later action (the action is usually one from the file
menu).

The
[Shell Command Line](#shell-command-line)
section list the keys which are used for entering and editing command
lines. Most of these copy file names and such from the directory
panels to the command line (to avoid excessive typing) or access the
command line history.

[Input Line Keys](#input-line-keys)
are used for editing input lines. This means both the command line and
the input lines in the query dialogs.

## Redefine hotkey bindings <a id="keys_redefine"></a>

The same can be done in the program itself, from the
**Options**
menu. The
[Key bindings](#key-bindings)
dialog lists every action with the keys it answers to, changes them and writes
the result to
**~/.config/mc6/keymap.ini**,
so the file the option looks for is the file that dialog keeps. The
[Learn keys](#learn-keys)
dialog is about the other end of the problem: it teaches the program the
sequences a terminal sends for keys it gets wrong. The
[Key sniffer](#key-sniffer)
shows what arrives for a key that is pressed, with the action it is bound to
in the current keymap, which is what to look at when a binding seems to do
nothing.

Hotkey bindings may be read from external file (keymap-file).
Initially, M-Commander creates key bindings using keymap defined
in the source code. Then, two files
**{{pkgdatadir}}/keymap.ini**
and
**{{sysconfdir}}/mcommander/keymap.ini**
are loaded always, sequentially reassigned key bindings defined earlier.
The package puts its own keymaps in
**{{sysconfdir}}/mcommander**:
**keymap.default.ini**,
**keymap.emacs.ini**
and
**keymap.vim.ini**,
with
**keymap.ini**
a link to the default one.
The option
**--nokeymap**
leaves every file out and keeps the bindings of the source code.

User-defined keymap-file is searched on the following algorithm
(to the first one found):

```
1) command line option -K <keymap>, --keymap=<keymap>
2) environment variable MC_KEYMAP
3) parameter keymap of the [Midnight-Commander] section
4) file ~/.config/mc6/keymap.ini
```

The first three take a name or an absolute path. A name that does not end in
**.keymap**
gets that extension added, and is looked for in (to the first one found):

```
1) ~/.config/mc6/
2) {{pkgdatadir}}/
```

Because of that extension the keymaps of the package, whose names end in
**.ini**,
cannot be chosen this way. To take one of them, copy or link it to
**~/.config/mc6/keymap.ini**,
which is read last and needs no option:

```
ln -s {{sysconfdir}}/mcommander/keymap.vim.ini ~/.config/mc6/keymap.ini
```

## Far mode <a id="far-mode"></a>

M-Commander can take the panel keys of Far Manager over its own. The mode is
for people who come from Far and want their fingers to find the same keys; it
is off by default, and nothing changes for anybody who does not turn it on.

Turn it on with the
*Far Manager keys*
check box of the
[Configuration](#configuration)
dialog in the
**Options**
menu, or set
**far_mode=true**
in the
**[Midnight-Commander]**
section of
**~/.config/mc6/ini**.
The keys change at once, and the setting is kept with the rest of the setup.
The mode is a layer over the keymap: it goes over the built-in keys and over
the
**keymap.ini**
files of the system, and under the
**keymap.ini**
of the user, so the user can still change any key by
[hand](#keys_redefine).

The mode covers the directory panels, the editor, the viewer, the command
line, the edit lines of the dialogs and a few keys of the dialogs
themselves. These are the keys that the mode gives to the panels, with what
each of them replaces:

**Alt-F7**
: find file. Alt-? keeps working.

**Alt-F8**
: the history of the command line. Alt-h keeps working.

**Alt-F11**
: the history of the viewed and edited files. Alt-Shift-e keeps working.

**Alt-F12**
: the history of the directories. Alt-Shift-h keeps working.

**Ctrl-F3, Ctrl-F4, Ctrl-F5, Ctrl-F6**
: sort the panel by name, by extension, by modification time and by size.
Far reverses the order when the same key is pressed twice; so does
M-Commander.

**Ctrl-F7, Ctrl-F8, Ctrl-F9, Ctrl-F11**
: keep the files unsorted, sort them by the time of change (Far has the
creation time there, which a Unix file system does not keep), by the time of
access and by the owner. Ctrl-F10, the sort by descriptions, is not bound:
M-Commander has no descriptions.

**Ctrl-F12**
: the sort order menu.

**Ctrl-L, Ctrl-Q, Ctrl-T**
: the information panel, the quick view panel and the tree panel. In the
panels Ctrl-L stops redrawing the screen (it still does in the dialogs), and
Ctrl-T stops tagging a file; Insert tags it, as it does in Far.

**Ctrl-1 to Ctrl-9, Ctrl-0**
: the listing modes of Far: brief (three columns of names), medium (two
columns), full (name, size and time), wide (name and size), detailed (with the
times of access and change and the permissions), names alone (Ctrl-6) and
names with the modification time (Ctrl-7), which stand for the descriptions
that M-Commander does not have, owners, links, and the long listing of
M-Commander for Ctrl-0. The modes are fixed and do not touch the list of
[panel modes](#panel-modes) of the user.

**Ctrl-A**
: the attributes of the file, which is the permission dialog (chmod). In the
command line Ctrl-A no longer moves to the beginning of the line; Home does.

**Ctrl-F**
: puts the full name of the file under the cursor into the command line
(Ctrl-Shift-Enter does it too). Ctrl-Enter puts the name alone, as it does in
the mode off.

**Ctrl-X**
: is the next command of the history, see below. The prefix of the extended
commands, which Ctrl-X was, moves to Alt-X.

**Ctrl-\\ (control-backslash)**
: goes to the root directory of the panel: of the file system, of the archive
or of the remote host that the panel shows (a panel of a plugin stays where it
is). The directory hotlist, which was Ctrl-\\, moves to Alt-\\.

**Shift-gray plus, Shift-gray minus**
: select all the files and unselect all of them (the gray keys alone ask for a
pattern, as they do without the mode). A terminal sends them only when it
reports modified keys, see below.

**Ctrl-Shift-1 to Ctrl-Shift-0, Alt-1 to Alt-0**
: remember the directory of the panel in one of ten folder shortcuts, and go to
it. Far goes to a shortcut with the right Ctrl and the digit, which a terminal
cannot tell from the left Ctrl (Ctrl and a digit is the listing mode), so the
go keys are Alt and the digit; they can be changed in keymap.ini (actions
SetFolderShortcut0 to 9 and GoFolderShortcut0 to 9). The shortcuts are in the
FarFolderShortcuts group of the setup file and are saved with it. Ctrl-Shift
with a digit is sent by a terminal of the kitty keyboard protocol only.

**Ctrl-[, Ctrl-]**
: put the path of the left and of the right panel into the command line. Ctrl-]
works everywhere; a terminal sends Ctrl-[ as Esc, so it is told apart only in a
terminal of the kitty keyboard protocol.

**Alt with a character, Alt-Shift with a character**
: the fast find of Far: the panel goes to the file whose name begins with the
characters typed while Alt is held down, and Ctrl-Enter and Ctrl-Shift-Enter go
on to the next and to the previous file that matches. The search ends with Esc
or with a key that is not a character, as the quick search of M-Commander does.
Every Alt key that has an action in M-Commander keeps it (Alt-h, Alt-a and so
on), and so does every Alt key of the command line editing while there is text
in the command line, but once the search has begun all Alt keys go to it; the
Alt-Shift keys of the same letters are free for it when the Alt keys are
taken. The quick search of M-Commander (Ctrl-s and Alt-s) works as before.

**Ctrl-G**
: applies a command to the files: it asks for a command and runs it for every
tagged file, or for the file under the cursor if none is tagged. In the
command, %f (or %p) is the name of the file, %n the name without the extension
and %x the extension, all quoted for the shell; the other macros of the user
menu work as they do there.

**Alt-F6, Alt-F10, Shift-F9, Shift-F10**
: create a hard link, show the tree of directories, save the setup and choose
the menu item chosen last (Shift-F7, which was it, still does).

**In the command line and in the edit lines of the dialogs** (they all share
the keys):
Ctrl-E is the previous command of the history and Ctrl-X the next one, as in
Far, and Ctrl-Y deletes the whole line. Ctrl-Backspace and Ctrl-Delete delete a
word to the left and to the right. The keys these took are given away: Ctrl-E
no longer moves to the end of the line (End does), and Yank, which was Ctrl-Y,
is Alt-y.

**In the editor** (as the far2l help lists them):
Ctrl-F7 replaces, and F4, which replaced text, quits the editor together with
F10 and Esc; Alt-F8 goes to a line (Alt-l keeps working); Ctrl-F3 shows the
line numbers; Alt-F11 shows the history of the edited files; Ctrl-Z undoes,
Ctrl-U deselects the block (it undid before) and Ctrl-A selects all. Ctrl-F7
no longer continues the search, Shift-F7 does, and Ctrl-Z no longer moves a
word left, Ctrl-Left does.

**In the viewer:**
Alt-F8 goes to a position (F5 keeps working), Alt-F7 continues the search in the
opposite direction and Alt-F11 shows the history of the viewed files.

**In the dialogs:**
Ctrl-Enter does what Enter does when no button has the focus, which is the
default action of the dialog, and the numeric plus and minus (and the plus and
the minus of the main keyboard) switch the check box that has the focus on and
off, where Space switches it over. PgDn gives the focus to the default button
(a list that has the focus keeps PgDn for itself). Shift-Enter in the edit line of a
dialog inserts the name of the file under the cursor of the active panel,
Ctrl-Shift-Enter the one of the passive panel (not in the command line, where
Ctrl-Shift-Enter keeps its meaning; a terminal sends them only when it reports
modified keys, see below). Esc, F10, Tab, the arrow keys and the
hotkeys of the buttons are the same in Far and in M-Commander. Ctrl-Enter is
sent by a terminal only when it reports modified keys (see below).

The keys that M-Commander shares with Far are the same with or without the
mode: F1 to F10, Shift-F4, Shift-F5, Shift-F6 and Shift-F8, Tab, Ctrl-U,
Ctrl-R, Ctrl-O, Ctrl-F1 and Ctrl-F2, Alt-F1 and Alt-F2, Ctrl-PgUp and Ctrl-PgDn,
Insert and the numeric plus, minus and asterisk.

What differs from Far, and stays so for now: F3 in the editor still marks a
block instead of wrapping lines, the clipboard keys Ctrl-C, Ctrl-V and Ctrl-X
of the editor are not bound (Ctrl-Insert, Shift-Insert and Shift-Delete work),
and the viewer keeps its own F8, F9, Space and plus and minus keys; the panels
keep the M-Commander colors and have no descriptions of the files (Ctrl-6 and
Ctrl-7 show other columns, see above), Ctrl-Z and Ctrl-M
are not bound, nor is Ctrl-H for the hidden files (a terminal sends Backspace
as Ctrl-H, and Enter as Ctrl-M, and M-Commander cannot tell the two apart;
Alt-. shows and hides the hidden files, as it does without the mode), and Ctrl-, does
not switch the panel layout (a terminal sends it as Ctrl-L, which shows the
information panel in this mode); Alt-, does. Alt with a character is the fast
find only where no action of M-Commander has the key, see above, and the fast
find takes no characters that are not Latin-1. The dialogs keep their own keys,
apart from the edit lines, Ctrl-Enter and the check boxes: M-Commander has no
Far key for moving a dialog, and the mouse is not part of the mode.

Some keys of the mode (Ctrl with a digit, with a comma, with Enter or with a function key) are sent by a
terminal only when it reports modified keys, as xterm and the terminals that speak the kitty keyboard protocol do.
In a terminal that does not, the key arrives as another one or does not arrive
at all, and nothing is lost: every action is also in the menus, and the key it
had before the mode is still bound where the list above says so.

## Miscellaneous Keys

Here are some keys which don't fall into any of the other categories:

**Enter**
: if there is some text in the command line (the one at the bottom of
the panels), then that command is executed. If there is no text in the
command line then if the selection bar is over a directory the
M-Commander does a
**chdir(2)**
to the selected directory and reloads the information on the panel;
if the selection is an executable file then it is executed. Finally,
if the extension of the selected file name matches one of the
extensions in the
[extensions file](#edit-extension-file)
then the corresponding command is executed.

**C-l**
: repaint all the information in M-Commander.

**C-x c**
: run the
[Chmod](#chmod)
command on a file or on the tagged files.

**C-x o**
: run the
[Chown](#chown)
command on the current file or on the tagged files.

**C-x l**
: run the hard link command.

**C-x s**
: run the absolute symbolic link command.

**C-x v**
: run the relative symbolic link command. See the
[File Menu](#file-menu)
section for more information about symbolic links.

**C-x i**
: set the other panel display mode to information.

**C-x q**
: set the other panel display mode to quick view.

**C-x !**
: execute the
[External panelize](#external-panelize)
command.

**C-x h**
: run the
[add directory to hotlist](#hotlist)
command.

**Alt-!**
: executes the Filtered view command, described in the
[view command](mview.md#internal-file-viewer).

**Alt-?**
: executes the
[Find file](#find-file)
command.

**Alt-c**
: pops up the
[quick cd](#quick-cd)
dialog.

**C-o**
: when the program is being run in the Linux or FreeBSD console or under
an xterm, it will show you the output of the previous command.  When ran
on the Linux console, M-Commander uses an external program
(cons.saver) to handle saving and restoring of information on the
screen.

You can type C-o at any time and you will be taken back to
M-Commander's main screen, to return to your application just type C-o.
If you have an application suspended by using this trick, you won't be
able to execute other programs from M-Commander until you
terminate the suspended application.

## Directory Panels

This section lists the keys which operate on the directory panels. If
you want to know how to change the appearance of the panels take a
look at the section on
[Left and Right Menus](#left-and-right-menus).

**Tab, C-i, left-key, right-key**
: change the current panel. The old other panel becomes the new current
panel and the old current panel becomes the new other panel. The
selection bar moves from the old current panel to the new current
panel.

**Insert, C-t**
: to tag files you may use the Insert key (the kich1 terminfo sequence).
To untag files, just retag a tagged file.

**Alt-e**
: to change charset of panel you may use Alt-e (M-e).
Recoding is made from selected codepage into system codepage. To
cancel the recoding, select "No translation" in the dialog of encodings.

**Alt-g, Alt-r, Alt-j**
: used to select the top file in a panel, the middle file and the bottom one,
respectively.

**Alt-t**
: toggle the current display listing to show the next display listing
format.
With this it is possible to quickly switch to brief listing, long
listing, user defined listing format, and back to the default.

**C-\\ (control-backslash)**
: show the
[directory hotlist](#hotlist)
and change to the selected directory.

**+  (plus)**
: this is used to select (tag) a group of files. M-Commander
will prompt for a selection options. When
*Files only*
checkbox is on, only files will be selected.  If
*Files only*
is off, as files as directories will be selected.
When
*Shell Patterns*
checkbox is on, the regular expression is much like the filename globbing
in the shell (\* standing for zero or more characters and ? standing
for one character). If
*Shell Patterns*
is off, then the tagging of files is done with normal regular
expressions (see ed (1)). When
*Case sensitive*
checkbox is on, the selection will be case sensitive characters.
If
*Case sensitive*
is off, the case will be ignored.

**\\ (backslash)**
: use the "\\" key to unselect a group of files. This is the opposite of
the Plus key.

**up-key, C-p**
: move the selection bar to the previous entry in the panel.

**down-key, C-n**
: move the selection bar to the next entry in the panel.

**home, a1, Alt-<**
: move the selection bar to the first entry in the panel.

**end, c1, Alt->**
: move the selection bar to the last entry in the panel.

**next-page, C-v**
: move the selection bar one page down.

**prev-page, Alt-v**
: move the selection bar one page up.

**Alt-(, Alt-)**
: scroll long filenames to the right or left.

**Alt-o**
: If the currently selected file is a directory, load that directory on
the other panel and moves the selection to the next file. If the
currently selected file is not a directory, load the parent directory
on the other panel and moves the selection to the next file.

**Alt-i**
: make the current directory of the current panel also the current
directory of the other panel.  Put the other panel to the listing mode
if needed.  If the current panel is panelized, the other panel doesn't
become panelized.

**C-PageUp, C-PageDown**
: only when supported by the terminal: change to ".." and to the currently
selected directory respectively.

**Alt-y**
: moves to the previous directory in the history, equivalent to clicking
the
*<*
with the mouse.

**Alt-u**
: moves to the next directory in the history, equivalent to clicking the
*>*
with the mouse.

**Alt-S-h, Alt-H**
: displays the directory history, equivalent to depressing the 'v' with
the mouse.

## Quick search and quick filter <a id="quick-search"></a>

The Quick search mode allows you to perform fast file search in a file panel.
Press
**C-s**
or
**Alt-s**
to start a filename search in the directory listing. Press
**Alt-Shift-s**
to start Quick filter, which uses the same pattern but hides entries that do
not contain it. The parent directory entry is always shown.

When either mode is active, the user input will be added to the shared pattern
instead of the command line. If the
*Show mini-status*
option is enabled, the pattern is shown on the mini-status line. When typing,
the selection bar will move to the next file starting with the typed letters;
in Quick filter mode the list is also reduced to matching entries. The
**Backspace**
or
**DEL**
keys can be used to correct typing mistakes.

Pressing
**C-s**
or
**Alt-s**
while Quick filter is active switches to Quick search and shows all entries
without losing the pattern or current file. Pressing
**Alt-Shift-s**
while Quick search is active switches back to Quick filter.
Repeating the shortcut of the active mode searches for the next match.

Navigation keys such as the arrows,
**Home**,
**End**,
**PageUp**
and
**PageDown**
move within the filtered list without closing Quick filter.

Files can be marked and unmarked while Quick filter is active. Their marks are
preserved when switching modes or closing the filter.

If either mode is started by pressing its shortcut twice, the previous pattern
will be used.

Besides the filename characters, you can also use wildcard
characters '\*' and '?'.

## Shell Command Line

This section lists keys which are useful to avoid excessive typing when
entering shell commands.

**Alt-Enter**
: copy the currently selected file name to the command line.

**C-Enter**
: same a Alt-Enter.  May not work on remote systems and some terminals.

**C-S-Enter**
: copy the full path name of the currently selected file to the command
line.  May not work on remote systems and some terminals.

**Alt-Tab**
: does the filename, command, variable, username and hostname
[completion](#completion)
for you.

**C-x t, C-x C-t**
: copy the tagged files (or if there are no tagged files, the selected
file) of the current panel (C-x t) or of the other panel (C-x C-t) to
the command line.

**C-x p, C-x C-p**
: the first key sequence copies the current path name to the command
line, and the second one copies the unselected panel's path name to
the command line.

**C-q**
: the quote command can be used to insert characters that are otherwise
interpreted by M-Commander (like the '+' symbol)

**Alt-p, Alt-n**
: use these keys to browse through the command history. Alt-p takes you
to the last entry, Alt-n takes you to the next one.

**Alt-h**
: displays the history for the current input line.

## General Movement Keys

The help viewer, the file viewer and the directory tree use common
code to handle moving. Therefore they accept exactly the same
keys. Each of them also accepts some keys of its own.

Other parts of M-Commander use some of the same movement
keys, so this section may be of use for those parts too.

**Up, C-p**
: moves one line backward.

**Down, C-n**
: moves one line forward.

**Prev Page, Page Up, Alt-v**
: moves one page up.

**Next Page, Page Down, C-v**
: moves one page down.

**Home, A1**
: moves to the beginning.

**End, C1**
: move to the end.

The help viewer and the file viewer accept the following keys in
addition the to ones mentioned above:

**b, C-b, C-h, Backspace, Delete**
: moves one page up.

**Space bar**
: moves one page down.

**u, d**
: moves one half of a page up or down.

**g, G**
: moves to the beginning or to the end.

## Input Line Keys

The input lines (they are used for the
[command line](#shell-command-line)
and for the query dialogs in the program) accept these keys:

**C-a**
: puts the cursor at the beginning of line.

**C-e**
: puts the cursor at the end of the line.

**C-b, move-left**
: move the cursor one position left.

**C-f, move-right**
: move the cursor one position right.

**Alt-f**
: moves one word forward.

**Alt-b**
: moves one word backward.

**C-h, Backspace**
: delete the previous character.

**C-d, Delete**
: delete the character in the point (over the cursor).

**C-@**
: sets the mark for cutting.

**C-w**
: copies the text between the cursor and the mark to a kill buffer and
removes the text from the input line.

**Alt-w**
: copies the text between the cursor and the mark to a kill buffer.

**C-y**
: yanks back the contents of the kill buffer.

**C-k**
: kills the text from the cursor to the end of the line.

**Ctrl-Insert**
: copies the selected text to the clipfile and to the external clipboard.
With nothing selected: the marked files of the panel on screen, one per
line; else the whole line; else the file under the panel cursor.

**Shift-Delete**
: cuts the selected text to the clipfile and to the external clipboard.

**Shift-Insert**
: pastes the clipfile into the line as one line: line breaks and other
control characters become spaces. On the command line it works with the
panels shown or hidden. More than 2 KB of text is pasted only after a
confirmation.

**Alt-p, Alt-n**
: Use these keys to browse through the command history. Alt-p takes you
to the last entry, Alt-n takes you to the next one.

**Alt-C-h, Alt-Backspace**
: delete one word backward.

**Alt-Tab**
: does the filename, command, variable, username and hostname
[completion](#completion)
for you.

<!-- help:break -->

# Menu Bar

The menu bar pops up when you press F9 or click the mouse on the top
row of the screen. The menu bar has six menus: "Left", "File",
"Attributes", "Command", "Options" and "Right".

The
[Left and Right Menus](#left-and-right-menus)
allow you to modify the appearance of the left and right directory
panels.

The
[File Menu](#file-menu)
lists the actions you can perform on the currently selected file or
the tagged files.

The
[Attributes Menu](#attributes-menu)
lists the commands that change the permissions, the owner and the file system
flags of the same files.

The
[Command Menu](#command-menu)
lists the actions which are more general and bear no relation to the
currently selected file or the tagged files.

The
[Options Menu](#options-menu)
lists the actions which allow you to customize M-Commander.

## Left and Right (Above and Below) Menus <a id="left-and-right-menus"></a>

The outlook of the directory panels can be changed from the
**Left**
and
**Right**
menus (they are named
**Above**
and
**Below**
when the horizontal panel split is chosen from the
[Layout](#layout)
options dialog).

### Listing Format...

The listing mode view is used to display a listing of files, there are
four different listing formats available:
**Full**,
**Brief**,
**Long**
and
**User**.
The full directory view shows the file name, the size of the file and
the modification time.

The brief view shows only the file name and it has from 1 up to 9 columns
(therefore showing more files unlike other views). The long view
is similar to the output of
**ls -l**
command. The long view takes the whole screen width.

If you choose the "User" display format, then you have to specify
the display format.

The user display format must start with a panel size specifier.  This
may be "half" or "full", and they specify a half screen panel and a
full screen panel respectively.

After the panel size, you may specify how many listings to fit in the
panel, side-by-side (in other words: how many times to repeat the
fields horizontally). This defaults to 1. You may change this by adding a
number from 1 to 9 to the format string.

After this you add the name of the fields with an optional size
specifier.  This are the available fields you may display:

**name**
: displays the file name.

**size**
: displays the file size.

**bsize**
: is an alternative form of the
**size**
format. It displays the size of the files and for directories it just
shows SUB-DIR or UP--DIR unless the directory size is computed by the
"Show directory sizes" command.

**type**
: displays a one character wide type field.  This character is similar to
what is displayed by ls with the -F flag -
**\***
for executable files,
**/**
for directories,
**@**
for links,
**=**
for sockets,
**-**
for character devices,
**+**
for block devices,
**|**
for pipes,
**~**
for symbolic links to directories and
**!**
for stale symlinks (links that point nowhere).

**mark**
: an asterisk if the file is tagged, a space if it's not.

**mtime**
: file's last modification time.

**atime**
: file's last access time.

**ctime**
: file's status change time.

**perm**
: a string representing the current permission bits of the file.

**mode**
: an octal value with the current permission bits of the file.

**nlink**
: the number of links to the file.

**ngid**
: the GID (numeric).

**nuid**
: the UID (numeric).

**owner**
: the owner of the file.

**group**
: the group of the file.

**inode**
: the inode of the file.

Also you can use following keywords to define the panel layout:

**space**
: a space in the display format.

**|**
: add a vertical line to the display format.

To force one field to a fixed size (a size specifier), you just add
**:**
followed by the number of characters you want the field to have.  If the
number is followed by the symbol
**+**,
then the size specifies the minimal field size - if the program finds
out that there is more space on the screen, it will then expand that
field.

For example, the
**Full**
display corresponds to this format:

half type name | size | mtime

And the
**Long**
display corresponds to this format:

full perm space nlink space owner space group space size space mtime
space name

This is a nice user display format:

half name | size:7 | type mode:3

Panels may also be set to the following modes:

**Quick View**
: In this mode, the panel will switch to a reduced
[viewer](mview.md#internal-file-viewer)
that displays the contents of the currently selected file, if you
select the panel (with the tab key or the mouse), you will have access
to the usual viewer commands.

**Info**
: The info view display information related to the currently
selected file and if possible information about the current file
system.

**Tree**
: The tree view is quite similar to the
[directory tree](#directory-tree)
feature. See the section about it for more information.

**Panelize**
: This mode shows the results of the latest
[External panelize](#external-panelize)
or
[Find file](#find-file)
commands.

# Panel modes

A panel mode is a named, reusable listing format. The list of modes is
shared between both panels and is edited and applied through two dialogs.

**Alt-t**
(also the
**Panel modes...**
entry of the Left and Right menus) opens the
**switcher:**
a plain list of the defined modes. Press
**Enter**
on a mode to apply it to the panel (the menu's panel, or the active panel
for
**Alt-t**),
or
**Esc**
to leave the panel unchanged.

The
**File panel modes...**
entry of the
**Options**
menu opens the
**manager:**
the same list, edited with keys.
**Insert**
creates a new mode,
**F4**
(or
**Enter**)
edits the selected one,
**F5**
duplicates it, and
**Delete**
(or
**F8**)
removes it. The
**Defaults**
button replaces the list with the built-in modes.
**Ok**
saves the list;
**Cancel**
(or
**Esc**)
discards every change made in the dialog.
The manager edits the global list of modes; it does not switch any panel.

The mode editor has separate inputs for the column field types and their
widths, and for the mini-status line, using the field names described under
[Listing Format...](#listing-format)
\. A field type list is comma-separated, one entry per column; a column may
contain several space-separated fields (for example
**type name**).
A width of 0 (or empty) leaves the field at its automatic width.
A complete format string (for example
**half name | size:7**)
may also be pasted into a types input: the
**|**
separators and
**:width**
suffixes are converted into the two lists.

The defined modes and each panel's selected mode are saved across sessions.

### Sort Order...

The eight sort orders are by name (alphabetical sort or version sort,
also known as natural sort), by extension, by modification time,
by access time, and by inode information modification time, by size,
by inode and unsorted.  In the Sort order dialog box you can choose
the sort order and you may also specify if you want to sort in reverse
order by checking the reverse box.

By default directories are sorted before files but this can be changed
from the
[Panel options](#panel-options)
menu (option
**Mix all files**).

### Filter...

The filter command allows you to specify a shell pattern (for example
**\*.tar.gz**)
which the files and directories must match to be shown.
The
[input line](#input-line-keys)
allow enter the pattern of file/directory names that will be shown
in the panel.

When
*Files only*
checkbox is on, only files will be matched to the filter, and all
directories will be shown. Otherwise, as files as directories will
be filtered. When
*Shell Patterns*
checkbox is on, the regular expression is much like the filename globbing
in the shell (\* standing for zero or more characters and ? standing
for one character). Otherwise, the matching of files/directories is done
with normal regular expressions (see ed(1)). When
*Case sensitive*
checkbox is on, the filtering will be case sensitive characters. Otherwise,
the case will be ignored.

### Reread

The reread command reload the list of files in the directory. It is
useful if other processes have created or removed files.

## File Menu

M-Commander uses the F1 - F10 keys as keyboard shortcuts
for commands appearing in the file menu.  The escape sequences for the
function keys are terminfo capabilities kf1 trough kf10.  On terminals
without function key support, you can achieve the same functionality by
pressing the Esc key and then a number in the range 1 through 9 and 0
(corresponding to F1 to F9 and F10 respectively).

The File menu has the following commands (keyboard shortcuts in parentheses):

**Help (F1)**

Invokes the built-in hypertext help viewer. Inside the
[help viewer](#contents),
you can use the Tab key to select the next link and the Enter key to
follow that link. The keys Space and Backspace are used to move
forward and backward in a help page. Press F1 again to get the full
list of accepted keys.

**Menu (F2)**

Invoke the
[user menu](#edit-menu-file).
The user menu provides an easy way to provide users with a menu and
add extra features to M-Commander.

**View (F3, F13)**

View the currently selected file. By default this invokes the
[Internal File Viewer](mview.md#internal-file-viewer)
but if the option "Use internal view" is off, it invokes an external
file viewer specified by the
**VIEWER**
environment variable.  If
**VIEWER**
is undefined, the
**PAGER**
environment variable is tried.  If
**PAGER**
is also undefined, the "view" command is invoked.  If you use F13
instead, the viewer will be invoked without doing any formatting or
preprocessing to the file.

See
[parameters for external viewer](#parameters-for-external-editor-or-viewer)
for explain how you may specify an extended command line options
for external viewers.

**Filtered View (Alt-!)**

This command prompts for a command
and its arguments (the argument defaults to the currently selected
file name), the output from such command is shown in the internal file
viewer.

**Edit (F4, F14)**

Press F4 to edit the highlighted file.  Press F14 (usually F14)
to start the editor with a new, empty file.
Currently they invoke the
**vi**
editor, or the editor specified in the
**EDITOR**
environment variable, or the
[Internal File Editor](mcedit6.md#internal-file-editor)
if the use_internal_edit option is on.

See
[parameters for external editor](#parameters-for-external-editor-or-viewer)
for explain how you may specify an extended command line options
for external editors.

**Copy (F5, F15)**

Press F5 to pop up an input dialog to copy the currently selected file (or
the tagged files, if there is at least one file tagged) to the
directory/filename you specify in the input dialog. The destination
defaults to the directory in the non-selected panel. Space for destination
file may be preallocated relative to preallocate_space configure option.
During this process, you can press C-c or Esc to abort the operation.
For details about source mask (which will be usually either \* or ^\\(.\*\\)$
depending on setting of Use shell patterns) and possible wildcards in the
destination see
[Mask copy/rename](#mask-copyrename).

F15 (usually F15) is similar, but defaults to the directory in the
selected panel. It always operates on the selected file, regardless of
any tagged files.

On some systems, it is possible to do the copy in the background by
clicking on the background button (or pressing Alt-b in the dialog
box).  The
[Background Jobs](#background-jobs)
is used to control the background process.

**Link (C-x l)**

Create a hard link to the current file.

**Absolute symlink (C-x s)**

Create a absolute symbolic link to the current file.

**Relative symLink (C-x v)**

Create a relative symbolic link to the current file.

To those of you who don't know what links are: creating a link to a file
is a bit like copying the file, but both the source filename and the destination
filename represent the same file image. For example, if you edit one of these
files, all changes you make will appear in both files. Some people call
links aliases or shortcuts.

A hard link appears as a real file. After making it, there is no way of
telling which one is the original and which is the link. If you delete
either one of them the other one is still intact. It is very difficult
to notice that the files represent the same image. Use hard links when
you don't even want to know.

A symbolic link is a reference to the name of the original file. If
the original file is deleted the symbolic link is useless. It is quite
easy to notice that the files represent the same image.
M-Commander shows an "@"-sign in front of the file name if it is a
symbolic link to somewhere (except to directory, where it shows a tilde (~)).
The original file which the link points to is shown on mini-status line if the
*Show mini-status*
option is enabled. Use symbolic links when you want to avoid the
confusion that can be caused by hard links.

When you press "C-x s" M-Commander will automatically fill in the
complete path+filename of the original file and suggest a name for the link.
You can change either one.

Sometimes you may want to change the absolute path of the original into
a relative path. An absolute path starts from the root directory:

*/home/frodo/mc/mc -> /home/frodo/new/mc*

A relative link describes the original file's location starting from the
location of the link itself:

*/home/frodo/mc/mc -> ../new/mc*

You can force M-Commander to suggest a relative path by pressing
"C-x v" instead of "C-x s".

**Rename/Move (F6, F16)**

Press F6 to pop up an input dialog to copy the currently selected file (or
the tagged files, if there is at least one file tagged) to the
directory/filename you specify in the input dialog.  The destination
defaults to the directory in the non-selected panel. For more details
look at Copy (F5) operation above, most of the things are quite similar.

F16 (usually F16) is similar, but defaults to the directory in the
selected panel. It always operates on the selected file, regardless of
any tagged files.

On some systems, it is possible to do the copy in the background by
clicking on the background button (or pressing Alt-b in the dialog
box).  The
[Background Jobs](#background-jobs)
is used to control the background process.

**Mkdir (F7)**

Pop up an input dialog and creates the directory specified.

**Delete (F8)**

Delete the currently selected file or the tagged files in the
currently selected panel. During the process, you can press C-c or
Esc to abort the operation.

**Quick cd (Alt-c)**
Use the
[quick cd](#quick-cd)
command if you have full command line and want to cd somewhere.

**Select group (+)**

This is used to select (tag) a group of files. M-Commander
will prompt for a selection options. When
*Files only*
checkbox is on, only files will be selected.  If
*Files only*
is off, as files as directories will be selected.
When
*Shell Patterns*
checkbox is on, the regular expression is much like the filename globbing
in the shell (\* standing for zero or more characters and ?  standing
for one character). If
*Shell Patterns*
is off, then the tagging of files is done with normal regular
expressions (see ed (1)). When
*Case sensitive*
checkbox is on, the selection will be case sensitive characters.
If
*Case sensitive*
is off, the case will be ignored.

**Unselect group (\\)**

Used to unselect a group of files. This is the opposite of the
*Select group*
command.

**Invert selection (\*)**

Select (tag) all items that are currently unselected, and unselect
(untag) all items that are currently selected. Applies to only files
or to both files and directories, depending on whether 'Reverse files
only' is enabled.

**Quit (F10, S-F10)**

Terminate M-Commander. S-F10 is used when you want to
quit and you are using the shell wrapper.  S-F10 will not take you
to the last directory you visited with M-Commander, instead
it will stay at the directory where you started M-Commander.

### Quick cd

This command is useful if you have a full command line and want to
[cd](#the-cd-internal-command)
somewhere without having to yank and paste the command line. This command
pops up a small dialog, where you enter everything you would enter after
**cd**
on the command line and then you press enter. This features all the things
that are already in the
[internal cd command](#the-cd-internal-command).

## Attributes Menu

The commands of this menu change what the file system knows about a file, not
what the file holds: the access permissions, the owner and the group, and the
flags of the file system. Each of them works on the selected file, or on the
tagged files when there are any.

**Chmod... (C-x c)**
: Set the access permissions in the
[Chmod](#chmod)
dialog.

**Chown... (C-x o)**
: Set the owner and the group in the
[Chown](#chown)
dialog.

**Advanced chown...**
: Set the permissions, the owner and the group in one dialog, see
[Advanced Chown](#advanced-chown).

**Chattr flags... (C-x e)**
: Set the flags of an ext2, ext3 or ext4 file system in the
[Chattr](#chattr)
dialog. The item is there only when the program is built with support for
those flags.

## Command Menu

The
[Directory tree](#directory-tree)
command shows a tree figure of the directories.

The
["Find file"](#find-file)
command allows you to search for a specific file.

The "Swap panels" command swaps the contents of the two directory panels.

The "Switch panels on/off" command shows the output of the last shell command.
This works only on xterm and on Linux and FreeBSD console.

The "Compare directories" command compares the directory
panels with each other. You can then use the Copy (F5) command to make
the panels identical. There are three compare methods. The quick method
compares only file size and file date. The thorough method makes a
full byte-by-byte compare. The size-only
compare method just compares the file sizes and does not check the
contents or the date times, it just checks the file size.

The
["External panelize"](#external-panelize)
allows you to execute an external program, and make the output of that
program the contents of the current panel.

The "Command history" command shows a list of typed commands. The
selected command is copied to the command line. The command history
can also be accessed by typing Alt-p or Alt-n.

The
["Hotlist"](#hotlist)
command makes changing of the current directory to often used directories
faster.

The
["Screen list"](#screen-selector)
command shows a dialog window with the list of currently running
internal editors, viewers and other M-Commander modules that support this mode.

### Directory Tree

The Directory Tree command shows a tree figure of the directories. You
can select a directory from the figure and M-Commander will
change to that directory.

There are two ways to invoke the tree. The real directory tree command
is available from Commands menu. The other way is to select tree view
from the Left or Right menu.

To get rid of long delays, M-Commander creates the tree
figure by scanning only a small subset of all the directories. If the
directory which you want to see is missing, move to its parent
directory and press C-r (or F2).

You can use the following keys:

[General movement keys](#general-movement-keys)
: are accepted.

**Enter.**
: In the directory tree, exits the directory tree and changes to this
directory in the current panel. In the tree view, changes to this
directory in the other panel and stays in tree view mode in the
current panel.

**C-r, F2 (Rescan).**
: Rescan this directory. Use this when the tree figure is out of date:
it is missing subdirectories or shows some subdirectories which don't
exist any more.

**F3 (Forget).**
: Delete this directory from the tree figure. Use this to remove clutter
from the figure. If you want the directory back to the tree figure
press F2 in its parent directory.

**F4 (Static/Dynamic).**
: Toggle between the dynamic navigation mode (default) and the static
navigation mode.

In the static navigation mode you can use the Up and Down keys to
select a directory. All known directories are shown.

In the dynamic navigation mode you can use the Up and Down keys to
select a sibling directory, the Left key to move to the parent
directory, and the Right key to move to a child directory. Only the
parent, sibling and children directories are shown, others are left
out. The tree figure changes dynamically as you traverse.

**F5 (Copy).**
: Copy the directory.

**F6 (RenMov).**
: Move the directory.

**F7 (Mkdir).**
: Make a new directory below this directory.

**F8 (Delete).**
: Delete this directory from the file system.

**C-s, Alt-s.**
: Search the next directory matching the search string. If there is
no such directory these keys will move one line down.

**C-h, Backspace.**
: Delete the last character of the search string.

**Any other character.**
: Add the character to the search string and move to the next directory
which starts with these characters. In the tree view you must first
activate the search mode by pressing C-s. The search string is shown
in the mini status line.

The following actions are available only in the directory tree. They
aren't supported in the tree view.

**F1 (Help).**
: Invoke the help viewer and show this section.

**Esc, F10.**
: Exit the directory tree. Do not change the directory.

The mouse is supported. A double-click behaves like Enter. See
also the section on
[mouse support](#mouse-support).

### Find File

The Find File feature first asks for the start directory for the
search and the filename to be searched for. By pressing the Tree
button you can select the start directory from the
[directory tree](#directory-tree)
figure.

The "File name" input field contains a filename pattern to be searched
for. It is interpreted as a shell pattern or as a regular expression
depending on the state of the "Using shell patterns" checkbox. An empty
value is valid and matches any file name.

The "Content" input field contains a string to search for within the
files. Leave this field empty to disable searching file contents.

Option "Whole words" allows select only those files containing matches that
form whole words. Like grep -w.

You can start the search by pressing the OK button.
During the search you can stop from the Stop button and continue from
the Start button.

The file list shows the modification time, the size and the permissions of
every found file along with its name. When the file contents are searched, a
file is shown once: a single match is shown along with the file name as
"file.c:12", and a file that contains more than one match shows their number
and is marked with "[+]". The matches of such a file are shown by the Left key
or by the click on the mark: the number of the line and the line itself. There
you can press Enter to go to the file, F3 to view or F4 to edit it at the
chosen match.

You can browse the filelist with the up and down arrow keys. The Chdir
button will change to the directory of the currently selected
file. The Again button will ask for the parameters for a new
search. The Quit button quits the search operation. The Panelize
button will place the found files to the current directory panel so
that you can do additional operations on them (view, copy, move,
delete and so on). To return to the normal file listing, change directory
to ".."; to see the panelized search results again, select the Panelize
mode in the Left and Right menu.

The 'Enable ignore directories' checkbox and input field below it
allow one to set up the list of directories that should be skip during the search
files (for example, you may want to avoid searches on a CD-ROM or on a NFS
directory that is mounted across a slow link). List components must be separated
with a colon, here is an example:

```
/cdrom:/nfs/wuarchive:/afs
```

Relative paths are supported also. The following example shows how to skip special
directories of version control systems:

```
/cdrom:/nfs/wuarchive:/afs:.svn:.git:CVS
```

Attention: input field can contain a dot (.), this means the current absolute path.

You may consider using the
[External panelize](#external-panelize)
command for some operations. Find file command is for simple queries
only, while using External panelize you can do as mysterious searches
as you would like.

### External panelize

The External panelize allows you to execute an external program, and
make the output of that program the contents of the current panel.

For example, if you want to manipulate in one of the panels all the
symbolic links in the current directory, you can use external
panelization to run the following command:

```
find . -type l -print
```

Upon command completion, the directory contents of the panel will no
longer be the directory listing of the current directory, but all the
files that are symbolic links. To return to the normal file listing,
change directory to ".."; to see the command output again, select
the Panelize mode in the Left and Right menu.

If you want to panelize all of the files that have been downloaded
from your FTP server, you can use this awk command to extract the file
name from the transfer log files:

```
awk '$9 ~! /incoming/ { print $9 }' < /var/log/xferlog
```

You may want to save often used panelize commands under a descriptive name,
so that you can recall them quickly. You do this by typing the command on
the input line and pressing Add new button. Then you enter a name under
which you want the command to be saved. Next time, you just choose that
command from the list and do not have to type it again.

### Hotlist

The Hotlist command shows the labels of the locations in the hotlist.
M-Commander will change to the location corresponding to the
selected label. A location is a directory, a path inside a virtual file
system, or a panel plugin address such as
*sftp:host/dir.*
From the hotlist dialog, you can remove already created label/location
pairs and add new ones. To add new locations quickly, you can use the Add
to hotlist command (C-x h), which adds the current directory or plugin
panel location into the hotlist, asking just for the label. A location
that is already in the hotlist is not added twice: the dialog shows the
existing entry instead.

Keys in the hotlist dialog:

```
Enter        change to the selected location
Alt-o        open the selected location in the other panel
Ctrl-Enter   put "cd location" into the command line
Alt-Enter    the same, for terminals without Ctrl-Enter
Insert       add the current location
Shift-F4     new entry: ask for a label and a location
F7           new group
F4           edit the label and location of the selected entry
Delete       remove the selected entry
Ctrl-Up      move the selected entry one line up
Ctrl-Down    move the selected entry one line down
F6           move the selected entry into another group: the dialog lists
             the groups, Enter opens one, Move or F6 again puts the entry at
             the end of the group shown, the starting group included
F9           sort the current group by label, groups first
Ctrl-s       search the list as you type, Ctrl-s again finds the next match
Right, Left  enter and leave a group
```

This makes cd to often used directories faster. You may consider using the
CDPATH variable as described in
[internal cd command](#the-cd-internal-command)
description.

### Background Jobs

This lets you control the state of any background M-Commander
process (only copy and move files operations can be done in the
background).  You can stop, restart and kill a background job from
here.

### Edit Menu File

The user menu is a menu of useful actions that can be customized by
the user. It comes in two forms: a menu that edits itself, kept in a key
file, and the older menu file written by hand. Where a key file exists,
that is the menu F2 opens; where none does, the old file is read as it
always was.

**The menu that edits itself**

The entries are kept in .mc6menu of the current directory and in
~/.config/mc6/menu.ini, and the two are shown together. A .mc6menu is read only where it
belongs to this user or to root and nobody else can write it, because
its entries run commands. Nothing else is
read: the menu holds what its owner put there, and mcommander ships no entries
at all, so the menu of a new user is empty and asks for its first one.
Inside the menu:

```
Enter       run the entry
Ins         add an entry
F4          edit the entry
F5          import entries from a menu file written by hand
Shift-F4    open the file the entry is kept in
Del         delete the entry
Ctrl-Up     move the entry up
Ctrl-Down   move the entry down
```

An entry is a hotkey, a label and the commands, and the dialog asks for
no more than that: there are no conditions and no file masks. The
commands take the same substitutions the old menu takes, %f, %s,
%{prompt} and the rest, described in
[macro substitution](#macro-substitution).
Two checkboxes say what to do with the output: whether it goes to the
viewer, and whether the command runs without the shell of the panel.

The label is shown with those substitutions put in as well, so a label of
"print %f" stands in the list with the name of the file under the cursor.
What the file holds does not change, and %{...} is left as it is written:
a list is no place to ask anything.

An entry can be a submenu instead of a command: Ins asks which of the two
to add. A submenu is shown with a slash after its name, the way a
directory is; Enter opens it, and the title names the submenus you are
inside. Esc goes up a level, and out of the menu at the top. Deleting a
submenu deletes what is in it. In the file, a submenu is a group with
submenu=true and no command, and an entry inside it names the submenu
in parent=.

The commands are a field of several lines: Enter opens a new one, and the
arrows, Home and End walk the text. Shift and a motion mark what the
motion goes over, the mouse marks by dragging, and Ctrl-Insert,
Shift-Insert and Shift-Delete copy, paste and cut through the clipboard
file, as they do on an input line. The Editor button leaves the dialog
and opens the file the entry is kept in, for what is easier to write
there.

An entry is written back to the file it came from, and the order of the
list is the order of the file. Conditions and masks belong to the older
form: what the dialog cannot say, the file still can, and Shift-F4 opens
it.

**The menu file written by hand**

The installation no longer ships one; what is described here is read
where somebody keeps a menu of his own in the older form:
the file .usermenu from the current directory is used if it exists, but
only if it is owned by user or root and is not world-writable. If no
such file found, ~/.config/mc6/menu is tried in the same way.

Where the menu that edits itself has no file yet and another menu is
found - his own written by hand, the usermenu of an installed mcommander, or a
menu.ini an older mcommander installed - mcommander offers once in a session to import
it; F5 in the menu, and the Import button of the empty menu, ask for it
at any time, for that file or one named by hand. It then lists what the file holds: Space
marks an entry, Ins marks it and steps down, '\*' turns every mark over,
and Enter takes the marked ones into ~/.config/mc6/menu.ini, where the
dialog can edit them. The file they came from is left where it is, and
conditions and masks are dropped, because the dialog has no place for
them.

The format of the menu file is very simple. Lines that start with
anything but space or tab are considered entries for the menu (in
order to be able to use it like a hot key, the first character should
be a letter). All the lines that start with a space or a tab are the
commands that will be executed when the entry is selected.

When an option is selected all the command lines of the option are
copied to a temporary file in the temporary directory (usually
/usr/tmp) and then that file is executed. This allows the user to put
normal shell constructs in the menus. Also simple macro substitution
takes place before executing the menu code. For more information, see
[macro substitution](#macro-substitution).

Here is a sample usermenu file:

```
A	Dump the currently selected file
	od -c %f

B	Edit a bug report and send it to root
	I=`mktemp ${MC_TMPDIR:-/tmp}/mail.XXXXXX` || exit 1
	vi $I
	mail -s "M-Commander bug" root < $I
	rm -f $I

M	Read mail
	emacs -f rmail

N	Read Usenet news
	emacs -f gnus

H	Call the info hypertext browser
	info

J	Copy current directory to other panel recursively
	tar cf - . | (cd %D && tar xvpf -)

K	Make a release of the current subdirectory
	echo -n "Name of distribution file: "
	read tar
	ln -s %d `dirname %d`/$tar
	cd ..
	tar cvhf ${tar}.tar $tar

= f *.tar.gz | f *.tgz & t n
X       Extract the contents of a compressed tar file
	tar xzvf %f
```

**Default Conditions**

Each menu entry may be preceded by a condition. The condition must
start from the first column with a '=' character. If the condition is
true, the menu entry will be the default entry.

```
Condition syntax: 	= <sub-cond>
  or:			= <sub-cond> | <sub-cond> ...
  or:			= <sub-cond> & <sub-cond> ...

Sub-condition is one of following:

  y <pattern>		syntax of current file matching pattern?
			(for edit menu only)
  f <pattern>		current file matching pattern?
  F <pattern>		other file matching pattern?
  d <pattern>		current directory matching pattern?
  D <pattern>		other directory matching pattern?
  t <type>		current file of type?
  T <type>		other file of type?
  x <filename>		is it executable filename?
  ! <sub-cond>		negate the result of sub-condition
```

Pattern is a normal shell pattern or a regular expression, according
to the shell patterns option. You can override the global value of
the shell patterns option by writing "shell_patterns=x" on the first
line of the menu file (where "x" is either 0 or 1).

Type is one or more of the following characters:

```
  n	not a directory
  r	regular file
  d	directory
  l	link
  c	character device
  b	block device
  f	FIFO (pipe)
  s	socket
  x	executable file
  t	tagged
```

For example 'rlf' means either regular file, link or fifo. The 't'
type is a little special because it acts on the panel instead of the
file. The condition '=t t' is true if there are tagged files in the
current panel and false if not.

If the condition starts with '=?' instead of '=' a debug trace will be
shown whenever the value of the condition is calculated.

The conditions are calculated from left to right. This means

```
	= f *.tar.gz | f *.tgz & t n
```

is calculated as

```
	( (f *.tar.gz) | (f *.tgz) ) & (t n)
```

Here is a sample of the use of conditions:

```
= f *.tar.gz | f *.tgz & t n
L	List the contents of a compressed tar-archive
	gzip -cd %f | tar xvf -
```

**Addition Conditions**

If the condition begins with '+' (or '+?') instead of '=' (or '=?') it
is an addition condition. If the condition is true the menu entry will
be included in the menu. If the condition is false the menu entry will
not be included in the menu.

You can combine default and addition conditions by starting condition
with '+=' or '=+' (or '+=?' or '=+?' if you want debug trace). If you
want to use two different conditions, one for adding and another for
defaulting, you can precede a menu entry with two condition lines, one
starting with '+' and another starting with '='.

Comments are started with '#'. The additional comment lines must start
with '#', space or tab.

## Options Menu

M-Commander has some options that may be toggled on and
off in several dialogs which are accessible from this menu. Options
are enabled if they have an asterisk or "x" in front of them. The menu holds,
in this order:

The
[Configuration](#configuration)
command pops up a dialog from which you can change most of settings of
M-Commander.

The
[Layout](#layout)
command pops up a dialog from which you specify a bunch of options how mcommander
looks like on the screen.

The
[Panel options](#panel-options)
command pops up a dialog from which you specify options of file manager panels.

The
[File panel modes](#panel-modes)
command opens the list of the named listing formats, where one is created,
edited and removed.

The
[Confirmation](#confirmation)
command pops up a dialog from which you specify which actions you want to
confirm.

The
[Appearance](#appearance)
command pops up a dialog from which you specify the skin.

The
[Learn keys](#learn-keys)
command pops up a dialog from which you test some keys which are not working
on some terminals and you may fix them.

The
[Key bindings](#key-bindings)
command opens the list of the actions with the keys they answer to, where a
key is reassigned and the result written to the keymap file.

The
[Key sniffer](#key-sniffer)
command shows what a terminal sends for the key that is pressed, and the
action that key is bound to.

The
**Diff viewer options**,
[Viewer options](mview.md#viewer-options)
and
**Editor options**
commands pop up the dialogs of the three programs that show a file: the
compare view, the viewer and the editor. The same dialogs are in the Options
menu of each of them; here they are reachable without opening a file first.
The compare view takes its options when it starts, so a view already on the
screen keeps the ones it was opened with.

The
[Manage plugins](#panel-plugins)
command lists the plugins that are loaded, switches one off and opens its
settings.

The
["Edit extension file"](#edit-extension-file)
command allows you to specify programs to executed when you try to
execute, view, edit and do a bunch of other thing on files
with certain extensions (filename endings).

The
**Edit highlighting group file**
command opens the file that says which names and which file types the panel
shows in which color, see
[Filenames Highlight](#filenames-highlight).

The
[Save setup](#save-setup)
command saves the current settings of the Left, Right and Options
menus. A small number of other settings is saved, too.

The
**About**
command shows the version of the program and who wrote it.

### Configuration

The options in this dialog are divided into several groups: "File
operation options", "Esc key mode", "Pause after run" and "Other options".

**File operation options**

*Verbose operation.*
This toggles whether the file Copy, Rename and Delete operations are
verbose (i.e., display a dialog box for each operation). If you have a
slow terminal, you may wish to disable the verbose operation. It is
automatically turned off if the speed of your terminal is less than
9600 bps.

*Compute totals.*
If this option is enabled, M-Commander computes total byte
sizes and total number of files prior to any Copy, Rename and Delete
operations. This will provide you with a more accurate progress bar
at the expense of some speed. This option has no effect, if
*Verbose operation*
is disabled.

*Classic progressbar.*
If this option is enabled, the progressbar of Copy/Move/Delete operations
is always grown form left to right. If disabled, the growing direction
of progressbar follows to direction of Copy/Move/Delete operation:
from left panel to right one and vice versa. Enabled by default.

*Mkdir autoname.*
When you press F7 to create a new directory, the input line in popup dialog
will be filled by name of current file or directory in active panel.
Disabled by default.

*Preallocate space.*
Preallocate space for whole target file, if possible, before copy operation.
Disabled by default.

**Esc key mode.**

By default, M-Commander treats the Esc key as a key prefix.
Therefore, you should press Esc code twice to exit a dialog. But there is
a possibility to use a single press of Esc key for that action.

*Single press.*
By default this option is disabled. If you'll enable it, the Esc key
will act as a prefix key for set up time interval (see
*Timeout*
option below), and if no extra keys have arrived, then the Esc key
is interpreted as a cancel key (Esc Esc).

*Timeout.*
This options is used to setup the time interval (in microseconds)
for single press of Esc key. By default, this interval is one second
(1000000 microseconds). Also the timeout can be set via KEYBOARD_KEY_TIMEOUT_US
environment variable (also in microseconds), which has higher priority
than Timeout option value.

**Pause after run**

After executing your commands, M-Commander can pause, so
that you can examine the output of the command.  There are three
possible settings for this variable:

*Never.*
Means that you do not want to see the output of your command.  If you
are using the Linux or FreeBSD console or an xterm, you will be able to
see the output of the command by typing C-o.

*On dumb terminals.*
You will get the pause message on terminals that are not capable of
showing the output of the last command executed (any terminal that is
not an xterm or the Linux console).

*Always.*
The program will pause after executing all of your commands.

**Other options**

*Use internal editor.*
If this option is enabled, the built-in file editor is used to edit
files. If the option is disabled, the editor specified in the
**EDITOR**
environment variable is used.
If no editor is specified,
**vi**
is used.  See the section on the
[internal file editor](mcedit6.md#internal-file-editor).

*Use internal viewer.*
If this option is enabled, the built-in file viewer is used to view
files. If the option is disabled, the pager specified in the
**PAGER**
environment variable is used.
If no pager is specified, the
**view**
command is used.  See the section on the
[internal file viewer](mview.md#internal-file-viewer).

*Ask new file name.*
If this option is enabled, file name is asked before open new file in editor.

*Auto menus.*
If this option is enabled, the user menu will be invoked at startup.
Useful for building menus for non-unixers.

*Drop down menus.*
When this option is enabled, the pull down menus will be activated as
soon as you press the F9 key. Otherwise, you will only get the menu title,
and you will have to activate the menu either with the arrow keys or with
the hotkeys. It is recommended if you are using hotkeys.

*Shell Patterns.*
By default the Select, Unselect and Filter commands will use shell-like
regular expressions. The following conversions are performed to achieve
this: the '\*' is replaced by '.\*' (zero or more characters); the '?'
is replaced by '.' (exactly one character) and '.' by the literal
dot. If the option is disabled, then the regular expressions are the
ones described in ed(1).

*Complete: show all.*
By default, M-Commander pops up all possible
[completions](#completion)
if the completion is ambiguous only when you press
**Alt-Tab**
for the second time.  For the first time, it just completes as much as
possible and beeps in the case of ambiguity.  Enable this option if you
want to see all possible completions even after pressing
**Alt-Tab**
the first time.

*Rotating dash.*
If this option is enabled, the
M-Commander shows a rotating dash in the upper right corner
as a work in progress indicator.

*Cd follows links.*
This option, if set, causes M-Commander to follow the
logical chain of directories when changing current directory
either in the panels, or using the cd command. This is the default
behavior of bash. When unset, M-Commander follows the
real directory structure, so cd .. if you've entered that directory
through a link will move you to the current directory's real parent
and not to the directory where the link was present.

*Safe delete.*
If this option is enabled, deleting files and directory hotlist entries
unintentionally becomes more difficult.  The default selection in the
confirmation dialogs for deletion changes from
**Yes**
to
**No**.
This option is disabled by default.

*Safe overwrite.*
If this option is enabled, overwriting files unintentionally becomes
more difficult.  The default selection in the overwrite confirmation dialog
changes from
**Yes**
to
**No**.
This option is disabled by default.

*Auto save setup.*
If this option is enabled, when you exit M-Commander, the
configurable options of M-Commander are saved in the
~/.config/mc6/ini file.

*Far Manager keys.*
If this option is enabled, the panels answer to the keys of Far Manager; see
[Far mode](#far-mode). This option is disabled by default.

### Layout

The layout dialog gives you a possibility to change the general layout
of screen. The options in this dialog are divided into several groups:
"Panel split", "Console output" and "Other options".

**Panel split**

The rest of the screen area is used for the two directory panels. You
can specify whether the area is split to the panels in
*Vertical*
or
*Horizontal*
direction. Panel layout can be changed using Alt-, (Alt-comma) shortcut.

*Equal split.*
By default, panels have equal sizes. Using this option you can specify
an unequal split.

**Console output**

On the Linux or FreeBSD console you can specify how many lines are shown
in the output window. This option is available if M-Commander runs
on native console only.

**Other options**

*Menu bar visible.*
If enabled, main menu of M-Commander is always visible on the top row
of screen above panels. Enabled by default.

*Command prompt.*
If enabled, command line is available. Enabled by default.

*Keybar visible.*
If enabled, 10 labels associated with F1-F10 keys are located at the bottom
row of screen. Enabled by default.

*Hintbar visible.*
If enabled, the one-line hints are visible below panels. Enabled by default.

*XTerm window title.*
When run in a terminal emulator for X11, M-Commander sets the
terminal window title to the current working directory and updates it
when necessary.  If your terminal emulator is broken and you see some
incorrect output on startup and directory change, turn off this option.
Enabled by default.

*Show free space.*
If enabled, free space and total space of current file system is shown
at the bottom frame of panel. Enabled by default.

### Panel options

**Main panel options**

*Show mini-status.*
If enabled, one line of status information about the currently selected item
is shown at the bottom of the panels. Enabled by default.

*Use SI size units.*
If this option is enabled, M-Commander will use SI prefixes (base 10)
when displaying any byte sizes. If disabled (default), M-Commander will
use IEC prefixes (base 2).

*Mix all files.*
If this option is enabled, all files and directories are shown mixed
together.  If the option is disabled (default), directories (and links to
directories) are shown at the beginning of the listing, and other files below.

*Show backup files.*
If enabled, M-Commander will show files ending with a tilde.
Otherwise, they won't be shown (like GNU's ls option -B). Enabled by default.

*Show hidden files.*
If enabled, M-Commander will show all files that start with
a dot (like ls -a). Disabled by default.

*Fast directory reload.*
If this option is enabled, M-Commander will use a trick to
determine if the directory contents have changed.  The trick is to reload
the directory only if the i-node of the directory has changed; this means
that reloads only happen when files are created or deleted.  If what
changes is the i-node for a file in the directory (file size changes,
mode or owner changes, etc) the display is not updated.  In these cases,
if you have the option on, you have to rescan the directory manually
(with C-r). Disabled by default.

*Mark moves down.*
If enabled, the selection bar will move down when you mark a file (with
Insert key). Enabled by default.

*Reverse files only.*
If enabled, 'Invert selection' in the File menu will apply to only files
rather than to both files and directories. Enabled by default.

*Simple swap.*
If both panels contain file listing, simple swap means that panels exchange
its screen positions: left panel become right one, and vice versa. If this
option is unchecked, file listing panels exchange its content keeping listing
format and sort options. Unchecked by default.

*Auto save panels setup.*
If this option is enabled, when you exit M-Commander, the
current settings of panels are saved in the ~/.config/mc6/panels.ini file.
Disabled by default.

*Watch directories.*
If this option is enabled, M-Commander asks the kernel to tell it about
changes in the directories the panels show, and rereads a panel when a file
in it is created, removed or changed by something else: another terminal, a
build, or the shell of the terminal window. A panel that is off screen is
reread when it comes back. One watch covers a whole directory, so the cost
does not depend on the number of files in it, and a burst of changes leads
to one reread. Directories of a virtual file system, and those on a file
system the kernel cannot watch, such as NFS, work as they did: there C-r
does the job. While this option is on, Fast directory reload has nothing to
save and is shown disabled. Enabled by default.

**Navigation**

*Lynx-like motion.*
If this option is enabled, you may use the arrows keys to automatically
chdir if the current selection is a subdirectory and the shell command
line is empty. By default, this setting is off.

*Page scrolling.*
If set (the default), panel will scroll by half the display when the
cursor reaches the end or the beginning of the panel, otherwise it
will just scroll a file at a time.

*Center scrolling.*
If set, panel will scroll when the cursor reaches the middle of the
panel column, only hitting the top or bottom of the panel when actually on
the first or last file. This behavior applies when scrolling one file
at a time, and does not apply to the page up/down keys.

*Mouse page scrolling.*
Controls whenever scrolling with the mouse wheel is done by pages or
line by line on the panels.

**File highlight**

You can specify whether
*permissions*
and
*file types*
should be highlighted with distinctive
[Colors](#colors).
If the permission highlighting is enabled, the parts of the
*perm*
and
*mode*
[display fields](#listing-format)
which apply to the user running M-Commander are highlighted with
the color defined by the
*marked*
keyword.  If
*permission colors*
are enabled, every character of the
*perm*
field is colored by what it stands for: the
*permread ,*
*permwrite ,*
*permexec ,*
*permspecial*
and
*permnone*
colors of the skin for r, w, x, s/t and -.  Both can be on at once; the
triplet that applies to the user then keeps the
*marked*
color.  If the file type highlighting is enabled, file names are colored
according to rules described in
{{sysconfdir}}/mcommander/filehighlight.ini
file. See
[Filenames Highlight](#filenames-highlight)
for more info.

**Quick search and quick filter**

You can specify how the
[Quick search](#quick-search)
and Quick filter modes should work: case insensitively, case sensitively or be matched
to the panel sort order: case sensitive or not.

### Confirmation

In this dialog you configure the confirmation options for file deletion,
overwriting files, execution by pressing enter, quitting the program,
directory hotlist entries deletion and history cleanup.

### Appearance

In this dialog you can select the skin to be used and enable shadow
for dialogs and drop down menus.

See the
[Skins](#skins)
section for technical details about the skin definition files.

*Shadows.*
If this option is enabled, all dialogs and drop down menus will have a shadow.

### Learn keys

This dialog teaches mcommander the escape sequences your terminal sends
for function keys, arrows and navigation keys.

Select a modifier combination (Ctrl, Alt, Shift) using the
checkboxes, then click a key button. Press the physical key
and wait for the capture message to disappear. The learned
sequence appears next to the button.

**Del**
\- clear a learned key.

**Save**
\- write learned keys to ~/.config/mc6/term/\<TERM>.

**Edit term file**
\- open the terminal key definitions file in the editor.

The old terminal key definitions from [terminal:TERM] in
~/.config/mc6/ini are migrated automatically on first run.

### Manage plugins <a id="manage-plugins"></a>

The plugins the program has loaded, in a table: the kind, the name and what the
plugin says about itself. A plugin is switched off and on with the checkbox of
its row, and what is switched off is not loaded the next time either.

**Enter, F4**
: Open the settings of the plugin the cursor is on. A plugin that has none says
so.

The
[panel plugins](#panel-plugins)
are listed here together with the editor plugins and the packages of Lua
scripts; the scripts of a package are listed by the
[Lua scripts](#lua-scripts)
dialog of its settings.

### Lua scripts <a id="lua-scripts"></a>

The scripts of a Lua package, in a table: the name, the identifier, where the
script lives, what it provides and what it does. A script is switched off and
on with the checkbox of its row.

**Settings**
: Run the script that carries the settings of the package, where it has one.

### Send to panel <a id="panel-plugins"></a>

When a file can be opened by more than one panel plugin, this list asks which
one to send it to. Enter takes the plugin the cursor is on, Esc leaves the
file where it is.

### File exists <a id="plugin-file-exists"></a>

A copy into a panel plugin found a file of that name there. The dialog shows
the path, the size and the time of what is being copied and of what is already
there, and asks what to do: overwrite it, skip it, resume the copy where it
stopped, when the plugin can continue one, or stop the whole operation.

### Choose codepage <a id="codepages-translation"></a>

The list of the codepages the program knows, from
**{{pkgdatadir}}/charsets**.
Choosing one tells the program in which codepage the names or the text at hand
are written, and
**\<No translation>**
leaves them as bytes. The list is opened by
**Alt-e**
in a panel, in the viewer and in the editor, and by the Encoding item of their
menus.

### History <a id="history-query"></a>

The list of what was typed into an input line before, newest first, which
**Alt-h**
opens for the line the cursor is in. Enter takes the entry the cursor is on
into the line, Esc leaves the line as it was, and
**F8, Del**
removes the entry the cursor is on from the history.

### Edit Extension File

This will invoke your editor on the file
*~/.config/mc6/extensions.ini.*
If this file does not exist and you are not root, it will be copied from
*{{sysconfdir}}/mcommander/extensions.ini.*
If you are root, you can choose the file to edit: user's
*~/.config/mc6/extensions.ini*
or system-wide
*{{sysconfdir}}/mcommander/extensions.ini.*
The format of this file is described in detail in it.

### Save Setup

At startup, M-Commander tries to load initialization information
from the ~/.config/mc6/ini file.
If this file doesn't exist, the system-wide file
**{{sysconfdir}}/mcommander/mc.ini**
is used. If this file doesn't exist, the system-wide file
**{{pkgdatadir}}/mc.ini**
is used. If this file doesn't exist, M-Commander uses the default settings.

The
*Save Setup*
command creates the ~/.config/mc6/ini file by saving the
current settings of the
[Left, Right](#left-and-right-menus)
and
[Options](#options-menu)
menus.

If you activate the
*auto save setup*
option, M-Commander will always save the current settings when exiting.

There also exist settings which can't be changed from the menus. To
change these settings you have to edit the setup file with your
favorite editor. See the section on
[Special Settings](#special-settings)
for more information.

<!-- help:break -->

# Executing operating system commands

You may execute commands by typing them directly in
M-Commander's input line, or by selecting the program you want to
execute with the selection bar in one of the panels and hitting Enter.

If you press Enter over a file that is not executable,
M-Commander checks the extension of the selected file against the
extensions in the
[Extensions File](#edit-extension-file).
If a match is found then the code associated with that extension is
executed. A very simple
[macro expansion](#macro-substitution)
takes place before executing the command.

## The cd internal command

The
*cd*
command is interpreted by M-Commander, it is not passed to
the command shell for execution.  Thus it may not handle all of the
nice macro expansion and substitution that your shell does, although it
does some of them:

*Tilde substitution.*
The (~) will be substituted with your home directory, if you append a
username after the tilde, then it will be substituted with the login
directory of the specified user.

For example, ~guest is the home directory for the user guest, while
~/guest is the directory guest in your home directory.

*Previous directory.*
You can jump to the directory you were previously by using the special
directory name '-' like this:
**cd -**

*CDPATH directories.*
If the directory specified to the
**cd**
command is not in the current directory, then M-Commander
uses the value in the environment variable
**CDPATH**
to search for the directory in any of the named directories.

For example you could set your
**CDPATH**
variable to ~/src:/usr/src, allowing you to change your directory to
any of the directories inside the ~/src and /usr/src directories, from
any place in the file system by using its relative name (for example
cd linux could take you to /usr/src/linux).

## Macro Substitution

When accessing a
[user menu](#edit-menu-file),
or executing an
[extension dependent command](#edit-extension-file),
or running a command from the command line input, a simple macro
substitution takes place.

The macros are:

*%i*
: The indent of blank space, equal the cursor column position.  For edit
menu only.

*%y*
: The syntax type of current file. For edit menu only.

*%b*
: The block file name.

*%e*
: The error file name.

*%m*
: The current menu name.

*%f* and *%p*
: In file manager user menu: the current file name in selected panel.
In mcedit6 user menu: the name of opened file.

*%x*
: The extension of current file name.

*%n*
: The current file name without extension.

*%d*
: The current directory name.

*%F*
: The current file in the unselected panel.

*%D*
: The directory name of the unselected panel.

*%t*
: The currently tagged files.

*%T*
: The tagged files in the unselected panel.

*%v* and *%V*
: Similar to the %t and %T, but expands to full names of tagged files.
*%u* and *%U*
Similar to the %t and %T macros, but in addition the files are untagged.
You can use this macro only once per menu file entry or extension file
entry, because next time there will be no tagged files.

*%s* and *%S*
: The selected files: The tagged files if there are any. Otherwise the
current file.

*%cd*
: This is a special macro that is used to change the current directory
to the directory specified in front of it.  This is used primarily as
an interface to the
[Virtual File System](#virtual-file-system).

*%view*
: This macro is used to invoke the internal viewer.  This macro can be
used alone, or with arguments.  If you pass any arguments to this
macro, they should be enclosed in brackets.

> The arguments are:
> *ascii*
> to force the viewer into ascii mode;
> *hex*
> to force the viewer into hex mode;
> *nroff*
> to tell the viewer that it should interpret the bold and underline
> sequences of nroff;
> *unformatted*
> to tell the viewer to not interpret nroff commands for making the text
> bold or underlined;
> *structured*
> to open the file in the structured (tree) mode.

*%%*
: The % character

*%{some text}*
: Prompt for the substitution. An input box is shown and the text inside
the braces is used as a prompt. The macro is substituted by the text
typed by the user. The user can press Esc or F10 to cancel. This macro
doesn't work on the command line yet.

*%var{ENV:default}*
: If environment variable
*ENV*
is unset, the
*default*
is substituted.  Otherwise, the value of
*ENV*
is substituted.

## The terminal

M-Commander runs your shell in a pseudo terminal behind the
panels. It works with the shells: bash, ash (BusyBox and Debian),
(o/m)ksh, tcsh, zsh and fish.

The shell is the one defined in the
**SHELL**
variable and if it is not defined, then the one in the /etc/passwd
file. Rather than invoking a new shell each time you execute a command,
the command is passed to that shell as if you had typed it.  This also
allows you to change the environment variables, use shell functions and
define aliases that are valid until you quit M-Commander.

**bash**
users may specify startup commands in ~/.local/share/mc6/bashrc (fallback ~/.bashrc)
and special keyboard maps in ~/.local/share/mc6/inputrc (fallback ~/.inputrc).

**ash/dash**
users (BusyBox or Debian) may specify startup commands in ~/.local/share/mc6/ashrc (fallback ~/.profile).

**ksh/oksh**
users (PD ksh variants) may specify startup commands in ~/.local/share/mc6/kshrc
(fallback
*ENV*
or ~/.profile).

**mksh**
users (MirBSD ksh) may specify startup commands in ~/.local/share/mc6/mkshrc
(fallback
*ENV*
or ~/.mkshrc).

**zsh**
users may specify startup commands in ~/.local/share/mc6/.zshrc (fallback ~/.zshrc).

**tcsh, fish**
users cannot specify mcommander-specific startup commands at present. They have to rely on
shell-specific startup files.

You can suspend applications at any
time with the sequence Ctrl-o and jump back to M-Commander, if
you interrupt an application, you will not be able to run other
external commands until you quit the application you interrupted.

Behind the panels the terminal keeps a scrollback of everything the shell
has printed, and while no panel is on screen it can be read, marked and
cleared.  The cursor keys walk the output and the shifted cursor keys mark
it, both while the terminal itself holds the focus; the keys that only move
the view work whoever is typing.  Every key not named below is typed into
the shell.

```
Ctrl-Insert    copy the marked output to the clipboard
Ctrl-Shift-u   take the mark back
Alt-s          search the output for what is typed next
Alt-Shift-s    show only the rows that match what is typed next
Ctrl-l         clear the screen, keeping the scrollback
Ctrl-Shift-l   clear the screen and the scrollback both
               (also Ctrl-Alt-l)
```

Alt-s and Alt-Shift-s take a pattern the way they do in the panels: it is
typed on the top row of the screen, and the output follows it as it
grows.  Case does not matter.  A search goes down the output from the
cursor and marks the nearest match; Alt-s again marks the one below it,
and past the newest row the search goes round to the oldest.  Where the
shell is typing nothing has been read yet, and the search takes the
output from its oldest row.  A filter shows the rows that match and no
others, and the cursor keys walk them while the pattern is still being
typed; Alt-Shift-s again moves the cursor to the row below.  The
*search_direction*
setting turns both round, to run up the output the way
**less**
does.  Pressed with nothing typed, either key takes the
pattern it had last.  Backspace takes a character back, and a character
that nothing matches is not taken.  Enter ends the typing and leaves the
view on what was found, the match still marked; Esc ends it and puts back
the view of before the typing: the cursor where it was reading, or at the
prompt where nothing was read, and the filter and the mark that were there.
Any other key ends the typing and then does what it does.

With no panel on screen most function keys are the terminal's own and the
button bar names them.  The file manager's View, Edit, Copy, RenMov and
Delete are not on offer there: they work on the file the panel cursor
stands on, and that cursor cannot be seen.  F8 is left empty on purpose,
so that the reach for Delete does nothing rather than something else.
F7 makes a directory and Shift-F4 edits a new file, as they do with the
panels up: both work in the directory of the panel, which is the one the
shell is in.

```
F2           copy the marked output to the clipboard
F3           mark the whole output, or take back the mark there is
F4           cut the output down to the rows that match the mark,
             or the word under the cursor
F5           turn that filter off, and on again
F6           clear the screen and the scrollback both
```

F1, F7, Shift-F4, F9 and F10 stay the file manager's while the shell waits
at its prompt, and F1 opens the help on this section.
Once a command is running the screen and every key on it are that command's,
these among them.  The five above are the exception: they stay the terminal's
while a command is running.  A full-screen application, an editor or a pager,
takes every key itself, those included.  All of them are listed in the
**[mcterm]**
section of the keymap file and can be redefined there.

If you type
**mcommander**
without arguments at the shell prompt behind the hidden panels, the running
M-Commander shows its panels again instead of starting a second copy.
Pass an argument, a directory name for example, to start a nested
M-Commander as before.

The basic prompt displayed by M-Commander is of the form
"user@host:current_path$ ". When using a capable shell, like Bash, the
prompt displayed by M-Commander will be the same prompt that you
are currently using in your shell.

(There's a known problem when using fish: the prompt is displayed only in
full screen mode (Ctrl-o), not when the panels are visible.)

To set a specific shell different from your current SHELL variable or
login shell defined in /etc/passwd, you may call M-Commander like this:
**SHELL=/bin/myshell mcommander**

# Chmod

The Chmod window is used to change the attribute bits in a group of
files and directories.  It can be invoked with the C-x c key combination.

The Chmod window has two parts -
*Permissions*
and
*File.*

In the File section are displayed the name of the file or directory
and its permissions in octal form, as well as its owner and group.

In the Permissions section there is a set of check buttons which
correspond to the file attribute bits.  As you change the attribute
bits, you can see the octal value change in the File section.

To move between the widgets (buttons and check buttons) use the
*arrow keys*
or the
*Tab*
key.  To change the state of the check buttons or to select a button
use
*Space.*
You can also use the hotkeys on the buttons to quickly activate them.
Hotkeys are shown as highlighted letters on the buttons.

To set the attribute bits, use the Enter key.

When working with a group of files or directories, you just click on
the bits you want to set or clear.  Once you have selected the bits
you want to change, you select one of the action buttons (Set marked
or Clear marked).

Finally, to set the attributes exactly to those specified, you can use
the
**[Set all]**
button, which will act on all the tagged files.

**[Marked all]**
set only marked attributes to all selected files

**[Set marked]**
set marked bits in attributes of all selected files

**[Clean marked]**
clear marked bits in attributes of all selected files

**[Set]**
set the attributes of one file

**[Cancel]**
cancel the Chmod command

# Chown

The Chown command is used to change the owner/group of a file. The hot
key for this command is C-x o.

# Advanced Chown

The Advanced Chown command is the
[Chmod](#chmod)
and
[Chown](#chown)
command combined into one window. You can change the permissions and
owner/group of files at once.

# Chattr

The Chattr window is used to change the attributes of a group of files
and directories on a Linux file system. It can be invoked with the C-x e
key combination.

Not all attributes are supported or utilized by all filesystems.
List of available attribute flags is represented as a set of check buttons
which correspond to the attribute flags (see
**chattr(1)**
for details). As you change the attribute flags, you can see the symbolic
value change below file name.

To move between the widgets (buttons and check buttons) use the
*arrow keys*
or the
*Tab*
key. To change the state of the check buttons or to select a button use
**Space**.

To set the attributes, use the Enter key.

When working with a group of files or directories, you just click on
the flags you want to set or clear. Once you have selected the flags
you want to change, you select one of the action buttons (Set marked
or Clear marked).

Finally, to set the attributes exactly to those specified, you can use
the
**[Set all]**
button, which will act on all the tagged files.

**[Marked all]**
set only marked attributes to all selected files.

**[Set marked]**
set marked flags in attributes of all selected files.

**[Clean marked]**
clear marked flags in attributes of all selected files.

**[Set]**
set the attributes of one file.

**[Cancel]**
cancel the Chattr command.

# File Operations

When you copy, move or delete files, M-Commander shows the
file operations dialog.  It shows the files currently being processed
and uses up to two progress bars.  The file bytes bar indicates the
percentage of the current file that has been processed so far.  The
total bytes bar indicates the percentage of the total size of the tagged
files that has been handled. Counters that show how many of the tagged
files have been handled are displayed. If the
*Verbose*
option is off, the file bytes bar and total bytes bar are not shown.

There are three buttons at the bottom of the dialog:

**[Skip]**
: button to skip the rest of the current file.

**[Suspend]**
: button to suspend the file operation and button transforms to the
**[Continue]**
one which continue the suspended operation.

**[Abort]**
: button to abort the whole operation, the rest of the files are skipped.

There are three other dialogs which you can run into during the file
operations.

The error dialog informs about error conditions and has four choices:

**[Ignore]**
: button to ignore this error.

**[Ignore all]**
: button to ignore this and all future errors.

**[Abort]**
: button to abort the operation altogether.

**[Retry]**
: button to continue if you fixed the problem from another terminal.

### Replace <a id="replace"></a>

The replace dialog is shown when you attempt to copy or move a file on
the top of an existing file.  The dialog shows the dates and sizes of
the both files. There are the following buttons in this dialog:

**[Yes]**
: button to overwrite the file.

**[No]**
: button to skip the file.

**[Append]**
: button to append the source file to the target one.

**[Reget]**
: button to append the rest of the source file to the target one.
This button is displayed only if the size of the target file
is non-zero and less than the size of the source file.

**[All]**
: button to overwrite all the files.

**[Older]**
: button to overwrite if the source file is newer than the target file.

**[None]**
: button to never overwrite files

**[Smaller]**
: button to overwrite if the source file size is less than the target one.

**[Size differs]**
: button to overwrite files with different sizes.

**[Abort]**
: button to abort the whole operation.

If the
**Don't overwrite with zero length file**
checkbox is on, the zero-sized source files don't overwrite the
non-zero-sized target files.

The recursive delete dialog is shown when you try to delete a directory
which is not empty. There are the following buttons in this dialog:

**[Yes]**
: button to delete the directory recursively.

**[No]**
: button to skip the directory.

**[All]**
: button to delete all the directories.

**[None]**
: button to skip all the non-empty directories.

**[Abort]**
: button to abort the whole operation.

If you have tagged files and perform an operation on them only the files
on which the operation succeeded are untagged. Failed and skipped files
are left tagged.

# Mask Copy/Rename

The copy/move operations let you translate the names of files in an
easy way.  To do it, you have to specify the correct source mask and
usually in the trailing part of the destination specify some wildcards.
All the files matching the source mask are copied/renamed according to
the target mask.  If there are tagged files, only the tagged files
matching the source mask are renamed.

There are other options which you can set:

**Follow links**

determines whether make the symlinks and hardlinks in the source
directory (recursively in subdirectories) new links in the target
directory or whether would you like to copy their content.

**Dive into subdirs**

determines the behavior when the source directory is about to be copied,
but the target directory already exists.  The default action is to copy
the contents of the source directory into the target directory.
Enabling this option causes copying the source directory itself into the
target directory.

For example, you want to copy directory
*/foo*
containing file
*bar*
to
*/bla/foo,*
which is an already existing directory.  Normally (when
**Dive into subdirs**
is not set), mcommander would copy file
*/foo/bar*
into the file
*/bla/foo/bar.*
By enabling this option the
*/bla/foo/foo*
directory will be created, and
*/foo/bar*
will be copied into
*/bla/foo/foo/bar.*

**Preserve attributes**

determines whether to preserve the permissions, timestamps and (if you
are root) the ownership of the original files.  If this option is not
set, the current value of the umask will be respected.

**Preserve ext2 attributes**

determines whether to preserve the attributes of files and directories
on an ext2/3/4 file system.

**Use shell patterns**

When this option is on you can use the '\*' and '?' wildcards in the source
mask. They work like they do in the shell. In the target mask only the '\*'
and '\\\<digit>' wildcards are allowed. The first '\*' wildcard in the target
mask corresponds to the first wildcard group in the source mask,
the second '\*' corresponds to the second group and so on.  The '\\1' wildcard
corresponds to the first wildcard group in the source mask, the '\\2' wildcard
corresponds to the second group and so on all the way up to '\\9'.
The '\\0' wildcard is the whole filename of the source file.

Two examples:

If the source mask is "\*.tar.gz", the destination is "/bla/\*.tgz" and the
file to be copied is "foo.tar.gz", the copy will be "foo.tgz" in "/bla".

Suppose you want to swap basename and extension so that "file.c" would
become "c.file" and so on.  The source mask for this is "\*.\*" and the
destination is "\\2.\\1".

**Use shell patterns off**

When the shell patterns option is off the M-Commander doesn't do automatic
grouping anymore. You must use '\\(...\\)' expressions in the source
mask to specify meaning for the wildcards in the target mask. This is
more flexible but also requires more typing. Otherwise target masks
are similar to the situation when the shell patterns option is on.

Two examples:

If the source mask is "^\\(.\*\\)\\.tar\\.gz$", the destination is
"/bla/\*.tgz" and the file to be copied is "foo.tar.gz", the copy
will be "/bla/foo.tgz".

Let's suppose you want to swap basename and extension so that "file.c"
will become "c.file" and so on. The source mask for this is
"^\\(.\*\\)\\.\\(.\*\\)$" and the destination is "\\2.\\1".

**Case Conversions**

You can also change the case of the filenames.  If you use '\\u'
or '\\l' in the target mask, the next character will be converted to
uppercase or lowercase correspondingly.

If you use '\\U' or '\\L' in the target mask, the next characters will
be converted to uppercase or lowercase correspondingly up to the
next '\\E' or next '\\U', '\\L' or the end of the file name.

The '\\u' and '\\l' are stronger than '\\U' and '\\L'.

For example, if the source mask is '\*' (
*Use shell patterns*
on) or '^\\(.\*\\)$' (
*Use shell patterns*
off) and the target mask is '\\L\\u\*' the file names will be converted
to have initial upper case and otherwise lower case.

You can also use '\\' as a quote character. For example, '\\\\' is
a backslash and '\\\*' is an asterisk.

**Stable symlinks**

commands M-Commander, that it should change symlinks in the target,
so that they'll point to the same location as it did before. With absolute
symbolic links this does nothing, but if you have a relative one, it will
recompute its value, adding necessary ../ and other directory parts and making
the value as short as possible (most modern filesystems keep short symlinks
inside inodes and thus don't waste much disk space).

# Select/Unselect Files

The dialog of group of files and directories selection or uselection.
The
[input line](#input-line-keys)
allow enter the regular expression of filenames that will be
selected/unselected.

When
*Files only*
checkbox is on, only files will be selected.  If
*Files only*
is off, as files as directories will be selected.
When
*Shell Patterns*
checkbox is on, the regular expression is much like the filename globbing
in the shell (\* standing for zero or more characters and ?  standing
for one character). If
*Shell Patterns*
is off, then the tagging of files is done with normal regular
expressions (see ed (1)). When
*Case sensitive*
checkbox is on, the selection will be case sensitive characters.
If
*Case sensitive*
is off, the case will be ignored.

# Regex Quick Reference

**Common Tokens**

```
A single character of: a, b or c        [abc]
A character except: a, b or c           [^abc]
A character in the range: a-z           [a-z]
A character not in the range: a-z       [^a-z]
A character in the range: a-z or A-Z    [a-zA-Z]
Any single character                    .
Alternate - match either a or b         a|b
Any whitespace character                \s
Any non-whitespace character            \S
Any digit                               \d
Any non-digit                           \D
Any word character                      \w
Any non-word character                  \W
Non-capturing group                     (?:...)
Capturing group                         (...)
Zero or one of a                        a?
Zero or more of a                       a*
One or more of a                        a+
Exactly 3 of a                          a{3}
3 or more of a                          a{3,}
Between 3 and 6 of a                    a{3,6}
Start of string                         ^
End of string                           $
A word boundary                         \b
Non-word boundary                       \B
```

**Anchors**

```
Start of match                          \G
Start of string                         ^
End of string                           $
Start of string                         \A
End of string                           \Z
Absolute end of string                  \z
A word boundary                         \b
Non-word boundary                       \B
```

**General Tokens**

```
Newline                                 \n
Carriage return                         \r
Tab                                     \t
Null character                          \0
```

**Meta Sequences**

```
Any single character                    .
Alternate: match a or b                 a|b
Any whitespace character                \s
Any non-whitespace character            \S
Any digit                               \d
Any non-digit                           \D
Any word character                      \w
Any non-word character                  \W
Unicode seq., linebreaks included       \X
Unicode newlines                        \R
Match anything but a newline            \N
Vertical whitespace character           \v
Negation of \v                          \V
Horizontal whitespace character         \h
Negation of \h                          \H
Reset match                             \K
Match subpattern number #               \#
Unicode property X                      \pX
Unicode property or script category     \p{...}
Negation of \pX                         \PX
Negation of \p{...}                     \P{...}
Quote; treat as literals                \Q...\E
Match subpattern 'name'                 \k{name}
Match subpattern 'name'                 \k<name>
Match subpattern 'name'                 \k'name'
Match nth subpattern                    \gn
Match nth subpattern                    \g{n}
Nth relative previous subpattern        \g{-n}
Nth capture group expression            \g<n>
Nth upcoming capture group expr.        \g<+n>
Nth capture group expression            \g'n'
Nth upcoming subpattern expr.           \g'+n'
Match named capture group               \g{letter}
Named capture group expression          \g<letter>
Named capture group expression          \g'letter'
Hex character YY                        \xYY
Hex character YYYY                      \x{YYYY}
Octal character ddd                     \ddd
Control character Y                     \cY
Backspace character                     [\b]
Makes any character literal             \
```

**Quantifiers**

```
Zero or one of a                        a?
Zero or more of a                       a*
One or more of a                        a+
Exactly 3 of a                          a{3}
3 or more of a                          a{3,}
Between 3 and 6 of a                    a{3,6}
Greedy quantifier                       a*
Lazy quantifier                         a*?
Possessive quantifier                   a*+
```

**Character Classes**

```
A single character of: a, b or c        [abc]
A character except: a, b or c           [^abc]
A character in the range: a-z           [a-z]
A character not in the range: a-z       [^a-z]
Character in range: a-z or A-Z          [a-zA-Z]
Letters and digits                      [[:alnum:]]
Letters                                 [[:alpha:]]
ASCII codes 0-127                       [[:ascii:]]
Space or tab only                       [[:blank:]]
Control characters                      [[:cntrl:]]
Decimal digits                          [[:digit:]]
Visible characters (not space)          [[:graph:]]
Lowercase letters                       [[:lower:]]
Visible characters                      [[:print:]]
Visible punctuation characters          [[:punct:]]
Whitespace                              [[:space:]]
Uppercase letters                       [[:upper:]]
Word characters                         [[:word:]]
Hexadecimal digits                      [[:xdigit:]]
Start of word                           [[:<:]]
End of word                             [[:>:]]
```

**Flags/Modifiers**

```
Multiline                               m
Case insensitive                        i
Ignore whitespace / verbose             x
Single line                             s
Unicode                                 u
eXtra                                   X
Ungreedy                                U
Anchor                                  A
Duplicate group names                   J
Non-capturing groups                    n
Ignore all whitespace / verbose         xx
```

**Group Constructs**

```
Non-capturing group                 (?:...)
Capturing group                     (...)
Atomic group (non-capturing)        (?>...)
Reset subpattern group number       (?|...)
Comment group                       (?#...)
Named capturing group               (?'name'...)
Named capturing group               (?<name>...)
Named capturing group               (?P<name>...)
Inline modifiers                    (?imsxUJnxx)
Localized inline modifiers          (?imsxUJnxx:...)
Conditional statement               (?(1)yes|no)
Conditional statement               (?(R)yes|no)
Recursive conditional statement     (?(R#)yes|no)
Conditional statement               (?(R&name)yes|no)
Lookahead conditional               (?(?=...)yes|no)
Lookbehind conditional              (?(?<=...)yes|no)
Recurse entire pattern              (?R)
Match expr. in capture group 1      (?1)
First relative capture group        (?+1)
Named capture group expression      (?&name)
Match subpattern 'name'             (?P=name)
Match expr. in group '{name}'       (?P>name)
Pre-define patterns before use      (?(DEFINE)...)
Positive lookahead                  (?=...)
Negative lookahead                  (?!...)
Positive lookbehind                 (?<=...)
Negative lookbehind                 (?<!...)
Alphabetic lookaround assertions    (*pla:...)
Non-atomic lookaround assertion     (*non_atomic_positive_lookahead:...)
Script run assertion                (*script_run:...)
Script run (shorthand)              (*sr:...)
Control verb                        (*ACCEPT)
Control verb                        (*FAIL)
Control verb                        (*MARK:NAME)
Control verb                        (*COMMIT)
Control verb                        (*PRUNE)
Control verb                        (*SKIP)
Control verb                        (*THEN)
```

# Screen selector

M-Commander supports running many internal modules (such as
editor, viewer and diff viewer) simultaneously and switching between
them without closing open files. Using several file managers at a time,
however, is not currently supported.

Let's call each of these modules a screen. There are three ways to
switch between screens, using one of these global shortcuts:

**Alt-}**
: switch to the next screen;

**Alt-{**
: switch to the previous screen;

**Alt-\`**
: open a dialog window with the list of currently open screens (or use the
"Screen list" menu item).

# Completion

Let M-Commander type for you.

Attempt to perform completion on the text before current position.  M-Commander
attempts completion treating the text as variable (if the text begins
with
**$**),
username (if the text begins with
**~**),
hostname (if the text begins with
**@**)
or command (if you are on the command line in the position where you
might type a command, possible completions then include shell reserved
words and shell built-in commands as well) in turn.  If none of these
matches, filename completion is attempted.

Filename, username, variable and hostname completion works on all input
lines, command completion is command line specific.  If the completion
is ambiguous (there are more different possibilities), M-Commander beeps and the
following action depends on the setting of the
[Complete: show all](#configuration)
option in the
[Configuration](#configuration)
dialog.  If it is enabled, a list of all possibilities pops up next to
the current position and you can select with the arrow keys and
**Enter**
the correct entry.  You can also type the first letters in which the
possibilities differ to move to a subset of all possibilities and
complete as much as possible.  If you press
**Alt-Tab**
again, only the subset will be shown in the listbox, otherwise the first
item which matches all the previous characters will be highlighted.  As
soon as there is no ambiguity, dialog disappears, but you can hide it by
canceling keys
**Esc**,
**F10**
and left and right arrow keys. If
[Complete: show all](#configuration)
is disabled, the dialog pops up only if you press
**Alt-Tab**
for the second time, for the first time M-Commander just beeps.

Apply escaping of **?**, **\***, and **&** symbols (as **\\?**, **\\\***,
and **\\&**) in filenames to disallow use them as metasymbols in regular
expressions when substitution is performed in the input line.

# Virtual File System

M-Commander is provided with a code layer to access the file
system; this code layer is known as the virtual file system switch.  The
virtual file system switch allows M-Commander to manipulate
files not located on the Unix file system.

Two virtual file systems are built into the program besides the
*local*
one, which is the regular Unix file system:
*extfs,*
which turns a file or a system-wide list into a directory tree with a script
of its own, and
*sfs,*
which passes a single file through a command and shows what comes out.
Everything that needs a connection to another machine, and the archives, are
[panel plugins](#panel-plugins)
now, not file systems of the switch.

The VFS switch code will interpret all of the path names used and will
forward them to the correct file system, the formats used for each one
of the file systems is described later in their own section.

## Panel plugins <a id="panel-plugins"></a>

A panel is not bound to a file system: a plugin can fill it with whatever it
can list. The plugins that come with the program are

```
arcmc        archives, and what is inside them
ftp, sftp    files on another machine
shell-link   files on another machine over ssh
samba        shares of an SMB server
s3           buckets of an S3 storage
git          the state of a repository
docker       containers, images and their logs
k8s          the objects of a cluster
mongo        the collections of a database
sqlite       the tables of a database
systemd      the units of the system
panelize     the result of a command as a panel
mcpeek       a look inside a file
mcstruct     a binary file as a tree of named fields
skineditor   the skin of the program
```

Every plugin carries its own help, which
**F1**
opens inside its panel or its dialog. The
**Manage plugins**
item of the Options menu lists what is loaded, switches a plugin off and opens
its settings. A plugin panel is reached from the
[Left and Right menus](#left-and-right-menus),
from the hotlist, or by typing the address of the plugin on the command line.

## EXTernal File System

**extfs**
allows you to integrate numerous features and file types into
M-Commander in an easy way, by writing scripts.

Extfs filesystems can be divided into two categories:

1\. Stand-alone filesystems, which are not associated with any existing
file.  They represent certain system-wide data as a directory tree.
You can invoke them by typing
*cd fsname://*
where fsname is an extfs short name (see below).  Examples of such
filesystems include audio (list audio tracks on the CD) or apt (list of
all Debian packages in the system).

For example, to list CD-Audio tracks on your CD-ROM drive, type

```
  cd audio://
```

2\. 'Archive' filesystems (like rpm, patchfs and more), which represent
contents of a file as a directory tree.  It can consist of 'real' files
compressed in an archive (urar, rpm) or virtual files, like messages
in a mailbox (mailfs) or parts of a patch (patchfs).  To access such
filesystems
*fsname://*
should be appended to the archive name.  Note that the archive itself
can be on another vfs.

For example, to list contents of a zip archive documents.zip type

```
  cd documents.zip/uzip://
```

In many aspects, you could treat extfs like any other directory.  For
instance, you can add it to the hotlist or change to it from directory
history.  An important limitation is that you cannot invoke shell
commands inside extfs, just like any other non-local VFS.

Common extfs scripts included with M-Commander are:

**a**
: access 'A:' DOS/Windows diskette
*(cd a://).*

**apt**
: front end to Debian's APT package management system
*(cd apt://).*

**audio**
: audio CD ripping and playing
*(cd audio://*
or
*cd device/audio://).*

**deb**
: package of Debian GNU/Linux distribution
*(cd file.deb/deb://).*

**dpkg**
: Debian GNU/Linux installed packages
*(cd deb://).*

**hp48**
: view and copy files to/from a HP48 calculator
*(cd hp48://).*

**lslR**
: browsing of lslR listings as found on many FTPs
*(cd filename/lslR://).*

**mailfs**
: mbox-style mailbox files support
*(cd mailbox/mailfs://).*

**patchfs**
: extfs to handle unified and context diffs
*(cd filename/patchfs://).*

**rpm**
: RPM package
*(cd filename/rpm://).*

**rpms**
: RPM database management
*(cd rpms://).*

**ulha, urar, uzip, uzoo, uar, uha**
: archivers
*(cd archive/xxxx://*
where xxxx is one of:
*ulha,*
*urar,*
*uzip,*
*uzoo,*
*uar,*
*uha).*

You could bind file type/extension to specified extfs as described in the
[Edit Extension File](#edit-extension-file)
section.  Here is an example entry for Debian packages:

```
  regex/\.deb$
          Open=%cd %p/deb://
```

## Single File fileSystem

**sfs**
passes one file through a command and shows the result as a file of its own,
which is how a compressed file is read without unpacking it by hand. The name
of the file system is appended to the name of the file, as with extfs:

```
  cd documents.gz/ugz://
```

The commands are listed in
**{{sysconfdir}}/mcommander/sfs.ini**,
one to a line: the name of the file system, a slash, the number of the
command, a tab, and the command itself, where
*%1*
is the file the panel is on and
*%3*
the file to write. The file that comes with the program holds the pairs that
pack and unpack gz, bz2, lz, lz4, lzma, lzo, xz and zst, and a few more.

# Colors

M-Commander will try to detect if your terminal supports
color using the terminal database and your terminal name.  Sometimes
it gets confused, so you may force color mode or disable color mode
using the -c and -b flag respectively.

If the program is compiled with the S-Lang screen manager instead of
ncurses, it will also check the variable
**COLORTERM,**
if it is set, it has the same effect as the -c flag.

You may specify terminals that always force color mode
by adding the
*color_terminals*
variable to the Colors section of the initialization file.  This will
prevent M-Commander from trying to detect if your terminal
supports color.  Example:

```
[Colors]
color_terminals=linux,xterm
color_terminals=terminal-name1,terminal-name2...
```

The program can be compiled with both ncurses and S-Lang, ncurses does
not provide a way to force color mode: ncurses uses just the
information in the terminal database.

# Skins

You can change the appearance of M-Commander.
To do this, you must specify a file that contain descriptions of colors
and lines to draw boxes. Redefining of the colors is entirely compatible
with the assignment of colors, as described in Section
[Colors](#colors).

If your skin contains any true-color definitions, you should define
the 'truecolors' key set to TRUE value in [skin] section. If true-color
is not used but 256-color is, you should define '256colors' instead.

A skin-file is searched on the following algorithm
(to the first one found):
```
1) command line option -S <skin>, --skin=<skin>
2) environment variable MC_SKIN
3) parameter skin of the [Midnight-Commander] section
4) file {{sysconfdir}}/mcommander/skins/default.ini
5) file {{pkgdatadir}}/skins/default.ini
```

Command line option, environment variable and parameter in config file may
contain the absolute path to the skin-file (with the extension .ini
or without it). Search of skin-file will occur in (to the first one found):

```
1) ~/.local/share/mc6/skins/
2) {{sysconfdir}}/mcommander/skins/
3) {{pkgdatadir}}/skins/
```

The format of skin files is described in
**{{pkgdatadir}}/skins/README.txt**.

# Filenames Highlight

Section [filehighlight] in current skin-file contains key names as
highlight groups and values as color pairs.

Rules of filenames highlight are placed in {{pkgdatadir}}/filehighlight.ini file
(~/.config/mc6/filehighlight.ini).
Name of section in this file must be equal to parameters names in
[filehighlight] section (in current skin-file).

Keys in these groups are:

*type*
: file type. If present, all other options are ignored.

*regexp*
: regular expression. If present, 'extensions' option is ignored.

*extensions*
: list of extensions of files. Separated by ';' sign.

*extensions_case*
: (make sense only with 'extensions' parameter) make 'extensions'
rule case sensitive (true) or not (false).

\`type' key may have values:

```
- FILE (all files)
  - FILE_EXE
- DIR (all directories)
  - LINK_DIR
- LINK (all links except stale link)
  - HARDLINK
  - SYMLINK
- STALE_LINK
- DEVICE (all device files)
  - DEVICE_BLOCK
  - DEVICE_CHAR
- SPECIAL (all special files)
  - SPECIAL_SOCKET
  - SPECIAL_FIFO
  - SPECIAL_DOOR
```

# Special Settings

Most of M-Commander settings can be changed from the
menus. However, there are a small number of settings which can only be
changed by editing the setup file.

These variables may be set in your ~/.config/mc6/ini file:

*clear_before_exec*
: By default, M-Commander clears the screen before executing a
command.  If you would prefer to see the output of the command at the
bottom of the screen, edit your ~/.config/mc6/ini file and change the value of
the field clear_before_exec to 0.

*confirm_view_dir*
: If you press F3 on a directory, normally M-Commander enters that directory.  If
this flag is set to 1, then M-Commander will ask for confirmation before changing
the directory if you have files tagged.

*vfs_timeout*
: The lifetime of the cache of a virtual file system, in seconds. After leaving
an archive or a compressed file, the listing that was read and the temporary
file that was unpacked are kept for that long, so that going back in is
immediate, and are released when the time is up. 60 by default; 0 releases them
at once.

*only_leading_plus_minus*
: Allow special treatment for '+', '-', '\*' in the command line (select,
unselect, reverse selection) only if the command line is empty.  You
don't need to quote those characters in the middle of the command line.
On the other hand, you cannot use them to change selection when the
command line is not empty.

*alternate_plus_minus*
: If true, use '+', '-', '\\' and '\*' keys normally. For select/unselect,
use 'Alt-+', 'Alt--' and 'Alt-\*'.

*show_output_starts_shell*
: When you use the C-o keystroke to go back to the user screen, if this
one is set, you will get a fresh shell.  Otherwise, pressing any key
will bring you back to M-Commander.

*timeformat_recent*
: Change the time format used to display dates less than 6 months from
now.
See strftime or date man page for the format specification. If this
option is absent, default timeformat is used.

*timeformat_old*
: Change the time format used to display  dates older than 6 months from
now or for dates in the future.
See strftime or date man page for the format specification. If this
option is absent, default timeformat is used.

*use_file_to_guess_type*
: If this variable is on (the default) it will spawn the file command to
match the file types listed on the
[extensions.ini file](#edit-extension-file).

*xtree_mode*
: If this variable is on (default is off) when you browse the file system
on a Tree panel, it will automatically reload the other panel with the
contents of the selected directory.

*shell_directory_timeout*
: This variable holds the lifetime of a directory cache entry in seconds. The
default value is 900 seconds.

*clipboard_store*
: This variable contains path (with options) to the external clipboard
utility like 'xclip' to read text into X selection from file.
For example:

<!-- -->

```
clipboard_store=xclip -i
```

*clipboard_paste*
: This variable contains path (with options) to the external clipboard
utility like 'xclip' to print the selection to standard out.
For example:

<!-- -->

```
clipboard_paste=xclip -o
```

*autodetect_codeset*
: This option allows use the \`enca' command to autodetect codeset of text files
in internal viewer and editor. List of valid values can be obtain by the
\`enca --list languages | cut -d : -f1' command. Option must be located
in the [Misc] section.

For example:

```
autodetect_codeset=russian
```

The settings of the internal file viewer are in the [Viewer] section of the
same file. They are all in the
[Viewer options](mview.md#viewer-options)
dialog as well; the names here are what that dialog writes.

*wrap*
: Wrap a line wider than the screen onto the next screen line. True by default.

*syntax*
: Color the text by the syntax rules of the editor. False by default.

*mouse_move_pages*
: Scroll with the mouse by pages rather than line by line. In ASCII mode the
left button selects text, so this scrolling uses the right or middle button
there. True by default.

*remember_file_position*
: Open a file at the place it was left the last time. False by default.

*structured_auto*
: Open supported files (json, yaml, yml, xml, html, htm) in the structured
(tree) mode right away. When a file fails to parse, the plain text view is
used silently. False by default.

*eof*
: The text printed after the last line of the file. Empty by default.

*structured_max_size*
: The largest file the structured (tree) view parses, in bytes. A larger one is
refused before it is read. 67108864 (64 MB) by default.

*structured_max_nodes*
: The largest tree the structured view builds, counted in nodes. A dense
document, such as XML of small tags, meets this limit before the size one: it
spends about a node per twelve bytes, and every node costs memory. 10000000 by
default, which holds some 120 MB of such a file in about 1.5 GB.

*dirt_limit*
: How many screen updates may be skipped at most while a file is being read.
Normally this value is not significant, because the code adjusts the number
of updates to skip according to the rate of incoming keystrokes. However, on
very slow machines, or terminals with a fast keyboard auto repeat, a big
value can make screen updates too jumpy. The default is 10, which behaves
best.

Older versions kept these settings in the main section under longer names
(wrap_mode, viewer_syntax_highlighting, mouse_move_pages_viewer,
mcview_remember_file_position, mcview_structured_auto, mcview_eof and
max_dirt_limit). They are read from there once and written back to the
[Viewer] section.

The settings of the terminal that runs the shell behind the panels are in
the [Terminal] section of the same file. No dialog writes them.

*search_direction*
: Which way
**Alt-s**
looks through the output of the shell, and which way
**Alt-Shift-s**
steps from one row of a filter to the next: "down" goes from the cursor
towards the newest row and past it round to the oldest, "up" goes towards
the oldest row and round to the newest, the way
**less**
searches. "down" by default.

# Parameters for external editor or viewer

M-Commander provides a way for specify an options for external editors
and viewers. M-Commander tries to search the
"[External editor or viewer parameters]" section in the system initialization file
(the defaults.ini file located in M-Commander's library directory)
and then in the ~/.config/mc6/ini file. The option name should be equal to the name
(full pathname) of external editor or viewer. The option value can contain following
variables:

*%filename*
: The filename to edit/view.

*%lineno*
: The start line in the opening file.

For example:

```
[External editor or viewer parameters]
    vi=%filename +%lineno
    joe=%filename +%lineno
    more=%filename +%lineno
```

Start line is passed to the external editor/viewer only if it is called from the
[Find file](#find-file)
results window.

If external editor/viewer is launched via F4/F3 keys, M-Commander hopes that program
(at least "joe", but probably others too) has an own feature that by default
opens the file where it was last open. M-Commander doesn't prevent external editor/viewer
to save and restore position in opened files.

# Terminal databases

M-Commander provides a way to fix your system terminal
database without requiring root privileges. M-Commander
searches in the system initialization file (the defaults.ini file located in
M-Commander's library directory) and in the
~/.config/mc6/ini file for the section
"terminal:your-terminal-name" and then for the section
"terminal:general", each line of the section contains a key symbol that
you want to define, followed by an equal sign and the definition for the
key.  You can use the special \\e form to represent the escape character
and the ^x to represent the control-x character.

The possible key symbols are:

```
f0 to f20     Function keys f0-f20
bs            backspace
home          home key
end           end key
up            up arrow key
down          down arrow key
left          left arrow key
right         right arrow key
pgdn          page down key
pgup          page up key
insert        the insert character
delete        the delete character
complete      to do completion
```

For example, to define the key insert to be the Escape + [ + O + p, you
set this in the ini file:

```
insert=\e[Op
```

Also now you can use
*extended learn keys.*
For example:

```
    ctrl-alt-right=\e[[1;6C
    ctrl-alt-left=\e[[1;6D
```

This means that ctrl+alt+left sends a \\e[[1;6D escape sequence
and therefore M-Commander interprets "\\e[[1;6D" as C-Alt-Left.

The
*complete*
key symbol represents the escape sequences used to invoke the completion
process, this is invoked with Alt-tab, but you can define other keys to do
the same work (on those keyboard with tons of nice and unused keys
everywhere).

<!-- help:break -->

# ENVIRONMENT

The variables below are the ones M-Commander reads, and the ones it sets for
the programs it starts. Variables such as **TERM**, **SHELL**, **HOME** or
**PATH** are not listed here: the program reads them to find out where it
runs, not to be configured by them.

## Read at start up

**MC_DATADIR**
: The directory the data files are taken from, in place of the one built in.
See [FILES](#files).

**MC_PROFILE_ROOT**
: The root of the user files, as an absolute path. See [FILES](#files).

**MC_SKIN**
: The skin to use, by name or by path. See [Skins](#skins).

**MC_KEYMAP**
: The keymap file to use. See [Keys](#keys).

**MC_TMPDIR**
: The directory for the temporary files of the program.

**MC_NO_LUA**
: Set to 1 to start without the Lua runtime. No Lua package is loaded, and
nothing that needs one is available.

**MC_SIXEL**
: Set to 0 to say the terminal has no sixel graphics, or to 1 to say it has.
Without the variable the terminal itself is asked.

**KEYBOARD_KEY_TIMEOUT_US**
: How long to wait for the rest of an escape sequence, in microseconds.

**COLORTERM**
: Read when the colours are chosen. See [Colors](#colors).

**CDPATH**
: The directories the internal cd command searches.

**EDITOR**, **VIEWER**, **PAGER**
: The external programs used when the built-in editor or viewer is turned
off. See
[Parameters for external editor or viewer](#parameters-for-external-editor-or-viewer).

## Set for the programs M-Commander starts

These are not meant to be set by hand. The program writes them so that a copy
of itself started from the built-in terminal can tell that it is already
running inside one.

**MC_SID**
: The session the program runs in. A copy started from that session opens no
panels of its own.

**MC_PID**
: The process id of the running program.

**MC_TTY**
: The terminal the program was started on.

## Debug logs

A log is written only when it is turned on, and the switch takes the value 1.
The plugin variables fall back to the general ones, so setting the general
pair alone logs everything.

**MC_LOG_ENABLE**, **MC_LOG_FILE**
: The general log. Without **MC_LOG_FILE** the file named by *logfile* in the
*[Logging]* section of the *ini* file is used, and without that entry
*mc.log* beside the other user files.

**MC_FTP_LOG_ENABLE**, **MC_FTP_LOG_FILE**
: The log of the ftp panel plugin. The file falls back to */tmp/mc-ftp.log*.

**MC_SMB_LOG_ENABLE**, **MC_SMB_LOG_FILE**
: The log of the samba panel plugin. The file falls back to
*/tmp/mc-samba.log*.

**MC_SPELL_LOG**
: The file the spell checker writes to. It has no switch of its own: the log
is written when the variable names a file.

To keep the log of a failing ftp connection:

```
MC_FTP_LOG_ENABLE=1 MC_FTP_LOG_FILE=/tmp/ftp.log mcommander
```

# FILES

Full paths below may vary between installations.  They are also affected
by the
**MC_DATADIR**
environment variable. If it's set, its value is used instead of
{{pkgdatadir}} in the paths below.

*{{pkgdatadir}}/help/mcommander.md*
: The help file for the program.

*{{pkgdatadir}}/extensions.ini*
: The default system-wide extensions file.

*~/.config/mc6/extensions.ini*
: User's own extension, view configuration and edit configuration
file.  They override the contents of the system wide files if present.

*{{sysconfdir}}/mcommander/mc.ini* *{{pkgdatadir}}/mc.ini*
: System-wide setup files for M-Commander, used only if the user
doesn't have his own
**~/.config/mc6/ini**
file. If {{sysconfdir}}/mcommander/mc.ini exists, {{pkgdatadir}}/mc.ini isn't used.

*{{pkgdatadir}}/defaults.ini*
: Global settings for M-Commander. Settings in this file
affect all users, whether they have ~/.config/mc6/ini or not.  Currently, only
[terminal settings](#terminal-databases)
are loaded from defaults.ini.

*~/.config/mc6/ini*
: User's own setup. If this file is present then the setup is loaded
from here instead of the system-wide startup file.

*{{pkgdatadir}}/hints/hint*
: This file contains the hints displayed by the program.

*~/.config/mc6/menu*
: User's own application menu. If this file is present it is used instead
of the system-wide applications menu.

*~/.config/mc6/menu.ini*
: The user menu that edits itself, one group per entry. Where this file
exists, it is the menu F2 opens, and a .mc6menu of the current directory
is shown along with it.

*~/.cache/mc6/Tree*
: The directory list for the directory tree and tree view features.

*./.usermenu*
: Local user-defined menu. If this file is present in the current
directory, it is used instead of the home or system-wide applications
menu.

To change default root directory of M-Commander, you can use
**MC_PROFILE_ROOT**
environment variable. The value of MC_PROFILE_ROOT must be an absolute path.
If MC_PROFILE_ROOT is unset or empty, HOME variable is used. If HOME is unset
or empty, M-Commander directories are get from GLib library.

# LICENSE <!-- help:skip -->

This program is distributed under the terms of the GNU General Public
License as published by the Free Software Foundation. See the built-in
help for details on the License and the lack of warranty.

# AVAILABILITY

The latest version of this program can be found at
<https://github.com/blue-panels/mcommander/releases> .

# SEE ALSO

ed(1), gpm(1), terminfo(1), view(1), sh(1), bash(1),
tcsh(1), zsh(1).

```
M-Commander's page on the World Wide Web:
	https://github.com/blue-panels/mcommander
```

# AUTHORS

Authors and contributors are listed in the AUTHORS file in the source
distribution.

# BUGS

If you want to report a problem with the program, please create bugreport at
<https://github.com/blue-panels/mcommander/issues> .

Provide a detailed description of the bug, the version of the program
you are running
*(mcommander -V*
displays this information), the operating system you are running the
program on.  If the program crashes, we would appreciate a stack trace.
