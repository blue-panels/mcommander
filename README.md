# M-Commander

M-Commander, based on [GNU Midnight Commander](https://midnight-commander.org)
version 4.8.33.

M-Commander is a text-mode, full-screen file manager: two panels, a built-in
editor and viewer, and an embedded terminal. The viewer has a structured tree
mode, and mcstruct is a structured binary viewer and editor. The core is small:
everything beyond it is optional and comes as a plugin loaded at run time,
from archives and git to docker, Kubernetes, S3, FTP, SFTP and Samba.
It runs in any terminal, from the Linux console to tmux and a remote session over ssh.

This fork is not an official GNU package. Report issues here, not upstream.
Its version numbering starts at `v6.0.1` and is independent of upstream.

![Git panel with inline diff](https://raw.githubusercontent.com/wiki/blue-panels/mcommander/assets/git-panel.gif)

## What this fork adds

Full notes: **[Releases wiki](https://github.com/blue-panels/mcommander/wiki/Releases)**.

- **Panel plugins.** Panel contents can come from a dynamically loaded plugin.
  Shipped: git, docker, Kubernetes, MongoDB, S3, FTP/FTPS, SFTP, Samba, systemd,
  shell connections, External Panelize, arcmc and mcstruct. The old built-in `ftpfs` and
  `sftpfs` VFS modules are replaced by the FTP and SFTP plugins.
- **arcmc** - an archive manager on libarchive: browse, create, pack and extract
  (zip, 7z, tar.\*, cpio) with progress and cancel. The legacy built-in `tarfs`
  and `cpiofs` VFS modules have been removed. See the
  [arcmc documentation](src/panel-plugins/arcmc/README.md) for external archive
  formats and helpers.

  ![arcmc](https://raw.githubusercontent.com/wiki/blue-panels/mcommander/assets/arcmc.gif)
- **mcstruct**: a structured binary viewer and editor, a text def-file (STL5,
  compatible with the DOS Struct Look) turns a firmware image, a header or a
  table file into a tree of named fields, synced with a hex dump; fields are
  edited in place. Shipped def-files for ELF, PE, ZIP, DBF, MBR, FAT, uImage,
  DTB, PNG, BMP and WAV. Start it with `mcstruct FILE`, Shift-F4 in the viewer,
  or F3 through `magic.ini`. See the
  [mcstruct guide](https://github.com/blue-panels/mcommander/wiki/Mcstruct).

  ![mcstruct](https://raw.githubusercontent.com/wiki/blue-panels/mcommander/assets/mcstruct.gif)
- **Editor** - code folding, an undo history browser, a macro explorer, and an
  editor plugin framework.
- **Viewer** - a structured tree mode for JSON, YAML, XML and HTML, a grep-style
  live filter, ANSI colour and terminal replay, and streaming of never-ending
  command output.

  ![Structured tree viewer](https://raw.githubusercontent.com/wiki/blue-panels/mcommander/assets/viewer-tree.gif)

  ![Grep-style live filter](https://raw.githubusercontent.com/wiki/blue-panels/mcommander/assets/viewer-filter.gif)
- **Embedded terminal** - run a shell inside the file manager, panels stay in
  sync with its directory.
- **Panels** - user-editable view modes, dialogs for managing key bindings and
  learning terminal keys, and the classic hide-a-panel / run-a-command flow.

  ![Hide a panel, run a command](https://raw.githubusercontent.com/wiki/blue-panels/mcommander/assets/panel-hide.gif)

## Building

See [`INSTALL`](INSTALL) for dependencies and instructions.

```sh
./autogen.sh          # from a git checkout
./configure
make
sudo make install
```

`mcommander --version` identifies this fork.

## Packages

This fork is packaged as **`mcommander`**, and its main command is `mcommander`,
with `mc6` as a short alias for it.  The others are `mcedit6`, `mview`,
`mcdiff6`, `mctree` and `mcstruct`.  It installs beside the
distribution `mc` package instead of replacing it.

There is no public package repository yet.  Each release builds `.deb`, `.rpm`
and `.pkg.tar.zst` and attaches them to its GitHub release; to build them
yourself, see [`packaging/README.md`](packaging/README.md).  The `.deb` assets
are built separately for Debian Trixie and Ubuntu 26.04; install only the one
whose distribution suffix matches your system.  The recipes carry no version
of their own: `packaging/prepare.sh <version>` generates the version-bearing
files from the release tag, and every build starts with it.

- Debian Trixie: `sudo apt install ./mcommander_*~debian13*.deb ./mcommander-plugins_*~debian13*.deb ./mcommander-lua_*~debian13*.deb`
- Ubuntu 26.04: `sudo apt install ./mcommander_*~ubuntu26*.deb ./mcommander-plugins_*~ubuntu26*.deb ./mcommander-lua_*~ubuntu26*.deb`
- RHEL/Fedora: `sudo dnf install ./mcommander-*.rpm ./mcommander-plugins-*.rpm ./mcommander-lua-*.rpm`
- Arch: `sudo pacman -U ./mcommander-*.pkg.tar.zst ./mcommander-plugins-*.pkg.tar.zst ./mcommander-lua-*.pkg.tar.zst`
- Gentoo: copy `packaging/gentoo` into a local overlay as `app-misc/mcommander`,
  then `emerge app-misc/mcommander`

Install downloaded packages through the package manager -- `apt install
./file.deb` and `dnf install ./file.rpm` -- never `dpkg -i` or `rpm -i`, which
skip the dependency and replacement handling.

## Documentation

- [Wiki](https://github.com/blue-panels/mcommander/wiki)
- Built-in help: press `F1` inside mcommander
- Manual pages: `mcommander(1)`, `mcedit6(1)`, `mview(1)`

## Translations

[![Translation status](https://translate.codeberg.org/widget/mc6/mc6/svg-badge.svg)](https://translate.codeberg.org/engage/mc6/)

Program strings are translated on Codeberg Weblate:
<https://translate.codeberg.org/engage/mc6/>

Sign in with a Codeberg account and start translating, no other setup is
needed. A maintainer brings the finished translations into `po/`, so please
do not open pull requests that edit `po/*.po` by hand.

## Reporting problems

Open an issue: <https://github.com/blue-panels/mcommander/issues>

Include `mcommander --version`, your OS and distribution, and the compiler and
configure flags if you know them. For a crash, attach a `gdb` backtrace
(`gdb mcommander core`, then `where`).

## License

GNU General Public License, version 3 or any later version. See
[`COPYING`](COPYING).
