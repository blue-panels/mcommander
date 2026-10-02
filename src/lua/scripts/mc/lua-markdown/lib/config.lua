-- What the rendering may be told to do, and the glyphs it draws with.
-- Everything a user or another module may read or change lives here; the
-- table is shared, so a change reaches every module that took it.

local M = {}

------------------------------------------------------------------------
-- Widths.

M.MIN_COLUMN = 8     -- a table column is never squeezed narrower than this
M.DEFAULT_WIDTH = 80 -- the screen width when the caller names none
M.MAX_WIDTH = 120    -- text is never flowed wider than this, whatever the screen

-- how much of a document is looked through for blocks that cannot be
-- wrapped, before the first screen of it is rendered
M.UNWRAPPED_SCAN = 256 * 1024

-- A diagram is laid out up to this width whatever the screen has, and a
-- block this wide is scrolled sideways rather than broken.  Squeezing a
-- drawing into a narrow screen costs more than the scrolling does.
M.DIAGRAM_WIDTH = 160

------------------------------------------------------------------------
-- Colors.

-- SGR colors of the heading levels; a level without one is bold, and the
-- first level keeps the heading color of the skin
M.HEADING_COLORS = { [2] = "96", [3] = "92" }

-- SGR background of a code block.  "auto" asks mc: a terminal of 256 colors
-- or more gets a shade of the background the skin paints the viewer with, a
-- poorer one keeps its background untouched.  A string such as "100" or
-- "48;5;236" is used as it is, and nil or "" leaves the background alone.
M.CODE_BG = "auto"

-- how far the background of a code block is moved away from the one of the
-- viewer, of 255 per channel
M.CODE_BG_SHIFT = 24

------------------------------------------------------------------------
-- Code blocks.

-- draw the corners of a code block, with the language of the fence in the
-- top edge
M.CODE_FRAME = true

-- what the top edge says of a block whose language is not known: a fence
-- that names none, or a block written with four columns of indent
M.CODE_PLAIN = "text"

-- the languages of a fence that say the block is plain text: it is wrapped
-- to the width of the prose, where code is given the whole screen
M.CODE_PLAIN_NAMES = { text = true, txt = true, plain = true, plaintext = true }

-- the mark in the margin of a line of code wrapped at the edge of the screen
M.CODE_WRAP_MARK = "\\"

-- the block is this share of its longest line wider than the code in it, so
-- that a line does not end right at the frame
M.CODE_AIR = 0.2

-- the narrowest a code block gets, margins counted, however short the code
M.CODE_MIN = 43

-- What a fence writes, turned into the name of a file: the rules of the
-- editor are chosen by name, and the name of a language is usually its
-- extension.  Only the ones that differ are listed.
M.CODE_LANGUAGES = {
    bash = "sh", zsh = "sh", shell = "sh", console = "sh",
    ["c++"] = "cpp", cxx = "cpp", cc = "cpp", hpp = "h",
    javascript = "js", node = "js", typescript = "ts",
    python = "py", ruby = "rb", rust = "rs", kotlin = "kt", perl = "pl",
    markdown = "md", yml = "yaml", patch = "diff", conf = "ini",
}

-- Languages whose rules are chosen by a whole name, not by an extension.
M.CODE_FILENAMES = {
    make = "Makefile", makefile = "Makefile", cmake = "CMakeLists.txt",
    dockerfile = "Dockerfile",
}

------------------------------------------------------------------------
-- Diagrams.

-- How a decision is drawn: "braille" slopes its sides with the dots of the
-- braille block, "box" keeps to the box drawing characters, for a font that
-- has no braille.
M.DECISION_STYLE = "braille"

------------------------------------------------------------------------
-- The glyphs the text is drawn with.

M.BOX_H = "\u{2500}"
M.BOX_V = "\u{2502}"
M.BOX_X = "\u{253C}"

-- one bullet per nesting level, cycled after the last
M.BULLETS = { "\u{2022}", "\u{25E6}", "\u{25AA}" }
M.LIST_INDENT = 2  -- columns a nesting level adds

-- the boxes of a task list
M.BOX_EMPTY = "\u{2610}"
M.BOX_DONE = "\u{2611}"

-- a tab in a code block moves to the next stop
M.TAB_WIDTH = 8

-- a code block is indented this far, and the frame and the background keep
-- a margin between their edge and the code
M.CODE_INDENT = "    "
M.CODE_MARGIN = 2

-- the corners of a code block, each with the stub of an edge
M.FRAME_TL = "\u{250C}\u{2574}"
M.FRAME_TR = "\u{2576}\u{2510}"
M.FRAME_BL = "\u{2514}\u{2574}"
M.FRAME_BR = "\u{2576}\u{2518}"

return M
