# Access control list

The Attributes menu entry
**Access control list...**
shows the access control list of a local file or directory and lets you change
the rights of users and groups. It supports POSIX ACLs, the ones `getfacl` and
`setfacl` show, and Windows ACLs on supported mounted shares and NTFS disks.
The file system must support ACLs. Virtual file system paths such as `smb://`
are not supported; open the file through its local mount point instead.

When files are marked, the dialog goes through them one by one; otherwise it
opens for the file under the cursor, and on *..* for the directory above, as
Chmod does.

A symbolic link shows the file it points to, and the list of that file is the
one that changes.

**POSIX access control lists**

The owner, the group of the file and other appear first, followed by named
users and groups. Each line is one entry:

```
owner   the owner of the file
group   the group of the file
other   everybody else
user    a user named in the list
group   a group named in the list
```

*Read*, *Write*, *Exec*
: What the entry is given.

*Effective*
: The entry's rights after applying the mask. The mask limits every named user
and group and the group of the file. It does not limit the owner or other.
A right the mask takes away is drawn in another color, and the entry is marked
*masked*.

**Keys in the list**

**Ins**
: Toggle the `*` mark on the entry under the cursor and move down. When the
current entry is marked, switching a permission sets that permission to the
same value on every marked entry. When it is not marked, only that entry
changes. Marks apply to the list currently displayed.

*Add*
: Add users or groups, all with the same rights. *Choose* picks them from the
accounts of the system: **Ins** there marks one and moves down, and OK takes
the marked ones, or the one under the cursor if none are marked. System
accounts are hidden until *Show system accounts* is checked, unless the
selected account is a system account. Names can also be typed, separated by
commas or spaces. A numeric UID or GID, optionally prefixed with `#`, can be
used even if the account is not listed. Adding an account already in the ACL
replaces its rights.

**Enter**, **F4**
: Change only the entry under the cursor, even when entries are marked.

**Del**, **F8**
: Remove the marked named users and groups, or the entry under the cursor if
none are marked. The owner, the group of the file and other cannot be removed
from the access list. See *Default ACLs* below for removing a default list.

**r**, **w**, **x**
: Switch read, write or execute. If the current entry is marked, set that
permission to the same value on all marked entries.

**Left**, **Right**, **Space**
: **Left** and **Right** move between the Read, Write and Exec boxes;
**Space** switches the selected box. A click on a box switches it too.
These changes also apply to all marked entries when the current one is marked.

*Mask*
: The button in the header shows the mask and opens its dialog. The existing
mask is kept when the ACL is opened or when switching between its two lists.
With *Set it from the entries* on, changing entries recalculates the mask from
the rights of the group of the file and all named users and groups. Changing
a box in the mask dialog sets the mask by hand and turns recalculation off.
Turn *Set it from the entries* off to keep a restrictive mask while editing
entries.

**Default ACLs**

A directory has two lists: *This folder* controls access to the directory
itself; *New files inside* is the default ACL inherited by newly created files
and directories. Inherited permissions are also limited by the mode requested
when creating a file. Without a default ACL, the creation mode is limited by
umask instead.

Adding the first named entry to an empty default list brings in the owner,
the group and other from the access list. To remove the whole default ACL,
first remove its named users and groups, clear any marks, then press **Del**
or **F8** on one of the three base entries and confirm removal.

**Several files**

*Set*
: Write this file and go on to the next one.

*Set all*
: Replace the access ACL of this file and the remaining marked files with
the list being edited. The default ACL is copied only between directories;
copying from a regular file leaves a target directory's default ACL unchanged.

*Changes to all*
: Write this file, and make the same changes on the rest: add, change and
remove only the entries changed in the dialog, and keep what else each file
has. Removing an entry a file does not have does nothing.

*Cancel*
: Discard changes for this file and stop processing the remaining files.
Changes already saved to earlier files remain.

If saving fails, *Skip* continues with the next file and *Abort* stops.
Unless running as root, a file that the current user does not own opens for
reading only.

**Windows access control lists**

A Windows share mounted through cifs, and an NTFS disk mounted through
ntfs-3g or ntfs3, can expose Windows access control lists. The dialog lists who
each entry is for, whether it allows or denies, and the set of rights, with the name
Windows gives it (*Full control*, *Modify*, *Read & execute*, *Read*, *Write*)
or *Special*. *(inside only)* marks an entry that applies to what the folder
holds but not to the folder itself; *(inherited)* one that came from the
folder above. The displayed rights belong to that entry; they are not a
calculation of a user's effective access across all matching entries.

**Ins**
: Add entries: *Allow* or *Deny*, a user or a group, the rights, and for a
folder where they apply. *Advanced...* shows every right as a box of its own.
Names are typed or picked with *Choose...*, several separated with commas.
Spaces within a name are kept, as in *Authenticated Users* or *CREATOR OWNER*.
For a directory, *Applies to* selects whether an entry affects the directory
itself, its files, its subdirectories, or a combination of these. Entries
which apply only to children are shown as *(inside only)*.

**Enter**, **F4**
: Change the entry under the cursor.

**Del**, **F8**
: Remove it.

**F3**
: Show the rights of the entry one by one.

The *Add*, *Edit* and *Delete* buttons perform the same actions as these keys.
An inherited entry cannot be edited or removed here; change it in the folder
it came from. New deny entries go before allow entries, and new explicit
entries go before inherited ones. Changing an entry between *Allow* and *Deny*
also moves it to the appropriate position.

*Set* writes the list and opens the next marked file. *Cancel* discards the
current changes and stops. Windows ACLs do not have *Set all* or *Changes to
all*. A Samba server may store the list in a different form; the dialog
reports this after saving. Removing the last entry leaves an empty DACL,
which denies access to everyone.

Accounts are named where the system can tell: the well known ones such as
*Everyone* and *Administrators*, and local users and groups, which show as
*Unix User\name* and *Unix Group\name*. Numeric local IDs can be entered as
*Unix User\#uid* or *Unix Group\#gid*. Domain accounts show as their SID and
can be entered in that form, such as `S-1-5-21-1000-2000-3000-1001`;
domain names are not resolved by the dialog.

A file with an NFSv4 access control list is only reported; `nfs4_getfacl`
shows it.
