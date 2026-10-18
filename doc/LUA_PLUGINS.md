# Lua runtime and scripts

M-Commander can load optional Lua 5.3+ support.  Lua is not linked into
the `mc` executable: `mc-lua.so` is a runtime extension loaded through
`dlopen` only when it is available and enabled.  That extension discovers and
runs Lua scripts.

Build it with:

```sh
meson setup build -Dlua=enabled
meson compile -C build
```

A Lua installed in an unusual place is found through pkg-config: set
`PKG_CONFIG_PATH` to its `lib/pkgconfig` before `meson setup`.

`-Dlua=auto` is the default.  In that mode the runtime is not built when a
compatible Lua development package is unavailable.

## Enabling and disabling

Lua is enabled by default in a Lua-enabled build.  Put this in
`~/.config/mc6/ini` to disable it persistently:

```ini
[Lua]
enabled=false
user_scripts_dir=
```

`user_scripts_dir` may override the user script directory, but must be an
absolute path.  `mc --no-lua` and `MC_NO_LUA=1 mc` prevent Lua code from being
loaded for one run.

**Manage Plugins** shows one native runtime, `runtime / lua / Lua engine`.
Clearing it disables all Lua code on the next start.  Enter or F4 on that row
opens the global and user scripts from the `mcedit` workspace.  In the script
list, F4 opens the selected package's declared `entry` file in the internal
editor; **Run** lists and invokes only actions registered by the selected
package.  Event-only packages have no runnable action.  Individual script
choices are stored as `lua/<id>=true` in
`[DisabledPlugins]` of `~/.config/mc6/plugins.ini`.  No Lua script is hot-reloaded
during a session.

## Script layout

System scripts live in `${datadir}/mc/lua/scripts/`; user scripts live in
`${XDG_DATA_HOME:-~/.local/share}/mc6/lua/scripts/` (the
`${XDG_CONFIG_HOME:-~/.config}/mc/lua/scripts/` of earlier releases is still searched).  A script belongs to a fixed
workspace, determined by the directory immediately below `scripts/`:

```text
scripts/editor/base64-decode/lua.ini
scripts/editor/base64-decode/lib/format.lua       # optional
```

| Workspace | Directory below `scripts/` |
| --- | --- |
| `mc` | `mc/` |
| `mcedit` | `editor/` |
| `mcview` | `viewer/` |
| `mcterm` | `terminal/` |
| `mcdiff` | `diff/` |

The workspace is not a `lua.ini` property.  A top-level directory such as
`scripts/my-script/` is not discovered; place it under one of the directories
above instead.

The script manifest is named `lua.ini`.  It must contain:

```ini
[Lua]
id=my-script
api_version=1
name=My script
entry=init.lua
provides=events
```

The ID is limited to 64 characters from `A-Z`, `a-z`, `0-9`, `_`, `.` and
`-`, and must match its directory name.  IDs are global across all workspaces.
Scripts load in lexical ID order.  A user script with the same ID replaces the
whole system script.

`provides` describes the entry points declared by the script.  It is an
optional comma-separated list for compatibility with earlier scripts:
`events`, `macros`, or both.  The **Lua scripts** list displays this value, so
the user can distinguish a script that reacts to events from one that exposes
an action.  A script using `mc.macro()` must declare `provides=macros`.

Each script has its own Lua state.  `require("a.b")` searches the script's
`lib/a/b.lua` and then the shared library directory from the same origin.  C
modules are disabled for these lookups.

The shared directories are `${datadir}/mc/lua/lib/` for system scripts and
`${XDG_DATA_HOME:-~/.local/share}/mc6/lua/lib/` for user scripts.  A system script
never searches the user shared directory.

## API

### Macros

A Lua macro is an action registered while its script is loaded, but executed
only when its key is pressed in the declared area.  This is distinct from an
event handler: loading the script registers the macro; it does not run its
action.

For now the supported area is `editor`.  The script must be in
`scripts/editor/` (the `mcedit` workspace) and declare `provides=macros`:

```lua
mc.macro {
    id = "decode-base64",
    area = "editor",
    key = "F11",
    description = "Decode Base64 selection",
    priority = 50, -- optional; 0 through 100
    menu = {       -- optional direct menu entry
        path = "Tools",
        label = "Decode Base64",
        position = 100,
    },
    action = function (ev)
        -- ev is an editor.key-style event snapshot
        return mc.CONSUME
    end,
}
```

Macro IDs are unique inside their script.  Key names are case-insensitive and
use the same spelling as `ev.key.name`, for example `F11` or `Ctrl-S`.
The matching macro with the greatest priority runs; equal priorities retain
script load and registration order.  A macro consumes its key by default.
Return `false` or `mc.PASS` to allow normal editor processing to continue.
After three action errors, only that macro is disabled for the session.

