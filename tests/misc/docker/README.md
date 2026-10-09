# Sandbox

Docker environments for trying out mc by hand and for pressing the keys from
a script: a remote host with cases on it, and a container that builds this
tree and runs mc against it.

    tests/misc/docker/sandbox.sh debian-12 up     # images, remote host, mc -- a few minutes
    tests/misc/docker/sandbox.sh debian-12 mc     # mc against that environment
    tests/misc/docker/sandbox.sh debian-12 test   # press the keys in every cases.tsv
    tests/misc/docker/ci.sh debian-12             # every subject, as CI runs it
    tests/misc/docker/sandbox.sh ui               # the same, chosen from menus

The environment name may be left out; `debian-12` is the default, or whatever
`$MC_SANDBOX` says. `sandbox.sh` with no command lists the rest: `build` after
an edit, `check` to load every plugin and ask every protocol for a listing
without a terminal,
`shell`, `remote`, `logs`, `down`, `clean`, and `list` for what there is.

Sources are mounted read-only and copied inside the container, so a build
leaves nothing in the working tree and reuses its object files between runs.

## Layout

    sandbox.sh              the driver; it holds no list of environments
    ci.sh                   every subject in one run, for CI and before a push
    common/                 what an environment should not have to write again
      build-mc.sh           copy the tree in, configure, make, install
      check-plugins.sh      can every plugin the build installed be loaded
      features.ini          build profiles for build -f
      run-cases.sh          press the keys, read the screen, write the report
      ui.sh                 menus that compose a test command
      check-remote.sh       ask each protocol for a listing
      keymaps/              mc.keymap files for test -k
      remote/               the host that serves the cases: sshd, vsftpd, smbd
    envs/
      debian-12/            docker-compose.yml, Dockerfile.mc, README.md, expect.tsv
    cases/
      archives/fixtures.sh  the files and the cases.tsv of one subject
      editor/, terminal/    the same for mcedit6 and the embedded terminal
      cmdline/              the same for copy and paste on the command line
      struct/               the same for the mcstruct plugin
      panel/                the same for the panel: the quick filter, quick cd
      lua/                  the same for the viewers written in Lua
      sqlite/, arcmc/       the same for the sqlite plugin and arcmc.ini
    reports/                what a run leaves behind (not in git)

Three axes, chosen independently: the **environment** (which image mc is
built and run in), the **subject** (which cases), and how mc is run there
(**transport**, locale, ini values, keymap, build profile).

## Adding an environment

Add a directory under `envs/` with a `docker-compose.yml` in it. Nothing else
has to change: `sandbox.sh` finds environments by looking for that file, and
each is its own compose project with its own network, containers and build
volume, so an existing one is never touched or rebuilt because a new one
appeared.

One that only differs in what is installed -- an older distribution, fewer
tools, another shell -- is a `Dockerfile.mc` with a different `FROM` and the
same `COPY` lines from `common/`; `build-mc.sh` does not care which
distribution it is on. One that differs in how the far end behaves reuses the
image and changes `common/remote` through its own compose file.

Base images are pinned by digest, so a run is the same run next month; moving
to a newer image is a change to the Dockerfile.

Build contexts are the sandbox root, which is why the Dockerfiles refer to
`common/...`, `cases/...` and `envs/<name>/...`. The compose file sets
`SANDBOX_ENV`, which is how `run-cases.sh` finds the environment's
`expect.tsv`.

Environments publish no host ports, so several can run side by side; mc
reaches its host over the compose network by the name `remote`.

## What a subject contains

`cases/<subject>/fixtures.sh` builds a directory of files and, in each
subdirectory, a `cases.tsv` of file, key, expected outcome and reason -- a
checklist to read, and the columns `test` walks. The bytes are generated from
a seed, so sizes and screens are the same from run to run.

The remote host builds every subject under `/home/mc/cases/<subject>` and
serves it four ways, all as user `mc` with password `mc`: sftp and ssh on port
22, ftp on 21 (`/cases/<subject>`), and the samba share `cases`. The mc
container has the same tree in `/work/local/<subject>` for what needs no
server.

