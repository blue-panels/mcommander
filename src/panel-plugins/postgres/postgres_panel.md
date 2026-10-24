# PostgreSQL Plugin <!-- help:notitle -->

```
  PostgreSQL Panel Plugin

  The PostgreSQL plugin shows the servers of PostgreSQL as
  panel contents: connection -> database -> schema ->
  relation. It reads the catalog of PostgreSQL 10 and newer;
  an older server is refused when it is connected to.

  Opening the plugin

    Alt-F1 / Alt-F2 -> PostgreSQL
                    The list of connections of
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
      connect_by          host, url or service: what names
                          the server
      host, port          host name, address or directory of
                          sockets, and port (connect_by=host)
      url                 postgresql://user@host:port/db?...
                          (connect_by=url)
      service             a service of pg_service.conf
                          (connect_by=service)
      database            the database; with
                          show_all_databases, the one the
                          list of databases is read from;
                          empty: what libpq takes, the
                          name of the user
      show_all_databases  false: the panel has only the
                          database
      user, sslmode       the user, the SSL mode of libpq
      save_password       true: a password typed for the
                          connection goes to ~/.pgpass once
                          it has connected
      description         a note
      read_only           true: the query editor opens
                          read-only transactions

    A field not given is left to libpq: the service file and
    the PG* variables of the environment, as psql does.

    No password is kept in postgres.ini. libpq reads it from
    ~/.pgpass (or $PGPASSFILE) or from the service file. When
    the server asks for one there is none of, the plugin asks
    for it and keeps it for the session; with save_password it
    is written to ~/.pgpass, for the host, the port and the
    user, any database, at mode 0600.

  The dialog of a connection

    Connect by chooses Host, URL or Service; the fields the
    choice does not use are disabled. With Host the URL is the
    one the fields make, read-only; going from URL back to
    Host fills the fields from the URL. Test connection
    connects with what is in the fields and tells the version
    of the server, or the error of libpq; it asks for a
    password the server wants and the fields have not got, and
    leaves it in the Password field. Ctrl-C gives up the wait
    for a server.

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
      Enter           Connect, list the databases.

    On the list of databases:
      Enter           Open the database.
      ..              Close the connection, back to the list
                      of connections.

    The list has the databases the user may connect to,
    without the templates, with their size and owner.
    Without "Show all databases" it has the database of the
    connection only.
    The connection of the panel is read-only and bounded by
    statement_timeout; Ctrl-C cancels a query of the panel. A
    connection found lost is opened again once.

    Below the connections the panel changes nothing: F4, F6,
    F7, F8 say so. The objects are changed with SQL in the
    query editor.
```
