# far2l clipboard

With the extensions on and no `clipboard_store` (or `clipboard_paste`) command in
`[Misc]`, a copy goes to the clipboard of the far2l terminal and a paste (Shift-Insert,
the editor's and the command line's) reads it. A command that is set is used as before.

- The calls are clipboard requests of the far2l protocol: open, then set or get of text,
  then close. The terminal asks its user whether M-Commander may use the clipboard; a
  refusal, no text, or no answer within 15 seconds leaves the clipfile as it was.
- The text taken from the terminal goes to the clipfile like the output of
  `clipboard_paste`; at most 4 MiB are taken or given.
- Keys typed while the terminal answers are read again afterwards.
- Nothing is sent when the extensions are off (`MC_FAR2L=0`, `screen*`/`tmux*`, no answer
  to the question at start).
- The tests in `tests/src/tty_far2l_clipboard.c` play the terminal.

Security: the far2l terminal, not M-Commander, decides. It shows its own confirmation
before it lets a client set or read the clipboard, and M-Commander asks for the clipboard
only when far2l was negotiated (`far2lok`) and neither `clipboard_store` nor
`clipboard_paste` is set. The clipboard is read only by an explicit paste command of the
user (Shift-Insert and the paste of the editor, input lines and text areas), never by a
program in the terminal, never at start and never on a timer; a request that is not
answered within 15 seconds is given up. This is not OSC 52: nothing here lets a program
that merely writes to the terminal read the clipboard.