That remote tree is built when the image is, so a case that writes to it would
hand what it left to the next one. A subject whose fixtures are small enough to
build again says so with a `rebuild-remote` file next to them, and `test` runs
them over ssh before each case; `fileops` is one, because its cases copy into
the remote host.

### archives

| directory      | what it is for                                            |
|----------------|-----------------------------------------------------------|
| `01-formats`   | tar, zip and 7z, including one past libarchive's buffer    |
| `02-content`   | archives with no extension, and plain text named as one    |
| `03-nested`    | an archive inside an archive, and one inside `uzip://`     |
| `04-non-ascii` | Cyrillic and spaces in names, inside the archives and out  |

### editor

| directory     | what it is for                                              |
|---------------|-------------------------------------------------------------|
| `01-filter`   | the line filter: Alt-Shift-S by the word, Alt-S to lift it   |
| `02-charset`  | 8-bit files, and every byte CP866 draws, laid out as a table |
| `03-search`   | the search dialog opening with the marked word              |
| `04-paste`    | a bracketed paste: one Undo takes it back, a control byte in it runs nothing, no auto indent on top of its own |

### terminal

| directory  | what it is for                                                   |
|------------|------------------------------------------------------------------|
| `01-shell` | a command in the terminal, Ctrl-L against Ctrl-Alt-L, the keybar, insert mode in the shell's line |

### struct

| directory    | what it is for                                                  |
|--------------|------------------------------------------------------------------|
| `01-formats` | the smallest u-boot image and MBR libmagic still names, so that magic.ini sends them to mcstruct |

### cmdline

| directory | what it is for                                                        |
|-----------|-----------------------------------------------------------------------|
| `01-clip` | Ctrl-Insert and Shift-Insert on the command line: marked files, the line, the file under the cursor; a paste as one line, with the panels hidden, and the question over 2 KB |
| `02-paste` | a paste from the terminal: the shell runs nothing until Enter, with the panels up and after Ctrl-O, one line for a shell that did not ask for bracketed paste, a dialog input and quick search |

### panel

| directory       | what it is for                                             |
|-----------------|-------------------------------------------------------------|
| `01-filter`     | the quick filter, Ctrl-G, quick cd in the panel, the find dialog |
| `02-permissions`| files for a person to look at with Permission colors on: a captured screen carries no colour |
| `03-plugin-connect` | an ftp or sftp connection that does not come up: a refused login, a host that is not there |
| `04-mark`       | tagging files: Insert, Ctrl-T, Shift with the arrows, PgUp, PgDn, Home and End, a run of them, under the quick filter, with text on the command line |

### fileops

| directory   | what it is for                                                  |
|-------------|------------------------------------------------------------------|
| `01-copy`   | F5: what the dialog says, a target that is already there, a directory, and a target filesystem with no room left |
| `02-move`   | F6: the dialog, a rename in place, a move onto a file that exists |
| `03-delete` | F8: the question, what the panel shows afterwards, a directory that is not empty |
| `04-links`  | that a link is deleted and renamed as a link, and what it points at stays |
| `05-upload` | F5 and F6 into a plugin panel over ftp: a file, a directory with what is below it, an empty one |
| `06-plugin-delete` | F8 and F6 on a directory that is not empty, inside a plugin panel over ftp |

These change the files they work on, so the subject is built again before every
case that touched anything. The full filesystem is `/small`, a 64k tmpfs every
environment mounts for this.

What is not here: a file that cannot be read. mc runs as root in these
containers, and root reads everything; a case for it would pass without
proving anything.

### lua

| directory | what it is for                                                    |
|-----------|--------------------------------------------------------------------|
| `01-dbf`  | a dBase III table written byte by byte, which lua-dbf decodes itself and draws on `mc.ui.screen` |
| `02-elf`  | an ELF and a symbolic link to it, which lua-readelf runs `readelf` on |
| `03-image`| an 8x8 true colour PNG, drawn in chafa's characters, with `i` for the properties and F1 for the script's help |
| `04-markdown`| a short markdown file, rendered by lua-markdown into the viewer's nroff mode: headings, bold, a list, a link, a code block, a table, LaTeX symbols, paragraphs flowed to the width; F8 for the file itself |

