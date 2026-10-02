-- A document, block by block: the one pass over the lines that decides what
-- each block is and hands it to the module that draws it.  The blocks are
-- emitted as they are finished, so that the viewer can show the first
-- screen of a long file before the rest is rendered.

local cfg = require("config")
local txt = require("text")
local inl = require("inline")
local env = require("formula.env")
local codeblock = require("block.code")
local tbl = require("block.table")
local para = require("block.flow")
local scanner = require("block.scanner")
local definitions = require("block.definitions")

local width = txt.width
local trim = txt.trim
local NBSP = txt.NBSP

local inline = inl.render
local footnote_number = inl.footnote_number

local math_block_of = env.block_of

local code_width = codeblock.line_width
local emit_code = codeblock.emit
local code_lines = codeblock.emit_fence

local render_table = tbl.render
local flow = para.emit
local collect_definitions = definitions.collect

local split_lines = scanner.split_lines
local join_display_math = scanner.join_display_math
local is_blank = scanner.is_blank
local is_hr = scanner.is_hr
local is_table_sep = scanner.is_table_sep
local fence_of = scanner.fence_of
local list_item = scanner.list_item
local definition_item = scanner.definition_item
local is_atx_heading = scanner.is_atx_heading
local starts_block = scanner.starts_block
local read_fence = scanner.read_fence
local read_indented = scanner.read_indented

local BOX_H, BOX_V = cfg.BOX_H, cfg.BOX_V
local BULLETS, LIST_INDENT = cfg.BULLETS, cfg.LIST_INDENT
local BOX_EMPTY, BOX_DONE = cfg.BOX_EMPTY, cfg.BOX_DONE

local M = {}

