# Packaging mcommander

`mcommander` is the package name for this fork.  It installs `mcommander` and
the `mc6`, `mcedit6`, `mview`, `mcdiff6`, `mctree` and `mcstruct` symbolic links,
under paths of its own, so it can be installed beside the distribution `mc`.

Three packages come out of it: `mcommander`, `mcommander-plugins` with the panel
plugins, and `mcommander-lua` with the Lua runtime plugin and its scripts.
Gentoo builds one package and puts Lua behind the `lua` USE flag.

The recipes replace `mc6`, the name this fork was packaged under before.

## Nothing to edit for a release

No recipe here names a version.  The version comes from the release tag, and
`packaging/prepare.sh` writes the files that have to name one:

| Generated | From |
| --- | --- |
| `debian/changelog` | the `v*` tags |
| `packaging/rpm/mcommander.spec` | `mcommander.spec.in` and the `v*` tags |
| `packaging/arch/PKGBUILD` | `PKGBUILD.in` |
| `packaging/gentoo/mcommander-<version>.ebuild` | `mcommander.ebuild.in` |

All four are in `.gitignore`.  Making a release is therefore: tag, and let the
`release` workflow build.  Nothing is committed afterwards.

Both changelogs take their entries from the **annotated tag message** and their
dates from the tag, so the same tag always yields the same source package.
What a release says is written once, when the tag is made, and it is read by
whoever installs the package:

```sh
git tag -a v6.0.3 -m "Filtered view in mcview.
Faster paste into the editor."
```

Sign it with `-s` instead if you keep a signing key; the packaging reads the
message and the date, and depends on no signature.

A version with no tag of its own is a test build.  Its Debian entry is marked
`UNRELEASED`, which stops `dput` from taking it by accident.

Only release versions are accepted, `6.0.3` and the like.  A pre-release has to
sort *below* the release it leads to, and the only character that does that is a
tilde, which Git does not allow in a tag name.  Mapping `6.0.2-rc1` to
`6.0.2~rc1` would also mean renaming the orig tarball and its directory to
match, and Arch allows neither the tilde nor the hyphen in `pkgver`.  Until that
is built, `prepare.sh` refuses such a version rather than quietly producing a
package that outranks the release.  To rehearse the release workflow, run it
from Actions against a tag that already exists.

The build dependency lists intentionally enable every shipped panel plugin.
Do not replace their exact core-version dependencies with `>=`: the plugin ABI
is not stable.

## Release source

`packaging/release-source.sh` creates the archive that every recipe consumes,
and that is uploaded as the GitHub release asset `mcommander-<version>.tar.gz`:

```sh
packaging/release-source.sh 6.0.3 v6.0.3
```

The archive is **bootstrapped**: `autogen.sh` runs inside it, so it carries
`configure`, the `Makefile.in` files, `po/Makefile.in.in` and `po/POTFILES.in`.
None of those is in Git, and no recipe can produce them all for itself:
`autoreconf` needs `autopoint` for `po/Makefile.in.in`, and `po/POTFILES.in`
comes from the `xgettext` pass in `autogen.sh`.  What is published is a release
tarball, not a snapshot of the tree, and the recipes only configure and build.
The script also adds the generated `mc-version.h`, so a build outside a Git
checkout reports the correct fork version, and it refuses to overwrite an
existing archive.

Because the bootstrap ran, the bytes depend on the autoconf, automake, libtool
and gettext versions that produced them.  Take a checksum from the archive that
was published, not from one rebuilt elsewhere.

`debian/` and `packaging/` carry `export-ignore` in `.gitattributes` and are
absent from the archive: the recipes are used from the repository, and a Debian
orig tarball must not carry a `debian` directory of its own.

## Architectures

Debian, Ubuntu and Fedora are built for `amd64` and `arm64`, each on a runner of
its own architecture.  The containers are multi-architecture, so both run the
same recipe; emulation is far too slow for a full build.  The two produce the
same source package and source RPM, and only one copy of each is attached to the
release.

Arch stays `x86_64`.  Arch Linux has no aarch64 port, and the `archlinux` image
is published for `x86_64` alone; the `PKGBUILD` still names `aarch64` for Arch
Linux ARM, which builds from its own recipes.  macOS is built for Apple Silicon.

A PPA builds its own binaries, so the architectures it produces are set in the
Launchpad settings of the PPA and not here.

## Building by hand

The `release` workflow does all of this on a tag push, in a container per
distribution.  Locally the same steps check that a distribution can still build
the package.

Debian and Ubuntu.  The source package is built from the release archive with
`debian/` copied into it, not from the Git checkout: that keeps it free of
patches, since the archive carries `mc-version.h` and no `debian` directory.
Build a binary package in its target distribution: the MongoDB plugin links to
the distribution's Mongo C Driver ABI, so a Debian-built `.deb` is not a
substitute for an Ubuntu-built one.

```sh
packaging/release-source.sh 6.0.3 v6.0.3
packaging/prepare.sh 6.1.0 dist/mcommander-6.1.0.tar.gz
tar -xf dist/mcommander-6.1.0.tar.gz -C /tmp
cp dist/mcommander-6.1.0.tar.gz /tmp/mcommander_6.1.0.orig.tar.gz
cp -r debian /tmp/mcommander-6.1.0/
cd /tmp/mcommander-6.1.0 && dpkg-buildpackage -us -uc
```

