# far2l keyboard extensions

M-Commander can take its keys from a terminal that speaks the far2l terminal
extensions (far2l's own terminal, and f4's). Such a terminal reports every key
with its virtual key code, character and Windows control key state, so the
combinations that collide in the legacy byte stream (Ctrl-Enter and Enter,
Shift-Tab, Ctrl-Shift-letter, Alt with a function key, and so on) are told apart.

## Negotiation

- At start M-Commander sends `ESC _ far2l1 ESC \` together with its other terminal
  questions, before Primary Device Attributes. A terminal with the extensions
  answers `ESC _ far2lok ESC \` (or ends it with BEL). Every other terminal
  ignores the APC and behaves as before.
- No question is asked when `TERM` is `screen*` or `tmux*`, or when `MC_FAR2L=0`
  is set.
- Whenever M-Commander gives the terminal to a subshell or a child program
  (Ctrl-O, running a command, an external editor or viewer) it sends
  `ESC _ far2l0 ESC \` first, and `ESC _ far2l1 ESC \` when it takes the terminal
  back and at start; the same goes for the exit. The child never gets a
  terminal that is still in far2l mode.
- With the extensions on, M-Commander does not turn on the kitty keyboard
  protocol or Win32 input mode: they would only carry the same information twice.
  A terminal without the extensions keeps using whichever of the two it knows.

## Keys

A key is `ESC _ f2l <base64> ESC \` (or BEL as the terminator). The payload is a
stack popped from the end, values little endian, the last byte the command:

| command | fields, in the order they are popped |
| --- | --- |
| `K` / `k` | character u32, control key state u32, scan code u16, virtual key u16, repeat count u16 |
| `C` / `c` | character u16, control key state u16, virtual key u8 |

Capital is a key press, small a release. A press is turned into a key by the same
rules as a Win32 input mode record (see the Win32 input mode change), and a release
is no key. Replies, mouse and resize packets are read and dropped; the mouse keeps
coming as the xterm mouse reports M-Commander asks for. A character outside ASCII
goes back to the keyboard as UTF-8 and is read as a typed character.

## Plugin authors

Nothing changes for plugins: a key is the same integer code as before, whichever
way the terminal reported it, and a terminal without the extensions behaves exactly
as it did.

## Checking by hand

The tests in `tests/src/tty_far2l_input.c` replay packets through the real decoder.
By hand: in far2l's terminal or f4 run `mcommander`, press Ctrl-Enter, Shift-Tab and
Alt-F4 in the panels and see that each does its own action; run with
`MC_FAR2L=0` to compare with the legacy stream.
