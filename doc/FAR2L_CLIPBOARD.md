# far2l clipboard

The far2l terminal extensions carry the clipboard both ways: M-Commander uses the
clipboard of a far2l terminal it runs in, and its own terminal (mcterm) gives a clipboard
to the programs in it that ask for one the far2l way. The protocol is described in
`VTExts.md` of far2l (https://github.com/elfmz/far2l/pull/3082); this is what M-Commander
does with it.

## M-Commander in a far2l terminal

With the extensions on and no `clipboard_store` (or `clipboard_paste`) command in
`[Misc]`, a copy goes to the clipboard of the far2l terminal and a paste (Shift-Insert,
the editor's and the command line's) reads it. A command that is set is used as before.

- The calls are clipboard requests of the far2l protocol: open, then set or get of text,
  then close. The terminal asks its user whether M-Commander may use the clipboard; a
  refusal, no text, or no answer within 15 seconds leaves the clipfile as it was. After
  an open that got no answer M-Commander closes the clipboard anyway, so that a late
  "yes" does not leave it open.
- M-Commander identifies itself by a client ID of its own, the passcode the terminal
  keeps the user's answer for. It is made once, from the host name and random letters
  and digits, and kept in `far2l-client-id` of the data directory
  (`~/.local/share/mc6/` by default), so "always allow" lasts, and another program can
  not take M-Commander's answer by naming a well-known ID.
- If the user tells the terminal that M-Commander is to use its own clipboard, it does,
  and the terminal is not asked again while M-Commander runs.
- The text taken from the terminal goes to the clipfile like the output of
  `clipboard_paste`; at most 4 MiB are taken or given.
- Keys typed while the terminal answers are read again afterwards, also keys that come
  as packets of the extensions.
- Nothing is sent when the extensions are off (`MC_FAR2L=0`, `screen*`/`tmux*`, no answer
  to the question at start).
- The tests in `tests/src/tty_far2l_clipboard.c` play the terminal.

## The programs in M-Commander's terminal

A program in mcterm that has the extensions on (far2l, f4, M-Commander itself) gets a
clipboard from mcterm as it would from a far2l terminal. It is M-Commander's own
clipboard, whatever that is: the clipfile, the `clipboard_store` and `clipboard_paste`
commands, OSC 52 to the terminal outside, or the clipboard of a far2l terminal outside.
None of it needs a far2l terminal outside: mcterm gives the extensions to its programs
in any terminal, unless `MC_FAR2L=0`.

- The first time a client opens the clipboard M-Commander asks the user: Block, Own
  clipboard, Allow, Allow always. Block fails the open, Own clipboard tells the program
  to keep to its own, Allow lets the client in while the program has the extensions on,
  and Allow always also writes its client ID to `far2l-clients` of the data directory.
  The first button is the safe one, so a hasty Enter blocks. The question is asked from
  the idle loop, never in the middle of the program's output, and the program waits
  for the answer; a program that gives the extensions up meanwhile gets none.
- The clipboard is read only for 5 seconds after the user pressed Ctrl+V (with Shift or
  without) or Shift+Insert in this terminal. Each read lets the next one come as late,
  at most 3 times; a close ends that. A read at any other moment gets no data, exactly
  as if the clipboard were empty: the program does not learn that it was refused.
  Writing needs no key, as in far2l.
- Text is `CF_TEXT` (UTF-8) and `CF_UNICODETEXT` (UTF-32), both M-Commander's text.
  Formats registered by name (far2l's vertical block, for one) are kept for as long as
  M-Commander runs. Data may come in chunks, and every piece of data has an ID, as far2l
  gives them, up to 16 MiB.
- Opens do not nest, requests before the extensions are on are not answered, and every
  request with an ID gets an answer, an empty one for what mcterm has not got, so a
  program never waits for nothing.
- The tests in `tests/src/mcterm_far2l.c` play the program.

## Security, and OSC 52

A program that can read the clipboard can take what the user copied last, a password
for one, without the user noticing. OSC 52 has such a read form
(`ESC ] 52 ; c ; ? BEL`): any program in the terminal, also a file shown with `cat`
or a command run on a remote host, may ask, and the terminal answers. That is why
terminals turn OSC 52 reading off or ask every time, and M-Commander's terminal never
answers it (`clipboard_write` lets a program set the clipboard by OSC 52, never read
it).

The far2l extensions guard the clipboard twice over, and M-Commander keeps both, as a
client and as a terminal:

1. A client has to be allowed by the user, by its client ID: a random passcode the
   program keeps, not something every program knows.
2. The clipboard is read only right after the user's own paste key, which the terminal
   sees on its way to the program. A program that reads at another moment gets nothing.

Writing is allowed once the client is, as in far2l. M-Commander as a client reads the
clipboard of the terminal only on an explicit paste command of the user, never at start
and never on a timer.
