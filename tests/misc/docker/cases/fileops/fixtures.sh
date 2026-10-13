#!/bin/sh
# Build the file operation cases under $1.
#
# Copy, move and delete: what the dialog says before anything is touched, what
# happens when the target is already there, and what is left in the panel
# afterwards.  The cases that change the tree are local only, and the tree is
# built again before each of them.
set -e

dir="${1:-/home/mc/cases/fileops}"
# the read-only directories below keep even their owner from removing what is
# in them, and the tree is built again as that owner on the remote host
[ -d "$dir" ] && chmod -R u+w "$dir"
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

# ------------------------------------------------------------------ copy ---

mkdir -p 01-copy/target 01-copy/sub
printf 'the file that is copied\n' > 01-copy/copy-me.txt
printf 'the file that is already there\n' > 01-copy/target/copy-me.txt
printf 'inside the subdirectory\n' > 01-copy/sub/inside.txt
# bigger than the 64k tmpfs the sandbox mounts at /small
dd if=/dev/zero of=01-copy/big.bin bs=1024 count=256 2>/dev/null

cat > 01-copy/cases.tsv <<'EOF'
file	key	expect	why	transports
copy-me.txt	F5	text: Follow links	F5 on a file opens the copy dialog	local
copy-me.txt	F5	text: to:	and asks where to put it	local
copy-me.txt	F5,Enter	copy to the other panel	Enter copies it to the other panel	local
copy-me.txt	F5,key C-a,key C-k,type target,Enter	text: File exists	a target that is already there is a question, not an overwrite	local
copy-me.txt	F5,key C-a,key C-k,type target,Enter	text: Overwrite this file?	and the question is which file to keep	local
copy-me.txt	F5,key C-a,key C-k,type target,Enter,key Escape	text: copy-me.txt	Escape leaves the question and the panel as they were	local
sub	F5	text: Copy directory	a directory says so in the dialog	local
big.bin	F5,key C-a,key C-k,type /small,Enter	text: No space left on device	a full filesystem is an error dialog, not a silent half a file	local
EOF

# ------------------------------------------------------------------ move ---

mkdir -p 02-move/target
printf 'the file that is moved\n' > 02-move/move-me.txt
printf 'the file that is already there\n' > 02-move/target/move-me.txt
printf 'the file that is renamed\n' > 02-move/rename-me.txt

cat > 02-move/cases.tsv <<'EOF'
file	key	expect	why	transports
move-me.txt	F6	text: Move file	F6 on a file opens the move dialog	local
rename-me.txt	F6,key C-a,key C-k,type renamed.txt,Enter	text: renamed.txt	a name typed instead of a path is a rename in place	local
rename-me.txt	F6,key C-a,key C-k,type renamed.txt,Enter	no text: rename-me.txt	and the old name is gone from the panel	local
move-me.txt	F6,key C-a,key C-k,type target,Enter	text: Overwrite this file?	a move onto a file that is there asks as a copy does	local
EOF

# ---------------------------------------------------------------- delete ---

mkdir -p 03-delete/full/inner
printf 'the file that is deleted\n' > 03-delete/delete-me.txt
printf 'the file that stays\n' > 03-delete/keep-me.txt
printf 'inside the directory\n' > 03-delete/full/inside.txt
printf 'deeper still\n' > 03-delete/full/inner/deeper.txt

cat > 03-delete/cases.tsv <<'EOF'
file	key	expect	why	transports
delete-me.txt	F8	text: Delete	F8 asks before it deletes	local
delete-me.txt	F8,Enter	no text: delete-me.txt	and the file is gone from the panel afterwards	local
delete-me.txt	F8,Enter	text: keep-me.txt	while the rest of the listing stays	local
delete-me.txt	F8,key Escape	text: delete-me.txt	Escape at the question keeps the file	local
full	F8,Enter	text: not empty	a directory with files in it is a second question	local
full	F8,Enter	text: Delete it recursively?	which says what saying yes means	local
EOF

# ----------------------------------------------------------------- links ---

mkdir -p 04-links
printf 'what the link points at\n' > 04-links/real.txt
ln -s real.txt 04-links/tofile
ln -s nowhere 04-links/dangling

cat > 04-links/cases.tsv <<'EOF'
file	key	expect	why	transports
tofile	F8,Enter	no text: tofile	deleting a link takes the link	local
tofile	F8,Enter	text: real.txt	and leaves what it points at	local
dangling	F8,Enter	no text: dangling	a link to nothing is deleted like any other	local
tofile	F6,key C-a,key C-k,type renamed,Enter	text: @renamed	a renamed link is still a link	local
EOF

