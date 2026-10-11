# Git panel plugin <!-- help:notitle -->

```
  Git Panel Plugin

  Navigation & Structure:
    Outside Git repo : Shows the favorites list.
    /branches        : Local and remote branches.
    /commits         : Commit history (parent commits act as directories).

  Keys (Repository Views):
    Alt-G      - Open this repository in coole (Git window)
    Alt-C      - Open coole's full Git workspace to write a commit message
    Alt-A      - Amend the last commit in coole
    Ctrl-R     - Refresh panel

  Keys (Status View):
    Shift-F3   - Show diff for file (in commit/status view)
    Shift-F5   - Stage file(s)
    Shift-F6   - Unstage file(s)
    Shift-F8   - Discard changes (reset to HEAD)
    Ctrl-B     - Add current repo to favorites

  Keys (Commit History):
    F3         - View selected commit details in MC
    F4         - Reword selected commit in coole (current branch only)
    Alt-G      - Open coole on the log
    Alt-V      - Open selected commit in coole
    Alt-R      - Reword selected commit in coole (also works inside its file list)

  Keys (Favorites View):
    Enter      - Open repository
    Shift-F4   - Add repository to favorites (by path)
    F8         - Remove from favorites
    Ctrl-R     - Refresh panel
```

Alt-C runs `coole -G <repository>`: the message editor, status lists and diff
stay open after a commit. From a status or staged list in coole, F2 moves to the
message for a new commit; F3 opens amend from the list or the new message editor.
F2 in the message editor performs the action named on its button. If nothing
is staged, coole can offer to stage all changes. Alt-A
opens the amend message directly with `--git-amend`. Alt-V and Alt-R use
`--git-show=<commit>` and `--git-reword=<commit>`.
Saving a message in coole performs the Git operation. Reword
recreates the selected commit and its descendants, changing their IDs and
removing their signatures. After a successful reword, the panel returns to
the current branch's commit history.

Set `coole_executable` in `[git-panel]` of `panels.git.ini` to use a different
coole binary. The new hotkeys are configurable there as `hotkey_coole`,
`hotkey_commit`, `hotkey_amend`, `hotkey_show` and `hotkey_reword`.