`menu` optionally places the same action directly in an editor menu.  `path`
is the stable, untranslated top-level menu name.  Existing names such as
`Command` append to that menu; any other name creates a top-level menu.  The
optional `label` defaults to `description`.  `position` ranges from -100000 to
100000 and orders runtime-provided entries within the target menu; lower
values appear first.  Entries with the same position retain script load and
registration order.  Menu placement is independent of `listed`, which only
controls the **Run action** list.

### Processes

`mc.process.run()` synchronously runs a shell command through the core runtime
host and captures its output:

```lua
local result, err = mc.process.run {
    command = "git status --short",
    max_output = 8 * 1024 * 1024, -- optional, 1 byte through 64 MiB
}
```

On success, `result` contains binary-safe `stdout` and `stderr` strings,
`exit_code` (or `nil` if terminated by a signal), `signal`, and the booleans
`stdout_truncated` and `stderr_truncated`.  A non-zero command exit status is a
successful process invocation and must be handled by the script.  The call is
allowed only during a user-initiated callback and blocks the UI until the
command exits.  Commands are intentionally interpreted by `/bin/sh`; scripts
must not concatenate untrusted text into `command`.

### Event handlers

Register callbacks with `mc.on()` and remove them with `mc.off()`:

```lua
local token = mc.on("panel.chdir", function (ev)
    mc.ui.status("Directory: " .. ev.new_path)
end, { priority = 10 })

mc.on("shutdown", function ()
    mc.off(token)
end)
```

Priorities range from `-100` to `100`; higher callbacks run first and equal
priorities keep registration order.  `mc.off()` is idempotent.  For
`editor.key`, returning `true` or `mc.CONSUME` stops normal editor processing;
all other event callbacks are notifications.  Prefer `mc.macro()` for a
user-visible key action; `editor.key` remains the low-level notification and
interception event.

Available event names are:

- `startup`, `shutdown`
- `panel.chdir`, `panel.selection_changed`, `panel.file_open`
- `editor.open`, `editor.save`, `editor.key`
- `viewer.open`

Every callback receives a fresh, copied snapshot.  The event-specific fields
are:

| Event | Fields in `ev` |
| --- | --- |
| `startup` | `run_mode`, `config_dir`, `data_dir` |
| `shutdown` | `reason` |
| `panel.chdir` | `panel`, `old_path`, `new_path`, `cause` |
| `panel.selection_changed` | `panel`, `current`, `selected`, `selected_count`, `selected_truncated` |
| `panel.file_open` | `panel`, `path`, `open_mode`, `is_dir` |
| `editor.open` | `editor`, `path`, `readonly`, `line`, `column` |
| `editor.save` | `editor`, `path`, `previous_path`, `save_as` |
| `editor.key` | `editor`, `key` (`name`, `code`, optional `text`, `modifiers`) |
| `viewer.open` | `viewer`, `path`, `source_kind`, `start_line` |

`current` and every element of `selected` are `mc.File` snapshots with
`name`, `path`, `is_dir`, `size`, `mtime`, and `marked`.  `selected` contains
at most 4096 items; `selected_count` remains the full count.

### Objects and commands

Panel, editor, and viewer references are opaque userdata, never C pointers.
They are valid only while their MC window is alive; a later call returns
`nil, "closed"` if it has gone away.

| Object | Creation and methods |
| --- | --- |
| Panel | `mc.panel.active()`, `mc.panel.passive()`; `:cwd()`, `:current()`, `:selected()`, `:refresh()`, `:chdir(path)` |
| Editor | `mc.editor.current()`; `:info()`, `:selection()`, `:text([range])`, `:replace(range, text)`, `:replace_selection(text)`, `:edit(spec)`, plus the legacy cursor, text, insert, path, readonly, and save methods |
| Viewer | `mc.viewer.current()`; `:path()`, `:position()`, `:mode()`, `:goto(offset)` |

`cursor()` and `set_cursor()` use one-based line and column numbers.
Their columns are editor display columns and therefore expand tabs using the
current `editor:tab_width()`.  The tab width query returns the configured
positive tab-stop width; scripts that align text must use it rather than
assuming eight columns.  When the editor permits the cursor beyond the end of
a line, `cursor()` includes that virtual-space distance in the returned
column; inserting at that position requires materializing the gap as spaces.
`get_text(from, to)` uses one-based inclusive byte positions.  Lua reserves
the word `goto`, so call the viewer method as
`viewer["goto"](viewer, offset)`.