lua-dbf and lua-markdown need nothing but the Lua runtime, lua-readelf needs `readelf` from
binutils, the picture needs `chafa`; the image carries `liblua5.4-dev` and
`chafa` for them.  magic.ini sends every image to the script that draws it in
sixel where the terminal can and in chafa's characters where it cannot: this
terminal cannot, so what the cases read is the characters.  A picture drawn in
sixel is not checked at all and cannot be: it reaches the terminal as a DCS the
screen library never sees.  The Java class viewer is left out with its tools,
which would cost the image a JRE.

The PNG is in true colour on purpose: chafa's loader turns down a paletted
one.

### shells

| directory     | what it is for                                                |
|---------------|----------------------------------------------------------------|
| `01-subshell` | that mc starts, that Ctrl-O gives a shell which runs a command, that a cd in that shell moves the panel, and that quitting mc leaves nothing running |

`cases/shells/shells.txt` names them: sh, bash, zsh, dash, busybox ash, mksh,
tcsh and fish. `test -s <name>` runs the subject under one of them and `ci.sh`
walks the whole list, one report each. A shell the image does not carry is
reported as not run rather than as a failure, so an environment carries the
shells it wants to answer for; debian-12 carries all of them.

### sqlite

| directory     | what it is for                                              |
|---------------|--------------------------------------------------------------|
| `01-tables`   | a database of two tables and a view: the tree down to one row as JSON, the schema at every level, and the same bytes under a name the rule does not know |

### arcmc

| directory           | what it is for                                        |
|---------------------|--------------------------------------------------------|
| `01-runtime-format` | a suffix that only `arcmc.ini` knows, opened with Ctrl+PgDn without a rebuild and without a magic.ini rule |

`cases/arcmc/config/arcmc.ini` is how it is registered: a subject may carry a
`config/` of its own, and what is in it is copied over mc's configuration
before the run.

These eight are local only: they press keys on mc itself, not on a file a
server holds.

**sftp** and **shell link** supply a stream, so an archive opens without being
downloaded first. `01-formats/big.7z` is the case that only works because the
stream can seek. **ftp** and **samba** have no `get_input_stream()` yet, so an
archive is fetched to a local copy first.

`02-content` is what happens when the name does not say: `magic.ini` knows
archives by extension, so an archive without one is left alone everywhere, and
plain text called `.tar.gz` gets an error from the operation that was asked to
open it.

## Pressing the keys

    sandbox.sh debian-12 test                          # archives, local panel
    sandbox.sh debian-12 test -w local,sftp,ftp,smb,sh # over every transport
    sandbox.sh debian-12 test -w sh 01-formats         # one directory
    sandbox.sh debian-12 test -l ru_RU.KOI8-R          # an 8-bit locale
    sandbox.sh debian-12 test -c editor -l ru_RU.CP866 # the DOS codepage
    sandbox.sh debian-12 test -c shells -s dash      # mc over another shell
    sandbox.sh debian-12 test -o old_esc_mode=true -k shift-tab-complete
    sandbox.sh debian-12 build -f all,ncurses && sandbox.sh debian-12 test

