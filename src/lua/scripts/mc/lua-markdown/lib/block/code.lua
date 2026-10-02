-- Code blocks: the language of the fence, colored by the syntax rules of
-- the editor through mc.syntax.scan, drawn in a frame over a background the
-- skin agrees with.  A mermaid fence is drawn as a diagram instead.

local cfg = require("config")
local txt = require("text")
local sgr = require("sgr")
local spans = require("spans")
local mermaid = require("mermaid")

local width = txt.width
local expand_tabs = txt.expand_tabs

local sgr_of_color = sgr.of_color
local code_bg = sgr.code_bg
local SGR_BG_OFF, SGR_RUN_OFF = sgr.SGR_BG_OFF, sgr.SGR_RUN_OFF

local units_of, unit_width, WIDE = spans.of_text, spans.unit_width, spans.WIDE

local CODE_INDENT, CODE_MARGIN = cfg.CODE_INDENT, cfg.CODE_MARGIN
local FRAME_TL, FRAME_TR = cfg.FRAME_TL, cfg.FRAME_TR
local FRAME_BL, FRAME_BR = cfg.FRAME_BL, cfg.FRAME_BR

local M = {}

-- The scan of a code block, or nil when there are no rules for it or no mc
-- to ask (the renderer also runs outside it).  The language of the fence is
-- given as the name of a file, the way the rules are chosen for a real one;
-- a fence without a language leaves the choice to the first line of the code,
-- which is how a shebang is recognized.
local function scan_code(code, language)
    if mc == nil or mc.syntax == nil or mc.syntax.scan == nil then
        return nil
    end
    if language == nil or language == "" then
        return mc.syntax.scan(code)
    end
    local key = language:lower()
    local filename = cfg.CODE_FILENAMES[key] or ("code." .. (cfg.CODE_LANGUAGES[key] or key))
    return mc.syntax.scan(code, { filename = filename })
end

