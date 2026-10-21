# GitHub Plugin <a id="github-plugin"></a>

The GitHub panel shows the pull requests and the issues of a GitHub
repository as a tree of directories. It asks the `gh` command for everything,
so it works with the accounts and the hosts `gh` is logged in to: run
`gh auth login` once before opening the panel. Nothing in the panel changes a
repository; it only reads.

Opened from a directory of a repository whose remote is on GitHub, the panel
starts at that repository, the one `gh` itself takes there: the default set by
`gh repo set-default`, else the remote. Opened anywhere else, it starts at
`gh:/`.

**The tree**

```
gh:/                         the owners
gh:/<owner>/                 the repositories of one
gh:/<owner>/<repo>/          pulls, issues, repo.md
  pulls/                     the pull requests
    <number> <title>/        the files of one
      files/                 the files it changes
      commits/               its commits
      conversation/          its comments and reviews
  issues/                    the issues
    <number> <title>/        the files of one
      conversation/          its comments
```

The address of a pull request is its number, *gh:/owner/repo/pulls/433*; the
title after the number in the panel only names it.

*gh:/*
: The account `gh` is logged in as, the owner of the repository the panel
was opened from, and the favorites. The Origin column says which of them it
is. A favorite owner is listed as an owner; a favorite place below one goes by
its path with colons for the slashes, *MidnightCommander:mc:pulls*, and Enter
opens it.

*gh:/owner/*
: The repositories of the owner, as `gh repo list` gives them, newest first.

*repo.md*
: What `gh repo view` shows: the description and the README.

*pulls/*, *issues/*
: The open pull requests or issues, newest first. **Alt-F** shows the closed
ones, then (pull requests only) the merged ones, then all of them. The footer
says which ones are shown and how many.

**The files of a pull request**

*description.md*
: The title, the state, the author, the branches, the review decision, the
labels and the description.

*files/*
: The files the pull request changes, by their path in the repository, with
what happens to them (added, modified, removed, renamed) and how many lines
come and go. They are compared from where the branch of the pull request left
the base, as GitHub's own page compares them, so what came into the base since
does not show. **Shift-F3** opens the two versions in the diff viewer, the
base on the left and the pull request on the right, both to look at only; an
added or a removed file has an empty side, a renamed one its old name on the
left. **F3** shows the file as the pull request has it, and Quick View the
lines that change, which come with the list. The versions are fetched from
GitHub the first time they are needed and kept while the pull request is
open: no clone is needed. GitHub lists 300 files at most; the footer says
so when there are more.

*commits/*
: The commits, newest first, each as the start of its hash and its headline.
F3 and Quick View show the commit as a patch: its message and what it
changes. Alt-W opens it on GitHub.

*conversation/*
: The comments and the reviews, one file each, numbered from the oldest:
*001 zyv commented.md*, *002 egmontkob approved.md*. A review is named by what
it decided: approved, changes_requested, commented, dismissed.

*diff.patch*
: The changes, as `gh pr diff` gives them.

*checks.txt*
: The checks and their results, as `gh pr checks` gives them.

An issue has *description.md* and *conversation/*, with its comments.

**Keys**

**Enter**
: Go into a directory. On a file, view it.

**F3**
: On a pull request, an issue or a repository, view its description. On a
file, view the file.

**Ctrl-R**
: Ask GitHub again. A file is fetched once and kept while its directory is
open; Ctrl-R fetches it again too.

**Alt-F**
: In *pulls/* or *issues/*, change which ones are shown.

**Alt-W**
: Open the entry under the cursor in the browser, through `gh --web`: a
repository, a pull request or an issue. Elsewhere it opens the repository or
the pull request or issue the panel is in.

**Alt-G**
: Open the local clone of the repository the panel is in, in the Git panel
on the other side: from the repository, its pull requests or issues, or from
inside one of them. The panel knows the clone when it was opened from it once;
otherwise Alt-G asks where the clone is, checks with `gh` that it is a clone of
this repository, and remembers it.

**Ctrl-B**
: Add the place the panel is in to the favorites: an owner, a repository,
*pulls/*, *issues/* or one pull request or issue. In *gh:/* it adds the owner
under the cursor.

**Shift-F4**
: Add a favorite by typing it: an owner, or a path below one such as
*MidnightCommander/mc/issues*. The address of a page on GitHub does too:
*https://github.com/MidnightCommander/mc/pull/5195* is saved as
*MidnightCommander/mc/pulls/5195*. The place the panel is in is filled in, to
be saved as it is or changed. In *gh:/*, **F7** does the same.

**F4**
: In *gh:/*, change the path of the favorite under the cursor, in the same
way as it is typed for Shift-F4.

**F8**
: In *gh:/*, remove a favorite. Nothing on GitHub is removed.

Quick View shows the description of the pull request or the issue under the
cursor without asking GitHub: the list brings it along. A file is fetched the
first time the cursor stops on it.

**Settings**

The settings are read from *~/.config/mc6/panels.gh.ini*:

```
[gh-panel]
gh=gh
limit=100
hotkey_state=alt-f
hotkey_web=alt-w
hotkey_fav_add=ctrl-b
hotkey_git=alt-g
hotkey_diff=shift-f3
```

*gh*
: The `gh` command to run.

*limit*
: How many pull requests, issues or repositories a list asks for, at most
1000.

*hotkey_state*, *hotkey_web*, *hotkey_fav_add*, *hotkey_git*, *hotkey_diff*
: The keys of Alt-F, Alt-W, Ctrl-B, Alt-G and Shift-F3; *none* turns a key
off.

The favorites are kept in the same file, under *[gh-favorites]*, as paths
without *gh:/*; the local clones under *[gh-clones]*, one *owner/repo=directory*
each.