`run-cases.sh` starts mc under tmux in the case directory (through the
plugin's connection list for a remote one), finds the file by quick search,
presses the keys and reads what came of it. A `cases.tsv` row is file, keys,
expectation, reason, and optionally the transports it is for. The keys go
comma separated, in order: `Enter`, `F3`, `F5`, `C-o`, `..` (up one level),
`on <name>` (the cursor goes there), `cd <path>` (the Quick cd box),
`type <text>`, `key <name>` for anything tmux can send (`F4`, `M-S`, `C-M-l`,
`C-Insert`, `Escape`), `paste <text>` for a paste from the terminal (`\n` in
it is a line break; tmux sends it as a bracketed paste when mc asked for one),
and `width <n>` for a narrower terminal. The
expectations: `archive panel`, `listing`, `error dialog`, `nothing, no error`,
`the panel it came from`, `extfs panel`, `copy to the other panel` (the file
is then in `/tmp`, as big as mc said), `the name as written` (the shell
printed it), `text: <what the screen must show>`, `no text: <what it must
not>`, and `clipfile: <what mc copied>`, which is read from the file mc keeps
a copy in rather than off the screen. mc's stderr is read as well; an assertion
or a critical warning fails the case whatever the screen shows. A row with
keys or an expectation the script does not know is listed as skipped.

The sequences this terminal sends for a function key with a modifier are
written into `~/.config/mc6/term` before mc starts, so that `C-F1` and
`C-Insert` reach mc at all, and what mc saves between runs is thrown away
before each case, so one case does not hand the next the cursor position it
left in a file.

`-s` is the shell mc drives, out of what the image has. `-o` writes ini values before mc starts (`section.key=value`, the section
`Midnight-Commander` when left out), `-k` puts a keymap from `common/keymaps/`
in place, `-l` picks the locale mc runs in (messages stay English so that
the screen can be read). `build -f` picks a profile from
`common/features.ini`; each set of features has its own build and install
directory, and `test` runs the one built last.

An environment that expects something else -- no archiver installed, so
`small.7z` gives an error rather than a panel -- says so in its
`envs/<name>/expect.tsv`: `dir/file`, key, expectation, and optionally the
transports it applies to.

### Under valgrind

    sandbox.sh debian-12 build -f all,debug     # -O0 -g3, so a stack reads
    sandbox.sh debian-12 test -g 01-formats     # the same cases under memcheck

`-g` starts mc under `valgrind --tool=memcheck` with
`common/valgrind.supp`, which holds what the libraries never free and nothing
of mc's own.  Every wait is multiplied by six, `$SLOW` sets another factor,
and mc is asked to quit with F10 instead of being killed, because a killed
process writes no summary.  A case fails on an invalid read, write or free,
on a jump on an uninitialised value, whatever the screen shows; what was
definitely lost is written down and left to a person, since mc frees little
on the way out by design.

The log of each case is kept next to its screen as `<case>.<key>.valgrind`,
`<transport>/valgrind.tsv` counts them, and `index.md` gains a Memory table
with a link to every log worth opening.  Only `debian-12` has valgrind in its
image; `-g` elsewhere says so and stops.

A case takes minutes rather than seconds, so a run under memcheck is one
directory at a time, not the whole subject over every transport.

### Reports

Every run writes `reports/<stamp>-<env>/`: `index.md` with a table per
transport and the list of failures, and under `<transport>/` a `results.tsv`
(case, key, expectation, verdict, milliseconds, reason), the screen of every
failure, and mc's stderr per case. `index.md` is what goes into a release
issue.

## In CI

`ci.sh` walks every subject, one `sandbox.sh test` each, and writes them into
one report directory:

    tests/misc/docker/ci.sh debian-12                     # every subject, local panel
    tests/misc/docker/ci.sh debian-12 -w local,sftp,ftp,smb,sh
    tests/misc/docker/ci.sh debian-12 -c editor,panel -l ru_RU.KOI8-R

It takes the same arguments as `test` beyond `-c` and `-w`, which it spells
comma separated, and it does not stop at the first subject that fails: the
`index.md` it writes says which ones did, each row a link to that subject's
own report. A subject that could not be run at all -- no fixtures, a name
that is not there -- is told apart from one whose cases failed.

`.github/workflows/ci-sandbox.yml` is the job. On a push and on a pull request
it builds the image and mc, asks whether every plugin loads, and presses the
keys of every subject on a local panel. Over the network the same cases take
about three times as long, so the protocols run on a schedule, nightly,
and from `workflow_dispatch` when a person asks. A failing run keeps the whole
`reports/` directory as an artifact, screens of the failures included, and the
`index.md` goes into the job summary either way.

`$SLOW` multiplies every wait; the job sets it to 2, because a runner is
slower than the machine the waits were written on and a wait that runs out
fails a case that would have passed.

## Poking at it by hand

    sandbox.sh debian-12 remote    # the remote host
    sandbox.sh debian-12 shell     # the build container
    sandbox.sh debian-12 clean     # remove containers and the build

Or directly, if mc is already built:

    docker run --rm -it --network mc-sandbox-debian-12_default \
        -v mc-sandbox-debian-12_work:/work mc-sandbox-debian-12-mc /work/opt/mc/bin/mcommander

The `mc` container has `ssh`, `curl` and `smbclient`, so a transfer can be
watched from outside mc as well.
