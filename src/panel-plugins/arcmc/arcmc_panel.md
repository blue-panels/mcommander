# arcmc <!-- help:notitle -->

**arcmc archive plugin**

Opens an archive in a panel, creates one, extracts and tests one. libarchive
reads and writes the formats it knows; for the rest the plugin calls the
archiver of the system or an extfs helper, and both can be chosen per format.

RPM packages open directly into their payload through libarchive, including
source packages (`.src.rpm` and `.spm`). Their files can be viewed and copied
out; the plugin does not change or create RPM packages.

**Opening an archive**

**Enter**
: On an archive in a panel, opens it in the plugin. What is inside is listed
like ordinary files, and the panel walks the directories of the archive.

The Left and Right menus reach the plugin through
**Archives**,
and its actions are in the File menu:

```
Open archive        open the archive under the cursor
Create archive      pack what is marked
Extract archive(s)  unpack the marked archives
Test archive(s)     check them without unpacking
Archiver settings   the formats and the tools
```

The Command menu carries
**Create archive**
as well, so packing needs no trip to the File menu.

**Inside an archive**

**F3, F4**
: View and edit a file of the archive. It is unpacked into a temporary file
first; the editor writes it back where the format allows it.

**F5, F6**
: Copy and move between the archive and the other panel, in both directions.

**F7, F8**
: Create a directory in the archive and delete what is marked, where the
format allows it.

**Ctrl-Space**
: The size of the directory under the cursor, counted from the entries of the
archive rather than from the disk.

**Creating an archive**

The dialog takes the name of the archive and:

*Format*
: The formats the plugin can write. The button beside the list opens the whole
list, the external archivers included.

*Compression*
: Store, Fastest, Normal or Maximum. What each one means is up to the format.

*Encryption*
: The password, typed twice, with
**Encrypt files**
and
**Encrypt headers**
as the format allows; headers hide the names of the files as well.
**Show password**
prints what is typed instead of stars. For 7z the encryption is done by the
external tool, so it has to be installed.

*Store relative paths*
: Keep the paths as they are in the panel instead of the names alone.

*Delete files after archiving*
: Remove what was packed once the archive is written.

What is marked in the panel goes into the archive; with nothing marked, the
file under the cursor does.

**The settings dialog**

**Archiver settings**
of the File menu, and the settings of the plugin in the Manage plugins dialog,
open the same window. It holds two tables.

*Builtin (libarchive)*
: The formats libarchive handles. Every row says the format, the extension,
how packing and unpacking are done, which tool is used and whether the format
is on.

*External archivers*
: The formats an outside program handles. The row shows whether that program
was found;
**!**
marks the one that is missing from the PATH.

**Space**
: Step the backend of the row: builtin, both, external, off.
*Both*
means libarchive where it can and the external tool where it cannot; for 7z it
applies to encrypted archives only.
*External*
always calls the tool.

**F4**
: The parameters of the tool of that row: the pack and unpack binary with their
arguments, the test binary and its arguments, the argument that passes a file
list, and the extfs helper used to list the archive. An empty field clears a
value and takes the operation away.

**Ok**
: Write the changes to the ini file.

**Hotkeys**

**Shift-F1**
: Create an archive from the panel, without the menu. The key is settable as
*hotkey_create*
of the
**[arcmc]**
section, and
**none**
switches it off:

```
[arcmc]
hotkey_create=shift-f1
```

**The ini file**

The settings live in
*~/.config/mc6/arcmc.ini*.
The section
**[arcmc-builtin]**
holds what the first table shows, one entry per format, and the sections of an
external format hold what F4 edits:

```
[arcmc-ext]
FOO=true

[arcmc-ext-params-FOO]
extension=.foo
pack_bin=foo-archive
pack_args=create
unpack_bin=foo-archive
unpack_args=extract
test_bin=foo-archive
test_args=test
extfs_helper=ufoo
list_file_arg=@%s
```

A name that the plugin does not know needs
*extension;*
the leading dot is optional, matching ignores case, and the longest matching
suffix wins. A missing command key takes the operation away, an empty value
clears a default. The commands are built as

```
pack_bin pack_args archive files...
unpack_bin unpack_args archive [destination]
test_bin test_args archive
```

and a selective extraction runs the unpack command in the destination
directory with the names of the files.

*list_file_arg*
is a printf template for a file-list argument, `@%s` for example, used when a
command line would otherwise pass 128 KiB of names.

*extfs_helper*
names an executable of the extfs protocol, looked for in
*~/.local/share/mc6/extfs.d*
and then in the extfs.d directory of the package. Its
*list*
command lets the plugin browse a format libarchive cannot read, and
*copyout*
lets a file of it be viewed or copied;
*copyin*
and
*rm*
are optional and allow adding and deleting. An external helper works on an
archive with a path on the local disk, not on one that only a plugin stream
provides.

The file
*README.md*
beside the plugin describes the same keys for whoever adds a format, with the
details of the extfs protocol.

Which action opens the plugin for a file is decided by
*magic.ini*
of the configuration directory, not by
*arcmc.ini*:
one says how a format is handled, the other which file goes to the plugin.
