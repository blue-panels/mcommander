# Changelog

The releases of this fork, newest first.

## 6.2.0 - 2026-11-15

New panel plugins for PostgreSQL, GitHub and duplicate files, the full kitty
keyboard protocol in the panels and in the built-in terminal, three-way merge in
the compare view, an optional Far mode, and many fixes of the terminal and the
editor.

### Features

#### Panel plugins

##### duplicates

- duplicates: Duplicate file search; the plugin also finds similar pictures (JPEG and PNG, by a perceptual hash) and texts ([#463](https://github.com/blue-panels/mcommander/pull/463))

##### postgres

- postgres: A new panel plugin for servers, databases, schemas and tables; Alt-F3 opens a query console for the database (bluerig or psql) ([#438](https://github.com/blue-panels/mcommander/pull/438))
- postgres: F3 on a table shows its SQL and its first rows as a table ([#444](https://github.com/blue-panels/mcommander/pull/444))

##### gh

- gh: Add a panel plugin for GitHub pull requests and issues ([#434](https://github.com/blue-panels/mcommander/pull/434))

##### mongo

- mongo: Alt-F3 opens a query console for the database (bluerig or mongosh) ([#435](https://github.com/blue-panels/mcommander/pull/435))
- mongo: F3 on a collection shows its documents as a table ([#440](https://github.com/blue-panels/mcommander/pull/440), [#443](https://github.com/blue-panels/mcommander/pull/443))

##### sqlite

- sqlite: Show a table, a view or a page of rows as a table on F3 ([#441](https://github.com/blue-panels/mcommander/pull/441))

##### git

- git: Run the bluerig editor from the Git panel ([#407](https://github.com/blue-panels/mcommander/pull/407))
- git: Show merge conflicts and merge them on Shift-F3 ([#421](https://github.com/blue-panels/mcommander/pull/421))
- git: Show all the changes of a branch in the diff folder at the top of the commit list, and the description of a commit in Quick view ([#425](https://github.com/blue-panels/mcommander/pull/425))

##### acl

- acl: Add an access control list editor for POSIX and Windows ACLs ([#409](https://github.com/blue-panels/mcommander/pull/409))

##### sftp, shell-link

- sftp: Support default keys and keyboard-interactive authentication ([#412](https://github.com/blue-panels/mcommander/pull/412))
- shell-link: Report remote write and unlink errors ([#411](https://github.com/blue-panels/mcommander/pull/411))

##### All panel plugins

- plugins: Panel plugins are loaded the first time they are used, which lowers the use of memory and other resources ([#462](https://github.com/blue-panels/mcommander/pull/462))
- plugins: The bluerig tool can be run from the panel plugins for git, MongoDB and PostgreSQL ([#467](https://github.com/blue-panels/mcommander/pull/467))
- filemanager: Recode the names of a plugin panel with Alt-e ([#442](https://github.com/blue-panels/mcommander/pull/442))

#### Built-in terminal (mcterm)

- tty: Support the full kitty keyboard protocol in the panels, and Ctrl with a digit ([#346](https://github.com/blue-panels/mcommander/pull/346), [#383](https://github.com/blue-panels/mcommander/pull/383), [#398](https://github.com/blue-panels/mcommander/pull/398))
- tty: While Shift, Ctrl or Alt is held, the key bar shows the commands of the F keys with that modifier; needs a terminal with the kitty keyboard protocol and the keybar_modifiers option in the settings file ([#398](https://github.com/blue-panels/mcommander/pull/398))
- mcterm: Support the full kitty keyboard protocol for programs in mcterm ([#399](https://github.com/blue-panels/mcommander/pull/399))
- mcterm: Show wide characters and more SGR attributes ([#354](https://github.com/blue-panels/mcommander/pull/354))
- mcterm: Pass the OSC 52 clipboard text of a program to the clipboard ([#378](https://github.com/blue-panels/mcommander/pull/378))
- session: mc can survive a closed terminal or a dropped connection: with the session plugin, the next start of mc returns to the same session; the plugin is turned on in Manage plugins ([#423](https://github.com/blue-panels/mcommander/pull/423))

#### Editor

- editor: Take a bracketed paste as one block ([#397](https://github.com/blue-panels/mcommander/pull/397))
- editor: Ctrl-A now selects all the text in the editor ([#416](https://github.com/blue-panels/mcommander/pull/416))
- widget: A new file chooser dialog ([#419](https://github.com/blue-panels/mcommander/pull/419))

#### Syntax highlighting

- syntax: One syntax can be embedded in another: scripts and style sheets in HTML are highlighted as JavaScript and CSS, and code blocks in markdown by the language they name; rule sets can be layered, as in Smarty or Jinja2 templates ([#404](https://github.com/blue-panels/mcommander/pull/404))
- syntax: Add highlighting for Jinja2, Mermaid, Smarty and git commit messages, and for awk, python and SQL code in shell scripts ([#405](https://github.com/blue-panels/mcommander/pull/405))

#### Viewer

- viewer: Searching for plain text or hex in large files is many times faster ([#458](https://github.com/blue-panels/mcommander/pull/458))

#### Compare view

- diffviewer: Compare files with git's xdiff instead of running diff ([#413](https://github.com/blue-panels/mcommander/pull/413))
- diffviewer: Merge three files in the compare view ([#415](https://github.com/blue-panels/mcommander/pull/415))
- diffviewer: Pick the base of a merge, show the active file and the copy direction ([#422](https://github.com/blue-panels/mcommander/pull/422))
- packaging: The compare program is renamed to mcdiff6 because of a name clash ([#343](https://github.com/blue-panels/mcommander/pull/343))
- keymap: Compare files is now on Ctrl-X f; it used to be Ctrl-X Ctrl-d ([#416](https://github.com/blue-panels/mcommander/pull/416))

#### Interface and core

- filemanager: An optional Far mode with the keys of Far Manager and its fast find on Alt with a character, turned on by separate options ([#403](https://github.com/blue-panels/mcommander/pull/403), [#437](https://github.com/blue-panels/mcommander/pull/437))
- tty: Take the keys of the Win32 input mode of Windows Terminal ([#401](https://github.com/blue-panels/mcommander/pull/401))
- widget: Draw the scrollbars of lists and tables as the skin sets them ([#432](https://github.com/blue-panels/mcommander/pull/432))
- widget: Scroll a drop-down menu taller than the screen ([#436](https://github.com/blue-panels/mcommander/pull/436))
- widget: Add a cursor on the check boxes of a table row ([#408](https://github.com/blue-panels/mcommander/pull/408))
- clipboard: Copy to the clipboard of the terminal by OSC 52 ([#430](https://github.com/blue-panels/mcommander/pull/430))
- tty: Repaint sixel pictures after screen refreshes ([#384](https://github.com/blue-panels/mcommander/pull/384))
- usermenu: Add conditions and a menu file to edit by hand ([#388](https://github.com/blue-panels/mcommander/pull/388))

### Fixes

#### Editor

- editor: Fix a crash after a ctags jump: the highlighting read the freed window ([#466](https://github.com/blue-panels/mcommander/pull/466))
- editor: Fix the memory errors valgrind found: a character read past the end of a buffer chunk, and three more ([#469](https://github.com/blue-panels/mcommander/pull/469))
- editor: Undo and redo a UTF-8 character as a whole ([#406](https://github.com/blue-panels/mcommander/pull/406))
- editor: The editor shows its text at start, not just the title and the buttons until the first key press ([#445](https://github.com/blue-panels/mcommander/pull/445))
- tty, editor: An editor window moved off the screen no longer loads the CPU, and "Press any key to continue" waits for a real key press ([#447](https://github.com/blue-panels/mcommander/pull/447))
- ctags: Fix the leak of pointer buffers in tag lookups ([#459](https://github.com/blue-panels/mcommander/pull/459))

#### Viewer and quick view

- filemanager: Quick view shows the file as soon as the panel switches to it, not after the cursor moves ([#464](https://github.com/blue-panels/mcommander/pull/464))
- filemanager: F3 and Quick view show pictures not only for local files but in plugin panels too ([#465](https://github.com/blue-panels/mcommander/pull/465))
- filemanager: A file is no longer downloaded twice when viewed (F3 and Quick view) in panel plugins ([#439](https://github.com/blue-panels/mcommander/pull/439))
- viewer: Give a source its own display mode back when the view is done ([#424](https://github.com/blue-panels/mcommander/pull/424))
- viewer: Show the bytes in hex on a line of their own ([#400](https://github.com/blue-panels/mcommander/pull/400))
- lua-markdown: Keep table rows whole when a cell holds a line break tag ([#380](https://github.com/blue-panels/mcommander/pull/380))
- lua-markdown: Keep long lines of a code block inside its frame ([#381](https://github.com/blue-panels/mcommander/pull/381))
- lua-markdown: Show plain text as text, not as code blocks ([#446](https://github.com/blue-panels/mcommander/pull/446))

#### Built-in terminal (mcterm)

- mcterm: Answer DSR, the cursor position and the status report ([#342](https://github.com/blue-panels/mcommander/pull/342))
- mcterm: Start bash with its own startup file and keep terminal answers out of the keyboard ([#356](https://github.com/blue-panels/mcommander/pull/356))
- mcterm: Pass a terminal paste to the shell as one block ([#379](https://github.com/blue-panels/mcommander/pull/379))
- mcterm: Wait for the shell echo only after a whole UTF-8 character ([#382](https://github.com/blue-panels/mcommander/pull/382))
- mcterm: Do not show a line break right after Enter ([#387](https://github.com/blue-panels/mcommander/pull/387))
- mcterm: Send Home and End to the shell the way xterm does ([#390](https://github.com/blue-panels/mcommander/pull/390))
- mcterm: Make the scroll and tag keys work with the embedded terminal ([#391](https://github.com/blue-panels/mcommander/pull/391))
- mcterm: Send Shift-Tab to the program in the terminal ([#393](https://github.com/blue-panels/mcommander/pull/393))
- mcterm: Quit when the shell behind the panels ends ([#394](https://github.com/blue-panels/mcommander/pull/394))
- mcterm: Skip APC, PM and SOS strings instead of drawing them ([#431](https://github.com/blue-panels/mcommander/pull/431))
- mcterm: Do not read the right prompt as typed text ([#456](https://github.com/blue-panels/mcommander/pull/456))

#### Panel plugins

- mongo: The last bucket keeps the last document of its range ([#448](https://github.com/blue-panels/mcommander/pull/448), [#450](https://github.com/blue-panels/mcommander/pull/450))
- arcmc: Archives with hundreds of thousands of files open many times faster ([#451](https://github.com/blue-panels/mcommander/pull/451))
- ftp, sftp: Make a directory with F7 inside a connection ([#452](https://github.com/blue-panels/mcommander/pull/452))
- shell-link: Make the missing parent directories on F7 ([#453](https://github.com/blue-panels/mcommander/pull/453))
- git: Fixed a crash on Enter on a file in the git panel with Quick view in the other panel ([#427](https://github.com/blue-panels/mcommander/pull/427))

#### Interface and core

- mouse: Stop taking a turn of the wheel for a press of a button ([#433](https://github.com/blue-panels/mcommander/pull/433))
- execute: Keep the alternate screen for an external editor or viewer ([#428](https://github.com/blue-panels/mcommander/pull/428))
- tty: Prefer the Win32 input mode to the kitty protocol ([#402](https://github.com/blue-panels/mcommander/pull/402))
- tty: Read a paste that comes as keys of the Win32 input mode ([#426](https://github.com/blue-panels/mcommander/pull/426))
- diffviewer: Comparing with case ignored works for Cyrillic even when the file has an invalid UTF-8 byte ([#420](https://github.com/blue-panels/mcommander/pull/420))
- usermenu: A program run from the user menu without the panel shell gets the keyboard, and the panels come back in the right mode after it ends ([#410](https://github.com/blue-panels/mcommander/pull/410))
- filemanager: The title of the terminal window shows which command runs in the built-in terminal ([#396](https://github.com/blue-panels/mcommander/pull/396))

## 6.1.0 - 2026-09-26

- The program is now M-Commander: the main binary is mcommander, the editor,
  viewer and diff viewer are mcedit6, mview and mdiff, and the packages are
  mcommander, mcommander-plugins and mcommander-lua, so it installs beside
  GNU Midnight Commander. User settings stay where they were.
- Packaging recipes for Debian are ready: the package builds clean with
  lintian, carries autopkgtest tests and follows the releases through
  debian/watch.
- The viewer renders Markdown: headings, lists, tables, links, footnotes and
  code blocks with the editor's syntax colors; a large file shows its first
  screen at once and the rest follows.
- Mermaid flowcharts, sequence and class diagrams and display math are drawn
  in the terminal.
- The viewer has a reading cursor; text can be selected with the keyboard or
  the mouse and copied to the clipboard.
- Syntax highlighting works in the viewer and in the diff viewer, with the
  same rules as the editor.
- lua-pdf shows PDF pages as searchable text with the pictures, drawn with
  sixel where the terminal has it and with Chafa elsewhere.
- The nroff mode understands SGR colors, OSC 8 links and overstrike glyphs,
  and follows the charset of the locale.
- Viewer settings have their own dialog in the Options menu.
- F6 in the structured view keeps only the nodes whose key or value matches.
- The editor can show control characters in caret notation or hide them.
- The editor clipboard remembers the codeset, and pasting into a file with
  another codeset recodes the text.
- In the terminal, the shell input line stays on the command line row, and
  Ctrl-Insert and Shift-Insert work whoever owns the command line.
- Alt-S searches the terminal output and Alt+Shift+S filters it, with the
  pattern typed in place.
- With the panels hidden, F1, F7 and Shift-F4 work in the terminal, and F8 no
  longer deletes the file under the hidden panel.
- The Hotlist keeps plugin locations such as Git, Docker, Kubernetes and
  archives, and its dialog can be searched.
- The User Menu can be edited in place: entries and submenus can be added,
  changed, moved and deleted.
- Local panels follow changes on disk through inotify; the Watch directories
  option turns it on.
- Marking many files at once takes a fraction of a second.
- File attributes have their own Attributes menu, and the files to edit by
  hand are in the Options menu.
- Help and manual pages are written in Markdown, each program has its own help
  file, and mctree and mcstruct have manual pages.
- The new mcpeek plugin opens .NET assemblies as trees and shows decompiled C#
  or IL.
- The skin editor changes a skin inside the program; all skins cover the
  terminal, mctree and mcstruct, and Mashdark and Tokyo Night are new.
- Lua scripts can use the syntax engine through mc.syntax.scan() and add a
  settings dialog through mc.settings().
- Ctrl-Space in an archive shows the size of a directory.
- DBF files show as tables in the Quick View.
- The viewer no longer hangs on a PDF page with a picture when the window is
  resized.
- SFTP keeps the error window open and says why the connection failed.
- Shell links send current helper scripts to a host that has none installed.

## 6.0.4 - 2026-09-01

- In line drawing mode the editor widens a table: Space on a frame character
  inserts a cell in the same column of every row, and Enter after the last
  frame character adds a row.
- Lua packages can read and change the editor's typing mode through
  editor:overwrite() and editor:set_overwrite().
- The terminal draws the DEC line drawing set an application switches to, so
  the frames of programs such as tig come out as lines and not as letters.
- Ctrl+Alt+L, or Ctrl+Shift+L where the terminal tells the two apart, clears
  the terminal screen and its scrollback and leaves the prompt where it is.
- Alt+Shift+S puts a line filter on at once, with the pattern taken from a one
  line selection or from the word under the cursor; it works in the editor and
  in the terminal, and Alt-S lifts it.
- The editor's search dialog opens with the selected text as the search
  string, still editable before the search starts.
- The terminal wraps long lines and reflows the scrollback when the width
  changes, so the tail of a long line is no longer lost.
- lua-dbf opens dBase, FoxPro and Clipper tables as pageable tables, with the
  structure, the record card, a choice of encoding and F8 for the raw file.
- mc.ui.screen lets a Lua script build a full screen of native widgets -
  tables, text, inputs, checkboxes - and drive it from events.
- The editor's Search dialog gains a Filter action that hides every line that
  does not match, keeping the original line numbers; Alt-S lifts the filter or
  puts the last one back on.
- mcstruct shows a binary file as a tree of named fields described by STL5
  definitions, with a hex strip, editing in place and definitions for many
  common formats.
- The new permission_colors option colours each permission bit by what it is,
  the way eza does, while the file type keeps the row colour.
- The image viewers show the picture first with one line about it; i switches
  to the full properties and F1 opens the handler's own help.
- Configuration, data and cache move to ~/.config/mc6, ~/.local/share/mc6 and
  ~/.cache/mc6; on first start the old mc directories are copied and the
  originals are left untouched.
- Sixel pictures work in the terminal and in the viewer, and the lua-sixel
  handler draws images with Chafa where the terminal supports it.
- lua-chafa shows a short description of an image before the picture and stops
  at one frame of an animated image.
- Terminal output can be marked with the mouse or with Shift and the arrows
  and copied with Ctrl-Insert; Ctrl-L scrolls the visible screen into the
  history.
- Alt-C asks for the directory in the panel's mini status instead of a dialog,
  with directory completion above the input and history on Up and Down.
- At the shell prompt the shell's own line editor owns the command line, so
  editing, history and completion work as they do in a terminal.
- A Lua handler runs readelf and shows the headers, sections, segments and
  dynamic data of an ELF file; F8 switches to the raw file.
- Shift-Tab unindents a block in the editor even when the ctags plugin holds
  the key for CtagsComplete.
- A generated view keeps its source while the viewer is open, and F8 switches
  between the generated data and the file itself.
- Alt+Shift+S filters the panel as the pattern is typed; Quick Search and
  Quick Filter share the pattern, and Ctrl-G removes the filter.
- A Lua handler can take over opening a file in the viewer or in Quick View;
  the Chafa handler uses this to show images.
- While a command runs, the prompt shows that command instead of the word
  background.
- The build asks for GLib 2.58, the version the code already needs.
- Lua scripting: a typed host ABI opens the editor, the panels, the viewer,
  the UI, processes and events to scripts, which are managed from Options.
- The embedded terminal replaces the old subshell: commands go to the shell
  already running there, and the command line shows that shell's real prompt.
- Terminal output can be selected and copied out, the terminal has a keymap
  and a skin section of its own, and the focus moves between it and the
  command line.
- The list of found files shows time, size and permissions, and groups the
  matches of a file under an entry that opens to the lines themselves.
- The terminal keeps a bounded scrollback: PgUp/PgDn, Shift and the arrows,
  Home/End and the wheel browse it, and typing returns to the live output.
- A panel plugin can hand another plugin an open seekable stream, and
  magic.ini routes a file to a plugin, so an archive on a remote host opens
  without downloading it whole.
- A read only SQLite browser opens a database as a panel of tables, columns
  and rows.
- Shift-F4 makes a new connection from the top listing of a plugin panel.
- The drive menu of each panel is in the Panel menu, with its shortcut.
- In the editor a space widens a table only while Insert is on; in overwrite
  mode it rubs the frame character out, as before.
- The gap before a column block keeps the editor's background, and copying a
  column block no longer loses bytes of multibyte characters.
- In an 8-bit locale the characters of the codepage are drawn as letters and
  not as dots, so CP866 Cyrillic comes out right in the editor and in the
  viewer.
- The mongo plugin builds with mongo-c-driver 1.24 again.
- The sqlite plugin no longer leaks the base name every time the title is
  rebuilt.
- Nothing zsh draws is lost any more: the terminal keeps the search prompt,
  the completion list and the tail of a long line.
- Shift-F3 diffs a file inside a commit and from the status list of the git
  panel.
- Switching between the panels and the terminal repaints the screen, so the
  panels no longer come back with holes in them.
- Ctrl+F1 and Ctrl+F2 toggle the panels while the focus is in the command
  line.
- A command run from the panels brings them back the way "Pause after run"
  asks, and the shell follows the current panel.
- The teaching examples are no longer installed among the loaded scripts, so
  the editor does not run them on every save.
- The terminal handles ICH and insert mode, so typing in the middle of a shell
  command line no longer overwrites what is there.
- Enter after Ctrl-O works on a directory again, and F9 opens the menu with
  the panels hidden.
- arcmc reads external archive formats from arcmc.ini, so a new format needs
  no rebuild, and Ctrl+PgDn opens such an archive.
- The active panel stays the active one when the terminal overlay opens.
- Entering a subdirectory of a plugin panel lands on "..", not on the first
  entry.
- sftp and shell-link ask for libssh2 1.9 and say why a connection failed.
- The build works with GLib 2.58 again, the version configure asks for.
- shell-link shows the owner and the group of remote files.
- Characters of an 8-bit codepage are drawn correctly on an 8-bit terminal.
- In a UTF-8 terminal the characters converted from CP1251 or KOI8-R are no
  longer shown as dots.
- The cursor and the state of the command line stay in step with the shell.
- Copying a directory out of a plugin panel copies what is in it.
- arcmc copies the contents of a directory taken out of an archive.
- The Esc key modes work as they are configured, and input no longer shows one
  key late.
- The mongo plugin is packaged for Ubuntu 26.04.
- arcmc opens password protected 7z archives.
- The Serbian man pages follow the build options, so the installed pages match
  the features built in.
- A long path in an extension cd no longer overflows the buffer.
- Full screen mcterm has a function key bar of its own, and the manual says
  what the scrollback is and which keys go to mc and which to the program in
  the terminal.
- The editor line filter feeds lines to the search engine through the normal
  buffer callback, shows progress and can be cancelled.

## 6.0.3 - 2026-08-02

- The built-in tar and cpio filesystems are gone. Archives are opened by the
  arcmc panel plugin, which comes in the mc6-plugins package.
- The Fedora package could not be installed: it asked for interpreter paths
  that no package owns. Packages are now installed in a clean image before a
  release is published.
- Spell checking finds aspell on a system that has only the library, without
  the development package.
- The embedded terminal answers the queries a shell sends, so fish no longer
  waits ten seconds and leaves the query on the screen.
- mc starts when the login shell is a plain sh, as it is on FreeBSD.
- Non-ASCII characters typed in the editor appear at once, not after the next
  keypress.
- The spell engine can be switched between aspell and hunspell in the plugin
  settings.
- Panel plugins can show a Quick View preview in the other panel.
- Spell checking is built in and needs no aspell headers to compile.
- The archive plugin is built where libarchive ships no pkg-config file.

## 6.0.2 - 2026-07-31

- Copying, overwriting and resuming now work between plugin panels.
- The HTML provider has been removed.
- Warnings fixed by using long for editor column instead of off_t
- Commands run before the first Ctrl-O no longer lose their console output.
- Large pastes no longer freeze the UI; screen repaints are limited while input is arriving.
- The arcmc 'Create archive' hotkey is now reassignable via arcmc.ini.
- The legacy built-in tarfs and cpiofs VFS modules have been removed; arcmc handles archive browsing when available.

## 6.0.1 - 2026-07-20

- Panel contents can come from a dynamically loaded plugin instead of the
  local filesystem or the built-in VFS. Twelve plugins ship on top of it.
- The same idea applies to mcedit: an editor plugin registers with the editor
  and adds commands of its own.
- Plugins load from the user directory as well as the system one.
- A plugin can open its own help file and its own settings dialog.
- Alt-F1 and Alt-F2 list the plugins that ask to appear there.
- Directory history works across plugin panels the way it works on a local
  one.
- Files can be copied into a plugin panel through the put_file and save_file
  callbacks.
- A plugin can re-drive the viewer's data source.
- arcmc browses, creates, packs and extracts archives through libarchive, and
  treats .jar, .war and .ear as archives.
- A git panel lists commits, branches and changed files, and keeps favorites.
- A docker panel groups containers by compose project and shows image sizes;
  Alt-s configures what the container log shows.
- A kubernetes panel backed by kubectl covers pods, nodes, services and more.
- A MongoDB browser covers connections, databases, collections and documents.
- An S3 panel covers connections, buckets and objects over libcurl, Cloudflare
  R2 among the providers.
- An FTP and FTPS plugin on libcurl replaces the old VFS module, with saved
  connections.
- An SFTP plugin manages connections, compressed sessions and public-key
  authentication.
- A Samba plugin browses SMB shares.
- A shell-link plugin keeps saved local and SSH connection profiles, reachable
  from the panel menus.
- External Panelize is a plugin now instead of code in the core.
- A ctags navigator for mcedit jumps to definitions and declarations.
- Fold a block of code by bracket scope from the gutter, the Command menu or
  Alt+Shift+F. A folded line keeps its text and gains a summary with the
  matching bracket and the line count, and the indicator comes from the skin.
- Alt+Shift+U opens the undo history: undoable and redoable groups with line,
  action, character count and a preview, and a jump to any point in it.
- Alt+Shift+M opens the macro explorer, which lists every recorded macro with
  its hotkey, shows the action sequence, runs it, deletes it with
  confirmation, or opens mc.macros at its definition.
- Saving a file you have no write permission for asks for a password and
  retries through sudo, instead of failing with EACCES and leaving Save As as
  the only way out.
- A file with no syntax definition is no longer flat: numbers, quoted strings
  and a configurable set of symbols are coloured line by line.
- A per-line layout cache keeps cursor movement, marking and redraw from
  rescanning the line, so logs, CSV and minified files become usable.
- Double click starts selection by words and dragging extends it by words;
  triple click does the same by lines.
- The viewer shows JSON, YAML, XML and HTML as a tree.
- A grep-style filter hides non-matching lines over the same data source, with
  the full set of search options and a dialog that tests a pattern before it
  is applied.
- The viewer replays escape-coded files as a virtual screen, covering
  scrolling regions, absolute vertical moves and the rest of the wider
  terminal emulation.
- Shift+F9 turns ANSI colour on and off in the viewer.
- Output of a command that never ends can be viewed: the stream is read
  without blocking.
- Markdown files are formatted in the viewer rather than shown raw.
- An optional terminal widget runs an interactive shell inside the file
  manager.
- Alt-t opens a list of named listing formats that can be edited and saved.
- A diff opened from a panel goes to the internal diff viewer instead of the
  external mcdiff.
- Key Bindings, Learn keys and Key Sniffer dialogs, and Learn keys understands
  Ctrl, Alt and Shift.
- Typing in a history popup narrows the list.
- Copy and paste works between input lines and the editor.
- Procyon is tried first when a .class file is viewed.
- A column block is a real rectangle: the highlight no longer stops at the end
  of each line.
- Pasting the output of an external command works in the TUI.
- A file whose first bytes look like an archive no longer crashes the viewer.
- The viewer filter no longer stalls on a huge line, and horizontal scrolling
  on such a line is no longer slow in unwrap mode.
- The cursor is restored when an application leaves the alternate screen, and
  a log with two alternate-screen brackets no longer comes out blank.
- Ctrl-F1 and Ctrl-F2 work when the panel shows Quick View instead of a
  listing.
- The panel gets its column format back after a plugin panel closes.
- A temporary copy made by a plugin keeps the file extension.
- The git panel listed the wrong range of commits.
- The samba plugin could not get back to the connection list.
- Esc and F10 did not close the viewer and the diff viewer.
- The old ftpfs and sftpfs VFS modules are gone, superseded by the FTP and
  SFTP panel plugins.
- WTable is model-backed: the widget no longer owns its rows.
- One hotkey parser serves the sftp, ftp, git, s3 and shell-link plugins
  instead of five.
- The docker plugin is split into domain modules, with test coverage.
- The spell plugin logs where it is told to, not to a hard-coded path in /tmp.
- libsmbclient is found with pkg-config, where the header check missed it.

## Before the fork

M-Commander is based on GNU Midnight Commander 4.8.33. The upstream NEWS up to
that release is kept in doc/NEWS.
