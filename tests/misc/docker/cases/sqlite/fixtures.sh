#!/bin/sh
# Build the sqlite cases under $1.
#
# The sqlite plugin opens a database as a directory tree, read-only:
# database -> table or view -> a page of rows -> one row, with a schema.sql at
# every level.  magic.ini binds it both by what libmagic says and by the name.
set -e

dir="${1:-/home/mc/cases/sqlite}"
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

mkdir -p 01-tables

sqlite3 01-tables/library.sqlite <<'SQL'
CREATE TABLE authors (id INTEGER PRIMARY KEY, name TEXT NOT NULL, born INTEGER);
CREATE TABLE books (id INTEGER PRIMARY KEY, title TEXT, author_id INTEGER, pages INTEGER);
CREATE VIEW recent AS SELECT title, pages FROM books WHERE pages > 100;
INSERT INTO authors VALUES (1, 'Strugatsky', 1925), (2, 'Lem', 1921);
INSERT INTO books VALUES (1, 'Roadside Picnic', 1, 224), (2, 'Solaris', 2, 204), (3, 'A short one', 2, 40);
SQL

# The same bytes, with a name the rule does not know: what is left is libmagic
cp 01-tables/library.sqlite 01-tables/nameless.dat

cat > 01-tables/cases.tsv <<'EOF'
file	key	expect	why	transports
library.sqlite	Enter	text: authors	the database opens as a listing of its tables	local
library.sqlite	Enter	text: recent	a view is a level of the tree as a table is	local
library.sqlite	Enter	text: schema.sql	every level carries the statements that made what is below it	local
library.sqlite	Enter,on books,Enter	text: rows	a table opens on its pages of rows	local
library.sqlite	Enter,on authors,Enter	text: schema.sql	and carries a schema of its own	local
library.sqlite	Enter,on books,Enter,on rows,Enter	text: row-000000000001.json	a page holds one file per row; the cursor lands on ".." after a chdir, so it is put on the page first	local
library.sqlite	Enter,on books,Enter,on rows,Enter,on row-000000000001,F3	text: Roadside Picnic	a row is shown as JSON, and the text of the record is in it	local
library.sqlite	Enter,on books,Enter,on rows,Enter,on row-000000000001,F3	text: pages	one field per column name	local
nameless.dat	Enter	text: authors	the name says nothing, so libmagic is what names it	local
EOF

# A table named in CP1251, while mc runs in UTF-8: the bytes of the name are no
# UTF-8, and Alt-e on the panel says what they are.  SQLite keeps a name as the
# bytes it was given.  The database itself has a Cyrillic name in UTF-8, the
# codeset of the disk: Alt-e must not touch that one.
mkdir -p 02-names
printf 'CREATE TABLE "люди" (name TEXT);\nINSERT INTO "люди" VALUES (1);\n' \
    | iconv -f UTF-8 -t CP1251 | sqlite3 02-names/данные.sqlite

# Windows 1251 is entry 5 of the codepage list, "No translation" is "-"
cat > 02-names/cases.tsv <<'EOF2'
file	key	expect	why	transports
данные.sqlite	Enter	no text: люди	the name is drawn as the bytes it is, not in the codeset of the terminal	local
данные.sqlite	Enter,key M-e,type 5	text: люди	Alt-e with Windows 1251 recodes the names of the plugin	local
данные.sqlite	Enter,key M-e,type 5	text: schema.sql	and the panel stays in the plugin, where Alt-e used to leave for the directory on the disk	local
данные.sqlite	Enter,key M-e,type 5	text: данные.sqlite	the title is not recoded: the name of the database in it is in the codeset of the disk	local
данные.sqlite	Enter,key M-e,type 5,key Down,Enter	text: rows-000000000001	the plugin gets the bytes of the name back and opens the table	local
данные.sqlite	Enter,key M-e,type 5,key C-s,type лю,key Enter	text: rows-000000000001	quick search finds the name as it is drawn	local
данные.sqlite	Enter,key M-e,type 5,key Escape,key M-S,type лю	no text: schema.sql	the quick filter matches the name as it is drawn and hides the rest	local
данные.sqlite	Enter,key M-e,type 5,key M-e,type -	no text: люди	"No translation" draws the bytes again	local
EOF2

echo "sqlite cases in $dir"
