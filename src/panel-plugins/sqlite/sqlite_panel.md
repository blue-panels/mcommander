# SQLite Plugin <!-- help:notitle -->

SQLite database browser panel plugin

Opening a database:

```
  Enter on a database file opens it, and F3 on the same file shows its
  schema.  Both come from magic.ini, which binds the file to this plugin:

    [sqlite.database]
    Type=^SQLite 3\\.x database
    Open=%plugin{sqlite:open}
    View=%plugin{sqlite:view}

  Add your own rules to ~/.config/mc6/magic.ini to open other names or types
  here.  A database inside another panel plugin, an archive for example, is
  read from a temporary copy and has no location of its own.

  You can also choose "SQLite database" in the Panel menu or drive menu,
  or type a path such as:

    cd sqlite:/absolute/path/to/database.sqlite
```

The database is always opened read-only.  This plugin never changes its
schema or records.

```
  The database and any active -wal/-shm sidecar files must be readable.  A
  database that needs an unreadable WAL file cannot be opened.
```

Navigation:

```
  Database -> table or view -> row page -> JSON row

  Each table and view contains schema.sql and one or more row-page
  directories.  A row page has at most 200 records, so large databases do
  not create an unwieldy panel listing.

  F3 or Shift-F3 on a table or a view shows its first 200 rows as a table
  of text, as psql prints it: a column for a column, the numbers to the
  right.  On a row page it shows the rows of that page.  The table is made
  as wide as the screen: when it does not fit, the widest columns become
  narrower, and a value cut at the end shows "~".  A NULL is an empty cell,
  a BLOB is written as its length, and a newline in a value as a space.
  Enter still opens the table or the page.

  F3 on schema.sql shows the CREATE statement.  F3 on a JSON row shows its
  column names and values.  If a text column itself contains a JSON object or
  array, F3 unfolds it as nested JSON.  Shift-F3 shows the original, unmodified
  stored value instead.  BLOB values are represented as a hexadecimal preview
  together with their original byte length.  Invalid UTF-8 in a TEXT value is
  shown as byte escapes.

  Ordinary rowid tables are read in rowid order.  Views and WITHOUT ROWID
  tables are supported too, but very large ones can be slower to browse.  Use
  Ctrl-C to cancel a long database scan.
```