`sudo apt install ../mcommander_*.deb ../mcommander-plugins_*.deb
../mcommander-lua_*.deb` installs the result; use `apt install ./file.deb`,
never `dpkg -i`.  For a repository or PPA users install `mcommander`,
`mcommander-plugins` and `mcommander-lua` with apt; the transition removes
`mc6` and `mc-data`, and `sudo apt purge mc mc-data` later clears old conffile
records.  For a PPA, set the target series and a series suffix:

```sh
DEB_DISTRIBUTION=noble DEB_VERSION_SUFFIX='~ubuntu24.04.1' \
    packaging/prepare.sh 6.1.0 dist/mcommander-6.1.0.tar.gz
```

## Launchpad PPA

A PPA builds the binaries itself, so what it takes is a signed **source**
package per Ubuntu series.  `packaging/ppa-source.sh` builds them:

```sh
packaging/release-source.sh 6.0.3 v6.0.3
packaging/ppa-source.sh 6.0.3 dist/mcommander-6.1.0.tar.gz resolute:26.04 noble:24.04 jammy:22.04
UPLOAD=yes packaging/ppa-source.sh 6.0.3 dist/mcommander-6.1.0.tar.gz noble:24.04
```

`NOSIGN=yes` builds unsigned packages for a dry run, `SIGN_KEY` picks the key,
`PPA` the target (default `ppa:bluepanels/mcommander`).

`PPA_REVISION` is the number after the series, `1` by default.  A PPA keeps
every version it has ever accepted, so an upload that was rejected or turned out
broken comes back as `~ubuntu24.04.2`; the same version is never accepted twice.

Each series is named with its Ubuntu version, because that version is what
orders the uploads: `~ubuntu24.04.1` sorts above `~ubuntu22.04.1`, so moving to
a newer series is an upgrade.  Codenames cannot do that, having wrapped the
alphabet in 2017.

The series lives in the Debian revision, not in the upstream version, so all
series of a release share one `mcommander_<version>.orig.tar.gz`.  Only the first
upload carries it; the rest refer to the one already in the PPA.  Use one
archive for every series of a release, the published one: Launchpad compares
what a later upload refers to against the copy it already has.

Before the first upload: register the OpenPGP key with Launchpad and confirm it
through the encrypted mail it sends, then create the PPA.  `debhelper-compat
(= 13)` needs the series to be 22.04 or newer.

RPM.  Put the archive in the RPM source directory:

```sh
packaging/prepare.sh 6.1.0 dist/mcommander-6.1.0.tar.gz
cp dist/mcommander-6.1.0.tar.gz ~/rpmbuild/SOURCES/
rpmbuild -ba packaging/rpm/mcommander.spec
```

`Conflicts` is deliberately used instead of `Obsoletes`, so `dnf upgrade` does
not silently replace a distribution package.  The repository instruction is
`sudo dnf install mcommander mcommander-plugins mcommander-lua`; for a downloaded RPM use
`dnf install ./mcommander-*.rpm`, never `rpm -i`.

Arch.  `makepkg` finds the archive next to the `PKGBUILD` instead of
downloading it:

```sh
packaging/prepare.sh 6.1.0 dist/mcommander-6.1.0.tar.gz
cp dist/mcommander-6.1.0.tar.gz packaging/arch/
cd packaging/arch && makepkg -si
```

From a repository or the AUR the same two names are installed with
`sudo pacman -S mcommander mcommander-plugins mcommander-lua`.

Gentoo.  Copy `packaging/gentoo` into a personal overlay as `app-misc/mcommander`,
together with the generated ebuild, and run `ebuild ... manifest`.  It is a
single package; panel plugins are controlled by USE flags, and the strong
blocker requires an explicit migration from `app-misc/mc`:

```sh
emerge app-misc/mcommander
```

Termux is the terminal of Android, and `pkg install` takes its packages from the
repository of `termux-packages`, which builds them with the Android NDK against
Bionic, the libc of Android.  `packaging/termux/build.sh` is the recipe that
repository wants, made from `build.sh.in` by `prepare.sh` like the others, with
the version, the release archive and its SHA-256:

```sh
packaging/prepare.sh 6.1.0 dist/mcommander-6.1.0.tar.gz
```

What it submits is that file, as `packages/mcommander/build.sh` of a pull request
to <https://github.com/termux/termux-packages>.  `packaging/termux/build-deb.sh`
builds the package the way that repository does, in the Docker image of its
package builder, and makes `mcommander_VERSION_ARCH.deb` for `aarch64`, `arm`,
`x86_64` or `i686`:

```sh
packaging/termux/build-deb.sh aarch64 dist/mcommander-6.1.0.tar.gz
```

Without an archive it makes one from the checked-out commit.  The package is
installed in Termux with `pkg install ./mcommander_6.1.0_aarch64.deb`, and
`packaging/termux/smoke.sh termux-out` installs and runs the x86_64 one in
`termux/termux-docker`.  The screen library is S-Lang, as in the other
packages; Samba and the Mongo driver are not in Termux, so their panels are not
built.  The `release` workflow builds `aarch64`, `arm` and `x86_64` and attaches
them to the release; the `CI` workflow builds `aarch64` and tries `x86_64`.

Gentoo is the one distribution the workflow does not build: that needs a full
stage3 with a Portage snapshot, which costs more than the ebuild is worth.  It
stays a manual check.
