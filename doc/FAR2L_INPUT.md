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
is no key. Replies and resize packets are read and dropped.

## Mouse

far2l's terminal sends its mouse only as `M` packets (and the compact `m`) once the
extensions are on, never as xterm reports. M-Commander turns a packet into the SGR
report (`ESC [ < b ; x ; y M`) it asks for from other terminals and hands it back to
the keyboard, so the mouse code has one input: presses, releases, wheel turns and,
in button-event tracking, moves with a button held. Moves with nothing held and
horizontal wheel turns are not reported, and nothing is if the mouse is off.

A character outside ASCII in a key packet goes back to the keyboard as UTF-8 and is
read as a typed character.

## The embedded terminal

A program run in mcterm (Ctrl-O's shell, or f4 or far2l inside it) is a program on a
terminal of its own, the emulator, and the two sides are kept apart:

- The negotiation of M-Commander with the terminal it runs in never reaches the
  program, and the program's never reaches that terminal. The emulator takes
  `ESC _ ... ST` strings itself (before, their text was drawn on the screen).
- A program that asks with `ESC _ far2l1 ST` is answered `ESC _ far2lok ST` only when
  the terminal M-Commander runs in has the extensions; otherwise it gets no answer, as
  on any terminal without them. `far2l0` gives them up again, and the mode ends with
  the emulator's reset.
- With the extensions on, the keys M-Commander hands to the program are packets
  (`ESC _ f2l:<base64> BEL`, a press and a release each) with the virtual key, the
  character and the control key state, so the program tells Ctrl-Enter from Enter and
  Shift-Tab from Tab as far as M-Commander did. A byte of a character out of ASCII
  and a key without a virtual key go as before, in xterm form.
- The other requests of the extensions (clipboard, images) are not passed on; a program
  that asks gets no answer and falls back, as it does on a terminal without them. Drag
  and drop is passed on, see below.

## Which input is active

Help, About shows `Keyboard input:` with `far2l`, `kitty`, `win32` or `legacy`: the
one the keys come in now. Terminal reports can name it, and it tells at once which of
`MC_FAR2L=0`, `MC_KITTY_KEYBOARD=0` and `MC_WIN32_INPUT=0` to try.

## Plugin authors

Nothing changes for plugins: a key is the same integer code as before, whichever
way the terminal reported it, and a terminal without the extensions behaves exactly
as it did.

## Checking by hand

The tests replay packets through the real decoder (`tests/src/tty_far2l_input.c`), the
emulator (`tests/src/viewer/vterm_terminal.c`) and the key encoder
(`tests/src/mcterm_key_encode.c`). By hand, one line each:

- Linux, far2l's terminal or f4: `mcommander`, About shows `far2l`; Ctrl-Enter, Shift-Tab
  and Alt-F4 in the panels each do their own action; Ctrl-O, and in the shell run
  `f4`: About of that program (or its key check) shows the extensions on.
- Linux, any other terminal: `MC_FAR2L=0 mcommander` and a terminal that does not know
  the extensions both show `legacy` or `kitty`, and behave as before.
- Windows Terminal (through `ssh`, or MSYS2 build): About shows `win32`; nothing is sent
  that the terminal did not answer for.
- tmux and screen, or over ssh: `TERM=screen*`/`tmux*` is not asked; through ssh the
  answer travels with the rest, so About shows what the terminal at the far end has.

## Dropped files

A terminal that also has the far2l drag and drop protocol (far2l with it, f4; the
specification is docs/FAR2L_DND.md of https://github.com/unxed/f4) lets files be dragged
from the desktop onto the M-Commander window. Right after the extensions are switched on
M-Commander binds drop reception (BIND), and unbinds before the extensions go off, so a
child program never gets a drop. A terminal without the protocol answers nothing or an
empty reply, and nothing changes.

A drop is one small event naming an offer. M-Commander lists it (LIST), reads the files
one bounded chunk at a time (READ) and releases the offer (CLOSE): nothing travels unasked,
so the same works over SSH. The files go by M-Commander's own file access into the
directory of the panel under the drop, so a panel that shows an archive or a remote host
is no special case. If the terminal does not say where the drop landed, M-Commander asks
before it copies to the active panel. A drop is taken only while the panels are on top,
not in a dialog, the viewer or the editor. Existing files ask for overwrite, skip,
overwrite all or cancel, and a file that could not be received whole is removed.

Only plain files are taken, and only names that are plain names: a name with a slash, `..`
or a control character is refused. Every request is answered within 20 seconds or the drop
is given up.

Not there yet: directories, a bigger window than one request at a time, and a progress bar
and Esc to cancel a long copy.

## Dropped files for the programs in the built-in terminal

A program in the built-in terminal that has the extensions on can bind drop reception
(BIND) as it would on a far2l terminal, so that a file dropped on the outer window over
the terminal reaches it, and mc inside mc inside a far2l terminal works too. Binding works
only while the terminal M-Commander itself runs in has drop reception bound; otherwise the
program is told it is not supported, as it is on any terminal without the protocol.

A file dropped on the outer window over the terminal is announced to the program as a drop
of its own, at the cell of the terminal it landed on. The program's LIST and READ are
answered with LIST and READ of the outer offer, one chunk at a time, nothing kept in
between; its CLOSE releases the outer offer, and so do `ESC _ far2l0 BEL`, a new binding,
the lease running out (600 seconds) and the terminal closing. The other far2l interactions
are answered with the empty reply, which is how a far2l says it has none.

The terminal serves what the outer one has: plain files, one READ at a time, at most 8
offers and chunks of at most 32 KiB. Mouse events of the extensions are not passed to the
program yet.
