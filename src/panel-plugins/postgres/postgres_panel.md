# PostgreSQL Plugin <!-- help:notitle -->

```
  PostgreSQL Panel Plugin

  The PostgreSQL plugin shows the servers of PostgreSQL as panel
  contents: connection -> database -> schema -> relation.

  Opening the plugin

    Alt-F1 / Alt-F2 -> PostgreSQL   The list of connections of
                                    ~/.config/mc6/postgres.ini.

  Configuration: ~/.config/mc6/postgres.ini

    [General]
      default_connection   the connection the cursor starts
                           on when the plugin opens
      connect_timeout      seconds to wait for a server
                           (default 10, 0: no limit)
      statement_timeout    seconds a query of the panel may
                           take (default 30, 0: no limit)
      show_system_schemas  true also lists pg_catalog,
                           information_schema and pg_toast
      coole                the query editor; empty: found
                           in PATH
      psql                 psql, run without coole; empty:
                           found in PATH

    The [General] options are edited from the Manage Plugins
    dialog (Options menu): select the PostgreSQL row and press
    Enter or F4.

    [Connection.<name>]
      service         a service of pg_service.conf
      host            host name, address or socket directory
      hostaddr        numeric address, no name lookup
      port            port
      user            user name
      sslmode         disable, prefer, require, verify-full...
      dbname          only this database: the list of
                      databases is left out
      maintenance_db  the database the list of databases is
                      read from (default postgres)
      description     a note
      read_only       true: the query editor opens read-only
                      transactions

    The keys but the last four are those of libpq. A key not
    given is left to libpq: the service file and the PG*
    variables of the environment, the way psql does it.
    service= is the preferred way to name a server.

    The file has no passwords: libpq reads them from ~/.pgpass
    or from the service file.

  Hierarchy and keys

    On the list of connections:
      F7, Shift-F4    A new connection: the name and the keys
                      above, written as a [Connection.<name>]
                      section (the file is created at mode
                      0600).
      F4              Edit the connection under the cursor.
                      Renaming rewrites its section.
      F8              Delete the marked connections, or the
                      one under the cursor, from postgres.ini.
```