-- A mermaid diagram is drawn, not shown as code, when it is one the drawing
-- knows; anything else stays a code block.
local function mermaid_lines(code, language, out, width_limit)
    if language == nil or language:lower() ~= "mermaid" then
        return false
    end
    local room = math.max(width_limit or cfg.DIAGRAM_WIDTH, cfg.DIAGRAM_WIDTH) - #CODE_INDENT
    local drawn = mermaid.render(code, room)

    if drawn == nil then
        return false
    end
    for _, line in ipairs(drawn) do
        out[#out + 1] = line == "" and "" or ("    " .. line)
    end
    return true
end

-- The columns a line takes on the screen, past the sequences in it.
local function code_width(line)
    return width((line:gsub("\27%[[%d;]*m", ""):gsub("\27%]8;;.-\27\\", "")))
end

-- The columns the block takes: the widest line and the air after it, never
-- wider than the screen.
local function code_box(lines, width_limit)
    local box = 0

    for _, line in ipairs(lines) do
        local w = code_width(line)

        if w > box then
            box = w
        end
    end
    box = math.max(box + math.floor(box * cfg.CODE_AIR), cfg.CODE_MIN - 2 * CODE_MARGIN)
    if width_limit ~= nil then
        box = math.min(box, math.max(width_limit - #CODE_INDENT - 2 * CODE_MARGIN, 1))
    end
    return box
end

-- A block of plain text, not of code: what four columns of indent or a fence
-- with no language make, as often prose kept as it was typed as code.
local function is_plain(language)
    return language == nil or language == "" or cfg.CODE_PLAIN_NAMES[language:lower()] == true
end

-- The pieces of a line too wide for the block, each at most w columns: cut
-- after the last space that fits, or where the line runs out of room when
-- there is none in the second half.  A piece after the first is indented as
-- far as the line is, and a color open at a cut is closed before it and
-- opened again after it, because the viewer may start reading at any line.
local function wrap_code(line, w)
    if code_width(line) <= w then
        return { line }
    end

    local units = units_of(line)
    local function is_space(u)
        return u:gsub("\27%[[%d;]*m", "") == " "
    end

    local indent = 0
    while units[indent + 1] ~= nil and is_space(units[indent + 1]) do
        indent = indent + 1
    end
    if indent > w // 2 then
        indent = 0
    end

    local pieces = {}
    local open
    local start = 1

    while start <= #units do
        local lead = #pieces > 0 and indent or 0
        local room = w - lead
        local stop, used = start - 1, 0

        while units[stop + 1] ~= nil and used + unit_width(units[stop + 1]) <= room do
            stop = stop + 1
            used = used + unit_width(units[stop])
        end
        if stop < start then
            -- a wide character in a room of one column
            stop = start
        end
        if stop < #units then
            for k = stop, start + room // 2, -1 do
                if is_space(units[k]) then
                    stop = k
                    break
                end
            end
        end

        local piece = table.concat(units, "", start, stop)
        local text = (" "):rep(lead) .. (open or "") .. piece

        for seq in piece:gmatch("\27%[[%d;]*m") do
            open = seq ~= SGR_RUN_OFF and seq or nil
        end
        if open ~= nil and stop < #units then
            text = text .. SGR_RUN_OFF
        end
        pieces[#pieces + 1] = (text:gsub(WIDE, ""))

        start = stop + 1
        -- the spaces at a cut are dropped, a color they open is not
        while stop < #units and units[start] ~= nil and is_space(units[start]) do
            for seq in units[start]:gmatch("\27%[[%d;]*m") do
                open = seq ~= SGR_RUN_OFF and seq or nil
            end
            start = start + 1
        end
    end
    return pieces
end

-- One edge of the frame: the corners with their stubs, the language in the
-- top one.  The edge is as wide as the lines between the corners, so that
-- the background of the block is a rectangle.
local function code_edge(left, right, label, box)
    local text = label ~= nil and label ~= "" and (" " .. label) or ""
    local fill = math.max(box + 2 * CODE_MARGIN - width(left) - width(right) - width(text), 0)

    return left .. text .. (" "):rep(fill) .. right
end

-- The lines of a code block, padded to the widest one so that the background
-- covers a rectangle.  Each line opens the background and closes it at its
-- end, because the viewer may start reading at any line.
-- Plain text is kept to the width of the prose and its lines are wrapped to
-- it; code takes as much of the screen as it needs, and only a line wider
-- than the screen is wrapped, with a mark in the margin where it goes on.
local function emit_code(lines, out, width_limit, language, screen)
    local plain = is_plain(language)

    if language == nil or language == "" then
        language = cfg.CODE_PLAIN
    end

    local box = code_box(lines, plain and width_limit or math.max(screen or 0, width_limit or 0))
    local color = code_bg()
    local bg = color ~= nil and ("\27[" .. color .. "m") or ""
    local off = bg ~= "" and SGR_BG_OFF or ""

    if cfg.CODE_FRAME and language ~= nil and language ~= "" then
        -- a language longer than the code widens the block, so that both
        -- edges and the lines between them keep the same width
        local edge = width(FRAME_TL) + width(" " .. language) + CODE_MARGIN + width(FRAME_TR)

        box = math.max(box, edge - 2 * CODE_MARGIN)
    end
    if cfg.CODE_FRAME then
        out[#out + 1] = CODE_INDENT .. bg .. code_edge(FRAME_TL, FRAME_TR, language, box) .. off
    end
    for _, whole in ipairs(lines) do
        local pieces = wrap_code(whole, box)

        for k, line in ipairs(pieces) do
            local fill = math.max(box - code_width(line), 0) + CODE_MARGIN
            local tail = bg ~= "" and (" "):rep(fill) or ""

            if not plain and k < #pieces then
                tail = (" "):rep(fill - CODE_MARGIN) .. cfg.CODE_WRAP_MARK .. (" "):rep(CODE_MARGIN - 1)
            end
            out[#out + 1] = CODE_INDENT .. bg .. (" "):rep(CODE_MARGIN) .. line .. tail .. off
        end
    end
    if cfg.CODE_FRAME then
        out[#out + 1] = CODE_INDENT .. bg .. code_edge(FRAME_BL, FRAME_BR, nil, box) .. off
    end
end

-- The lines of a code block, colored where the rules say so.  Each line
-- opens the color it starts in and closes it at its end, because the viewer
-- may start reading at any line.
local function code_lines(code, language, out, width_limit, screen)
    if mermaid_lines(code, language, out, width_limit) then
        return
    end
    local scan = scan_code(code, language)
    local colored = {}
    local lines = {}
    local pos = 1

    if scan == nil then
        for line in (code .. "\n"):gmatch("(.-)\n") do
            lines[#lines + 1] = expand_tabs(line)
        end
        if code:sub(-1) == "\n" then
            lines[#lines] = nil
        end
        emit_code(lines, out, width_limit, language, screen)
        return
    end

    for _, run in ipairs(scan.runs) do
        local sgr = sgr_of_color(scan.colors[run.color])
        local text = code:sub(run.offset, run.offset + run.length - 1)

        for piece, eol in (text .. "\0"):gmatch("([^\n]*)(\n?)") do
            if piece ~= "" then
                colored[#colored + 1] = sgr ~= "" and (sgr .. piece:gsub("%z", "") .. SGR_RUN_OFF)
                    or piece:gsub("%z", "")
            end
            if eol == "\n" then
                colored[#colored + 1] = "\n"
            end
        end
        pos = run.offset + run.length
    end
    if pos <= #code then
        colored[#colored + 1] = code:sub(pos)
    end

    for line in (table.concat(colored) .. "\n"):gmatch("(.-)\n") do
        lines[#lines + 1] = expand_tabs(line)
    end
    if code:sub(-1) == "\n" then
        lines[#lines] = nil
    end
    emit_code(lines, out, width_limit, language, screen)
end

M.line_width = code_width
M.mermaid = mermaid_lines
M.emit = emit_code
M.emit_fence = code_lines

return M