# ---------------------------------------------------------------- upload ---
#
# Copying into a panel a plugin drives.  The destination is not a filesystem
# mc can write to, so the core makes each directory through the plugin and
# hands it the files one by one.  The plugin panel is the one the transport
# opened; the other panel is pointed at the local copy of this tree.

mkdir -p 05-upload/source/tree/deep 05-upload/source/empty
printf 'a file of its own\n' > 05-upload/source/plain.txt
printf 'inside the tree\n' > 05-upload/source/tree/inner.txt
printf 'two levels down\n' > 05-upload/source/tree/deep/deeper.txt

cat > 05-upload/cases.tsv <<'EOF'
file	key	expect	why	transports
source	key Tab,cd /work/local/fileops/05-upload/source,on plain.txt,F5,Enter,key Tab,on plain.txt,F3	text: a file of its own	a file goes into the plugin panel	ftp,sh
source	key Tab,cd /work/local/fileops/05-upload/source,on tree,F5,Enter,key Tab,on tree,Enter	text: inner.txt	a directory goes in with what is in it	ftp
source	key Tab,cd /work/local/fileops/05-upload/source,on tree,F5,Enter,key Tab,on tree,Enter,on deep,Enter	text: deeper.txt	and with the directories below it	ftp
source	key Tab,cd /work/local/fileops/05-upload/source,on empty,F5,Enter,key Tab,on empty,Enter	text: 05-upload/empty	an empty directory is made, not skipped	ftp
source	key Tab,cd /work/local/fileops/05-upload/source,on tree,F6,Enter,key Tab,on tree,Enter,on deep,Enter	text: deeper.txt	F6 takes the whole directory over as well	ftp
EOF

# --------------------------------------------------------- plugin delete ---
#
# A directory that is not empty, removed from inside a plugin panel: by F8, and
# by F6, which removes what it has taken once the copy is there.  An FTP server
# removes an empty directory only, so the plugin has to empty it first.

mkdir -p 06-plugin-delete/doomed/deep 06-plugin-delete/moved/deep
printf 'inside the directory\n' > 06-plugin-delete/doomed/inner.txt
printf 'two levels down\n' > 06-plugin-delete/doomed/deep/deeper.txt
printf 'inside the directory\n' > 06-plugin-delete/moved/inner.txt
printf 'two levels down\n' > 06-plugin-delete/moved/deep/deeper.txt
mkdir -p 06-plugin-delete/stuck/a-locked 06-plugin-delete/jammed/a-locked 06-plugin-delete/readonly
for d in stuck jammed; do
    printf 'a name the server removes\n' > 06-plugin-delete/$d/first.txt
    printf 'another one\n' > 06-plugin-delete/$d/last.txt
    printf 'the server will not remove this\n' > 06-plugin-delete/$d/a-locked/kept.txt
done

cat > 06-plugin-delete/cases.tsv <<'EOF'
file	key	expect	why	transports
doomed	F8,Enter	no text: /doomed	F8 takes a directory with everything in it	ftp
doomed	F8,Enter	no text: Delete failed	and says nothing went wrong	ftp
moved	F6,Enter	no text: delete from plugin failed	F6 out of a plugin panel removes the directory it took	ftp
stuck	F8,Enter	text: Delete failed	a name the server will not remove is an error	ftp
stuck	F8,Enter	text: Cannot delete stuck	a name the server will not remove is an error	sh
stuck	F8,Enter,Enter,on stuck,Enter	no text: first.txt	and everything it will remove is gone all the same	ftp
stuck	F8,Enter,Enter,on stuck,Enter	no text: last.txt	whichever side of the refusal it was listed on	ftp
stuck	F8,Enter,Enter,on stuck,Enter,on a-locked,Enter	text: kept.txt	while what it refused is still there	ftp
stuck	F6,Enter	text: delete from plugin failed	F6 says the copy arrived but the source could not all go	ftp
jammed	F6,Enter,Enter,key Tab,on jammed,Enter,on a-locked,Enter	text: kept.txt	and the copy has everything, the refused names too	ftp
readonly	Enter,key Tab,cd /work/local/fileops/05-upload/source,on tree,F5,Enter	text: Cannot copy tree to plugin	a directory the server will not write to is an error	ftp
readonly	Enter,key Tab,cd /work/local/fileops/05-upload/source,on tree,F6,Enter	text: Cannot move tree to plugin	and a move says so too	ftp
readonly	Enter,key Tab,cd /work/local/fileops/05-upload/source,on tree,F6,Enter,Enter	text: /tree	and keeps the source	ftp
EOF

# the server refuses to remove what is in these, and to write into them; the
# locked one is listed first, so that what comes after the refusal is tried too
chmod 555 06-plugin-delete/stuck/a-locked 06-plugin-delete/jammed/a-locked 06-plugin-delete/readonly

echo "fileops cases in $dir"