-- referenced follow in the order they were written.
local function render_footnotes(doc, width_limit, out)
    if #doc.note_defs == 0 then
        return
    end
    for _, key in ipairs(doc.note_defs) do
        footnote_number(doc, key)
    end
    while out[#out] == "" do
        out[#out] = nil
    end
    out[#out + 1] = ""
    out[#out + 1] = BOX_H:rep(math.min(20, width_limit))
    local k = 1
    while k <= #doc.note_order do
        flow(doc.notes[doc.note_order[k]], "[" .. k .. "] ", width_limit, out, doc)
        k = k + 1
    end
end

-- The widest line of a chunk, kept in the options: the caller tells the
-- viewer from it whether the text has to be broken or can be scrolled
-- sideways.  A line no longer than the screen in bytes cannot be wider than
-- it in columns, which keeps the walk off most of the text.
local function note_width(chunk, opts, width_limit)
    if opts == nil then
        return
    end
    local most = opts.max_line or 0

    for line in chunk:gmatch("([^\n]*)\n") do
        if #line > width_limit then
            local w = code_width(line)

            if w > most then
                most = w
            end
        end
    end
    opts.max_line = most
end

local function render_document(text, opts, emit)
    local width_limit = opts and opts.width or cfg.DEFAULT_WIDTH
    -- code is not held to the width of the prose, only to the screen
    local screen = opts and opts.screen or width_limit
    local lines, doc = collect_definitions(join_display_math(split_lines(text)))
    local out = {}
    local i = 1
    local emitted = false
    local prev_blank = true
    local prev_list = false
    local list_levels = {}  -- the indents of the lists that are open
    local list_hanging = ""  -- what a continuation line of the last item is indented by

    while i <= #lines do
        local line = lines[i]
        local next_line = lines[i + 1]
        local fence = fence_of(line)
        local blank = is_blank(line)
        local was_list = prev_list
        prev_list = blank and was_list
        if not blank and not was_list then
            list_levels = {}
        end

        if fence ~= nil then
            local language, code

            language, code, i = read_fence(lines, i, fence)
            code_lines(table.concat(code, "\n"), language, out, width_limit, screen)
        elseif line:find("<!--", 1, true) and not line:find("-->", 1, true) then
            while i <= #lines and not lines[i]:find("-->", 1, true) do
                i = i + 1
            end
            i = i + 1
        elseif blank then
            out[#out + 1] = ""
            i = i + 1
        elseif prev_blank and not was_list and (line:match("^    ") or line:match("^\t")) then
            local block

            block, i = read_indented(lines, i)
            emit_code(block, out, width_limit, nil, screen)
        elseif math_block_of(line, width_limit) ~= nil then
            for _, l in ipairs(math_block_of(line, width_limit)) do
                out[#out + 1] = l
            end
            i = i + 1
        elseif line:find("|", 1, true) and next_line ~= nil and is_table_sep(next_line) then
            local rows = { line, next_line }
            i = i + 2
            while i <= #lines and lines[i]:find("|", 1, true) and not is_blank(lines[i]) do
                rows[#rows + 1] = lines[i]
                i = i + 1
            end
            render_table(rows, out, width_limit, doc)
        elseif definition_item(line) ~= nil then
            -- the meaning is indented under the term it belongs to
            local pieces = { definition_item(line) }

            i = i + 1
            while i <= #lines and not starts_block(lines[i], lines[i + 1]) do
                pieces[#pieces + 1] = trim(lines[i])
                i = i + 1
            end
            flow(pieces, "    ", width_limit, out, doc)
        elseif is_hr(line) then
            out[#out + 1] = BOX_H:rep(width_limit)
            i = i + 1
        elseif is_atx_heading(line) then
            local hashes, rest = line:match("^ ? ? ?(#+)%s*(.-)%s*$")
            rest = rest:gsub("%s+#+$", ""):gsub("^#+$", "")
            out[#out + 1] = inline(rest, { heading = math.min(#hashes, 6) }, doc)
            i = i + 1
        elseif next_line ~= nil and not was_list and list_item(line) == nil
            and (next_line:match("^ ? ? ?=+%s*$") or next_line:match("^ ? ? ?%-+%s*$")) then
            out[#out + 1] = inline(trim(line), { heading = next_line:match("=") and 1 or 2 }, doc)
            i = i + 2
        else
            local quotes = 0
            local rest = line
            while true do
                local after = rest:match("^%s*> ?(.*)$")
                if after == nil then
                    break
                end
                quotes = quotes + 1
                rest = after
            end
            local prefix = (BOX_V .. " "):rep(quotes)
            local indent, marker, item = list_item(rest)
            if indent ~= nil then
                local depth = #indent
                local level

                -- the indent of the source says how deep the item is; the
                -- output indents every level by the same amount
                while #list_levels > 0 and list_levels[#list_levels] > depth do
                    list_levels[#list_levels] = nil
                end
                if #list_levels == 0 or list_levels[#list_levels] < depth then
                    list_levels[#list_levels + 1] = depth
                end
                level = #list_levels

                local bullet = marker:match("^%d") and marker
                    or BULLETS[(level - 1) % #BULLETS + 1]
                local task, task_text = item:match("^%[([ xX])%]%s+(.*)$")

                -- a task list: the box takes the place of the bullet
                if task ~= nil and not marker:match("^%d") then
                    bullet = task == " " and BOX_EMPTY or BOX_DONE
                    item = task_text
                end

                prefix = prefix .. (" "):rep((level - 1) * LIST_INDENT) .. bullet .. " "
                list_hanging = (" "):rep(width(prefix) - quotes * 2)
                rest = item
                prev_list = true
            elseif was_list and rest:match("^%s") then
                prefix = prefix .. list_hanging
                rest = rest:gsub("^%s+", "")
                prev_list = true
            end
            local pieces = { rest }
            i = i + 1
            while i <= #lines and not starts_block(lines[i], lines[i + 1]) do
                local more = lines[i]
                for _ = 1, quotes do
                    more = more:match("^%s*> ?(.*)$") or more
                end
                if quotes == 0 and more:match("^%s*>") then
                    break
                end
                pieces[#pieces + 1] = more:gsub("^%s+", "")
                i = i + 1
            end
            flow(pieces, prefix, width_limit, out, doc)
        end
        prev_blank = blank
        if emit ~= nil then
            -- Footnotes remove trailing blank lines, so hold those until the next block.
            local last = #out
            while last > 0 and out[last] == "" do
                last = last - 1
            end
            if last > 0 then
                local chunk = (table.concat(out, "\n", 1, last):gsub(NBSP, " ")) .. "\n"
                local pending = {}
                for k = last + 1, #out do
                    pending[#pending + 1] = out[k]
                end
                out = pending
                emitted = true
                note_width(chunk, opts, width_limit)
                emit(chunk)
            end
        end
    end
    render_footnotes(doc, width_limit, out)
    local tail = (table.concat(out, "\n"):gsub(NBSP, " ")) .. "\n"

    note_width(tail, opts, width_limit)
    if emit == nil then
        return tail
    end
    if #out > 0 or not emitted then
        emit(tail)
    end
end
function M.render(text, opts)
    return render_document(text, opts)
end

-- One complete Markdown block per call, nil at EOF.  The preliminary pass
-- collects forward references before any output is emitted.  Each iterator
-- owns its document, so a resize or another viewer can render between two
-- calls.
function M.blocks(text, opts)
    local co = coroutine.create(function()
        render_document(text, opts, coroutine.yield)
    end)
    return function()
        if coroutine.status(co) == "dead" then
            return nil
        end
        local ok, chunk = coroutine.resume(co)
        if not ok then
            error(chunk, 0)
        end
        return chunk
    end
end

return M
