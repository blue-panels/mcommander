# MongoDB Plugin <!-- help:notitle -->

```
  MongoDB Panel Plugin

  The MongoDB plugin lets you navigate a MongoDB deployment as panel
  contents: cluster -> database -> collection -> documents, with F3
  to view a document as pretty-printed JSON.

  Opening the plugin

    Alt-F1 / Alt-F2 -> MongoDB    Opens the list of clusters loaded
                                  from ~/.config/mc6/mongo.ini.

  Configuration: ~/.config/mc6/mongo.ini

    [General]
      default_cluster              optional preferred cluster name
      bucket_strategy              "auto" groups large document lists
                                   into nested buckets; "none" shows a
                                   flat list
      bucket_leaf_size             max flat docs before bucketing kicks
                                   in (must be > 0)
      bucket_fanout                children per bucket level (must be
                                   >= 2)
      page_size                    documents per page (default 1000)
      server_selection_timeout_ms  SRV / DNS / topology discovery
      connect_timeout_ms           TCP connect
      socket_timeout_ms            operation socket
      op_max_time_ms               per-query maxTimeMS
      coole                        the query editor for Alt-F3;
                                   empty: found in PATH
      mongosh                      the mongosh program coole runs
                                   the queries with; empty: found
                                   in PATH

    The [General] options above are edited from the Manage Plugins
    dialog (Options menu): select the MongoDB row and press Enter or
    F4. On OK they are written back to mongo.ini while the
    [Cluster.*] sections are preserved.

    [Cluster.<name>]
      uri          mongodb:// or mongodb+srv:// URI
      description  optional human-readable note
      read_only    true blocks F4 / F5 / F7 / F8 before they open;
                   false allows all mutations

    Keep mongo.ini at mode 0600: it stores the URI in plaintext.

  Hierarchy and keys

    On the cluster list:
      Enter           Connect, list databases.
      F7              Create a new connection: prompts for a name,
                      URI, optional description, and a read-only
                      flag, then writes a [Cluster.<name>] section
                      to mongo.ini (created at mode 0600 if absent).
                      The list opens empty when no mongo.ini exists,
                      so the first connection is made here too.
      F4              Edit the selected connection: name, URI,
                      description, and read-only flag. Renaming
                      rewrites the [Cluster.<name>] section. General
                      plugin options live in the Manage Plugins
                      dialog (Options menu), not here.

    On the database list:
      Enter           List collections; the Size column shows
                      estimated document count.

    On the collection list:
      Enter           List documents (first page).

    On the document list:
      Enter           Pagination only -- on "*page:prev" or
                      "~page:next" virtual entries.
      F3              View document as Relaxed Extended JSON,
                      pretty-printed. Long base64 payloads are
                      truncated to ~24 chars so binary blobs do not
                      flood the viewer.
      F4              Edit document: opens the editor on the JSON
                      copy. On save, the plugin parses the JSON,
                      verifies _id is unchanged, and runs
                      replaceOne({_id: <original>}, <new>).
      F5 / F6         Copy / move a JSON file from the other panel
                      INTO the collection (insertOne).
      F7              Insert an empty document (auto-_id); use F4
                      afterwards to fill in fields.
      F8              Delete the marked documents (deleteMany).

    On every level:
      Alt-F3          The query editor, see below.

    Going up one level (".." or "*page:prev") always restores cursor
    onto the entry you just left.

  The query editor

    Alt-F3 runs coole, the editor of queries for mongosh ("coole
    -Q"), for the cluster and the database under the cursor, or
    the open ones, in a screen of its own beside the panels, the
    editors and the viewers. coole has the whole screen and every
    key but Alt-F3, which comes back to the panels. In coole F5
    runs the query under the cursor, Ctrl-F5 the whole file, and
    the result is shown at the bottom; the queries of a database
    are kept in ~/.local/share/coole/queries/<database>.js.

    Alt-F3 is the command PluginConsole of the [panel] section of
    the keymap, and can be bound to another key there.

    Without coole, Alt-F3 runs mongosh itself there, connected to
    the database: the shell of MongoDB at its prompt, without the
    editor; .exit closes it. With neither, Alt-F3 says what to
    install.

    One coole runs per cluster and database: Alt-F3 again brings
    back the same one. Its screen stays in the list of screens
    (Alt-`) until coole is quit.

    The URI, with its password, goes to coole in its environment
    (MC_MONGO_URI, MC_MONGO_DB, MC_MONGOSH), never on a command
    line, where ps would show it; so does it to mongosh.

  Read-only clusters

    On a cluster with read_only=true, F4 / F7 / F8 are caught early
    and the modal "Cluster is read-only." dialog is shown without
    starting the corresponding action. F5 / F6 from another panel
    reach put_file directly, which refuses with the same message.

  Document identifiers (_id)

    Panel file names use opaque slot tokens "doc:NNNNN" valid only
    within the current page snapshot. The real _id of every type
    (ObjectId, string, integer, binary, document, ...) is shown in
    the custom "_id" column. The Type column carries the BSON tag:
    "oid", "str", "int", "i64", "dbl", "bool", "null", "date",
    "doc", "arr", "bin".

  Pagination

    Big collections page automatically. The virtual entries
    "<<  prev page (X..Y)" and ">>  next page (X..Y)" are
    inserted at the top and bottom of the document list when
    applicable. Enter on either re-fetches the new range.

  Editor expectations and _id immutability

    The editor receives Relaxed Extended JSON: ObjectIds as
    {"$oid": "..."}, dates as ISO strings, and so on. The plugin
    parses it back via bson_new_from_json. If your document used
    BSON types not representable in Relaxed JSON, switch the cluster
    to read_only for safety -- v1 does not promise Canonical
    round-trip yet.

    Changing the _id in F4 is rejected before the request reaches
    the server: the plugin compares the new _id with the original
    (OID, string, int, int64, bool, double, null are checked) and
    refuses with "_id is immutable" if they differ. Remove the
    _id field from the JSON to keep the original.
```