`selected_text()` returns the current ordinary text selection, or
`nil, "no_selection"`.  Column selections return
`nil, "column_selection_not_supported"` rather than silently decoding a
different range.

New buffer operations use zero-based, half-open byte ranges.  A stored range
should carry the revision that produced it:

```lua
local info = assert(editor:info())
local bytes = assert(editor:text {
    from = 0, to = info.byte_length, revision = info.revision,
})
assert(editor:replace({ from = 0, to = 3, revision = info.revision }, "new"))
```

`editor:edit { revision = n, changes = {...}, cursor = { offset = n } }`
validates every range before changing the buffer, applies non-overlapping
changes atomically, and creates one undo entry.  A changed document returns
`nil, "stale_revision"`.  `mc.ui.text_width(text)` returns MC's terminal display
width for valid UTF-8 text and is intended for alignment and drawing scripts.

Object and UI methods require an active MC event callback.  Outside one they
return `nil, "no active MC context"`; unavailable application modes return
`nil, "not_ready"`.  State-changing calls are rejected with
`nil, "forbidden_in_phase"` in `panel.file_open`.  That hook is a notification
only, not a way to intercept the file open operation.

`mc.ui.status(text)` updates the file-manager hint line and returns `true`.
`mc.ui.message(title, text)` displays a modal message.  Both return
`nil, "not_ready"` when no compatible UI is active; `message()` is also a
state-changing operation in `panel.file_open`.

`mc.ui.indicator { id, area = "editor", text, priority = 0 }` installs or
updates a persistent status-line indicator and `mc.ui.indicator_clear(id)`
removes it.  IDs are scoped to the owning package, so scripts cannot replace
one another's indicators.  Higher-priority indicators are placed first and
lower-priority ones are omitted when the status line is too narrow.  All
indicators owned by a package are removed automatically when it is unloaded.
Supported areas are `editor` and `viewer`.

```lua
mc.ui.indicator {
    id = "mode", area = "editor", text = "[╔═╗]", priority = 100,
}
-- later:
mc.ui.indicator_clear("mode")
```

### Screens

`mc.ui.screen(spec)` describes a full-screen grid of widgets that a script fills and drives.
The status line is at the top and the keys make the button bar at the bottom; between them
`layout`: rows of cells, one control per cell.  A row is `height = n` lines or `weight = n`,
a share of the lines left; a cell is `width = n` columns or `weight = n`.  `screen:run()`
shows it and returns when it is closed (F10 or Esc, the `close` action, `screen:close()`, or a
callback returning `{ close = true }`); it needs an active MC callback, a file handler for
instance.  Dialogs and viewers opened from a callback nest over the screen.

```lua
local screen = mc.ui.screen {
    title  = "sample.dbf",
    status = "1234 records",
    help   = { file = "help.hlp", node = "[DBF Viewer]" },
    layout = {
        { height = 1, { type = "label", id = "hint", text = "Enter: the card" } },
        { weight = 3,
          { weight = 1, id = "grid", type = "table",
            columns = {
                { id = "name", title = "NAME", align = "left", min_width = 4, expands = true },
                { id = "qty",  title = "QTY",  align = "right", min_width = 3 },
                { id = "sel",  title = "",     type = "check" },
            },
            row_count = 1234,     -- nil: unknown, rows are asked for until a short page
            page_size = 256,      -- rows asked for at a time
            rows = function(first, count)     -- first from 0; a cell is a string, a number,
                return read_page(first, count)    -- or { text = ..., color = "red;black" }
            end } },
        { weight = 1, { weight = 1, id = "card", type = "text", text = "" } },
    },
    keys = {
        { key = "f2",  label = "Struct", action = "structure" },
        { key = "f10", label = "Quit",   action = "close" },   -- built in, like "help"
    },
    on_row    = function(scr, ev) scr:update("card", { text = card_of(ev.row) }) end,
    on_enter  = function(scr, ev) ... end,
    on_action = function(scr, id, ev) ... end,
    on_key    = function(scr, ev) return ev.key.name == "x" end,  -- true: the key is taken
    on_check  = function(scr, ev) marks[ev.row] = ev.value end,
    on_resize = function(scr, columns, lines) end,
    on_close  = function(scr) end,
}
screen:run()
```

Cell types are `label` and `status` (`text`), `text` (a scrolling, wrapping text area),
`separator`, `input` (`value`), `checkbox` (`label`, `value`) and `table`; a screen may hold
any number of tables, each with its own `rows()`.  A column is `text` or `check`; check cells
read `1`, `x`, `true` or `yes` as checked.  A table keeps the pages it fetched recently and
asks `rows()` again for the others; Left and Right scroll its columns when they do not fit,
Tab moves between the cells.  Every callback gets `ev.control` (the cell the user is on) and,
for a table, `ev.row` (from 0), `ev.column` (the column id) and `ev.column_index`; `on_key`
also gets `ev.key.name` and `ev.key.code`.  The keys of `keys` are taken before any widget
sees them; only F1 through F10 have a button.

While the screen runs, `screen:status(text)` changes the status line and
`screen:update(control, patch)` a cell: `patch.text` for a label, status or text cell,
`patch.value` for an input or checkbox, and for a table `patch.rows` (a new rows function),
`patch.row_count`, `patch.invalidate` (drop the rows fetched so far) and `patch.row` (the
current row).

A file handler registered with `mc.file_handler.register { id, kind, handler }` answers a
`magic.ini` rule `%plugin{lua(<script-id>):<id>}`.  The handler returns `{ handled = true,
controller = ... }` to show a viewer source, `{ handled = true }` alone when it did the work
itself (for instance `panel:chdir()` to a panel provider of its own), or `nil, "not_supported"`
to leave the file to MC's usual handling.

A viewer-source definition may provide `source_state(session, event)`.  Process sources report
`started`, then exactly one of `finished`, `failed`, or `cancelled`.  The event also contains
`generation` and `output_size`; `failed` may include `exit_code` or `signal`.  Rebuilding a source
cancels the old generation before starting the new one, so asynchronous UI state can ignore stale
terminal events.

A prepared viewer spec may set `initial_display`: `"text"` (the default) shows the
source as plain text, `"nroff"` turns on the viewer's nroff mode, so overstruck
letters (`a\ba`, `_\ba`) are painted bold and underlined, and `"terminal"` feeds
the source to the embedded terminal, so ANSI escapes and cursor movement work.

In the terminal display a prepared spec may set `top_row`: the row of the output the
viewer opens at, counted from 0.  It is what a source that writes a whole page at once
uses to put a line of its own in view; `auto_scroll = "bottom"` ignores it.

A prepared spec may also set `wrap`: `false` opens the viewer without wrapping, so a
line wider than the screen is reached by scrolling sideways, `true` opens it with
wrapping, and leaving the field out keeps whatever the user set.  A source that lays
its own text out uses it for a table or a diagram it does not want broken in two.

A viewer-source definition may take viewer keys of its own with `keys` (up to 32 key
names, as in the keymap: `"gt"`, `"plus"`, `"alt-n"`) and the `on_key(session, params, key)`
callback they go to.  Such a key reaches the source before the viewer looks it up in its
own keymap, and `key` is the name the definition gave it.  `on_key` returns new params,
and `prepare()` runs again with them, exactly like `options()`; `nil` leaves the source as
it is.  Function keys, `Esc`, `q` and `Ctrl-O` are refused: they belong to the viewer.

A prepared viewer spec may set `raw_path` to the original local file.  The viewer then keeps the
controller attached while F8 switches between the generated source and the raw file.

A viewer-source definition may carry `help = { file = "help.hlp", node = "[Image Viewer]" }`:
F1 in that view opens the node of that file instead of MC's own help.  A relative `file` is
taken from the script's directory, so a script ships its help next to its `init.lua` in MC's
help format (`[Node]` headings).  The node shown gets a link to the node of MC's help it stands
in for (the viewer's), and a link or F2 (Index) naming a node the file does not have opens it
in MC's help.  Panel providers accept the same `help` table.

An `input` control in `mc.ui.dialog()` accepts an optional `history` name and
an optional `completion` array.  `complete_on_tab = true` makes `Tab` invoke
completion while that input has focus; `Shift-Tab` still moves to the previous
control.  Completion providers are `files`, `hosts`, `commands`, `variables`,
`users`, `cd`, and `shell`; they use MC's existing input completion engine and
may be combined.  For example:

```lua
{
    id = "command", type = "input", value = "",
    history = "my-command-history",
    complete_on_tab = true,
    completion = { "commands", "files", "variables", "shell" },
}
```

`mc.log.debug/info/warn/error(text)` writes a message tagged with the Lua
script ID.

Teaching examples (`notify-editor-save`, `demo-panel`) are installed under
`PREFIX/share/mcommander/lua/examples`; MC never loads scripts from there.  Copy an
example into the corresponding user workspace directory to try it, and copy
an installed script before adapting it, so system updates do not overwrite
local changes.

## Trust boundary

Lua scripts run with the permissions of the current MC process.  Install only
scripts you trust.  MC rejects a script or loaded Lua module if its directory
tree is symbolic-linked, owned by neither the current user nor root, or is
group/world writable.  A Lua callback error is isolated to that callback;
after three errors in one session the callback is disabled.
