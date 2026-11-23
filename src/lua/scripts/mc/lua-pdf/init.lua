--[[
   PDF viewer lua-pdf for the M-Commander
   F3 on a PDF: the text layer as text, the pictures as sixel

   Copyright (C) 2026
   Ilia Maslakov il.smind@gmail.com

   This file is part of M-Commander.

   M-Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   M-Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see https://www.gnu.org/licenses/.
]]

-- pdftohtml writes the page as XML: where every run of characters sits and
-- where every picture sits, the pictures next to it as files.  The text goes
-- on the screen as text, one run per its own cells, and the pictures over it
-- in sixel.  What the paper has side by side stays side by side.
--
-- A page is written out whole, as tall as it needs to be, so the viewer
-- scrolls it the way it scrolls any terminal output.  > and < turn the page,
-- + and - change the zoom, p asks for a page number, and i opens the menu,
-- which also picks the layout.  / searches the text of the document, n and
-- N walk the matches: the page with the match is drawn with it in reverse
-- video, and the viewer opens at its row.

local pdfxml = require("pdfxml")
local render = require("render")

-- Pages come out of the file in runs of this many: the first page of a big
-- file is not worth the wait for the rest.  The text of this many runs is
-- kept; older runs are read again when the reader comes back to them.
local CHUNK_PAGES = 8
local MAX_CACHED_CHUNKS = 4
local MAX_XML_BYTES = 64 * 1024 * 1024
local MAX_SIXEL_BYTES = 4 * 1024 * 1024

local next_session_id = 0

local function quote(text)
    return "'" .. text:gsub("'", "'\\''") .. "'"
end

local function run(command, max_output)
    local result, err = mc.process.run {
        command = command,
        max_output = max_output or 1024 * 1024,
    }
    if result == nil then
        mc.log.warn("lua-pdf: " .. tostring(err))
        return nil
    end
    return result
end

local function have(tool)
    local result = run("command -v " .. quote(tool) .. " >/dev/null 2>&1")
    return result ~= nil and result.exit_code == 0
end

local function read_file(path, limit)
    local file = io.open(path, "rb")
    if file == nil then
        return nil
    end
    local size = file:seek("end")
    if size == nil or size > limit then
        file:close()
        return nil
    end
    file:seek("set", 0)
    local text = file:read("a")
    file:close()
    return text
end

------------------------------------------------------------------------
-- The pages.

local function chunk_first(page_number)
    return math.floor((page_number - 1) / CHUNK_PAGES) * CHUNK_PAGES + 1
end

-- The text of a page is kept for as long as the reader may come back to it;
-- a session that walks a long document keeps neither the text of every page
-- it has been through nor the pictures written out with it.
local function forget_old_chunks(session)
    while #session.chunk_order > MAX_CACHED_CHUNKS do
        local oldest = table.remove(session.chunk_order, 1)
        local chunk = session.chunks[oldest]

        if chunk ~= nil then
            for page = chunk.from, chunk.to do
                session.page_cache[page] = nil
            end
            session.chunks[oldest] = nil
            run("rm -f -- " .. quote(string.format("%s/p%05d", session.dir, oldest)) .. "*")
        end
    end
end

-- The run of pages that holds @number, extracted once and kept.
local function load_chunk(session, number)
    local first = chunk_first(number)
    if session.chunks[first] then
        return
    end

    local last = math.min(first + CHUNK_PAGES - 1, session.pages)
    local xml_path = string.format("%s/p%05d.xml", session.dir, first)
    -- A terminal that draws no sixel has no use for the pictures, and
    -- writing them out is most of the work on a scanned document.
    local command = string.format(
        "pdftohtml -xml%s -noroundcoord -enc UTF-8 -f %d -l %d -- %s %s >/dev/null 2>&1",
        session.want_images and "" or " -i", first, last, quote(session.local_path),
        quote(xml_path))
    local result = run(command)

    if result == nil or result.exit_code ~= 0 then
        mc.log.warn("lua-pdf: pdftohtml failed on pages " .. first .. "-" .. last)
        return
    end
    local text = read_file(xml_path, MAX_XML_BYTES)
    if text == nil then
        mc.log.warn("lua-pdf: cannot read " .. xml_path)
        return
    end
    local parsed = pdfxml.parse(text)

    if #parsed == 0 then
        mc.log.warn("lua-pdf: no pages in " .. xml_path)
        return
    end
    for _, page in ipairs(parsed) do
        session.page_cache[page.number] = page
    end
    session.chunks[first] = { from = first, to = last }
    session.chunk_order[#session.chunk_order + 1] = first
    forget_old_chunks(session)
end

local function page_of(session, number)
    if session.page_cache[number] == nil then
        load_chunk(session, number)
    end
    return session.page_cache[number]
end

------------------------------------------------------------------------
-- The pictures.

local function image_path(session, src)
    if src:sub(1, 1) == "/" then
        return src
    end
    return session.dir .. "/" .. src
end

-- The size in pixels of a cell of chafa, which has no terminal to ask:
-- 10x20 in 1.18, 8x8 in older ones under --font-ratio=1/1. A picture one cell
-- big is that cell exactly: a 1x1 PNG drawn so tells the size.
local CHAFA_CELL_SCRIPT = [[
tmp=$(mktemp) || exit 1
trap 'rm -f "$tmp"' EXIT HUP INT TERM
printf '\211\120\116\107\015\012\032\012\000\000\000\015\111\110\104\122\000\000\000\001\000\000\000\001\010\000\000\000\000\072\176\233\125\000\000\000\012\111\104\101\124\170\332\143\140\000\000\000\002\000\001\345\047\336\374\000\000\000\000\111\105\116\104\256\102\140\202' > "$tmp"
chafa --format=sixel --font-ratio=1/1 --size=1x1 -- "$tmp" 2>/dev/null
]]

-- Asked once a session. The probe draws a picture of its own, so when it
-- fails it fails for every picture: false then, and no sixel from chafa.
local function chafa_cell(session)
    local cell = session.tools.chafa_cell

    if cell == nil then
        local result = run(CHAFA_CELL_SCRIPT)
        local w, h

        if result ~= nil then
            w, h = result.stdout:match('"1;1;(%d+);(%d+)')
        end
        w, h = tonumber(w), tonumber(h)
        cell = w ~= nil and h ~= nil and w > 0 and h > 0 and { w = w, h = h } or false
        session.tools.chafa_cell = cell
    end
    return cell or nil
end

-- The picture in chafa's cells: the most that fits in its place, which chafa
-- fills keeping the proportions. Nil when not even one cell fits, or when the
-- size of a chafa cell is not known.
local function chafa_size(session, image)
    local cell = chafa_cell(session)

    if cell == nil then
        return nil
    end

    local cols = image.width // cell.w
    local rows = image.height // cell.h

    if cols < 1 or rows < 1 then
        return nil
    end
    return cols, rows
end

-- The bytes of one picture, in the size it takes on the screen: sixel where
-- the terminal draws it, and chafa's characters where it does not.
local function picture_command(session, image, sixel)
    local path = quote(image_path(session, image.src))

    if not sixel then
        return string.format("chafa --format=symbols --stretch --size=%dx%d -- %s 2>/dev/null",
                             image.cols, image.rows, path)
    end
    if session.tools.encoder == "img2sixel" then
        return string.format("img2sixel -w %d -h %d -- %s 2>/dev/null",
                             image.width, image.height, path)
    end
    local cols, rows = chafa_size(session, image)

    return string.format("chafa --format=sixel --font-ratio=1/1 --size=%dx%d -- %s 2>/dev/null",
                         cols, rows, path)
end

-- Only the picture itself: chafa wraps what it draws in sequences of its
-- own, which the viewer has no use for.
local function picture_bytes(result, sixel)
    if result == nil or result.stdout_truncated then
        return nil
    end
    if sixel then
        local first = result.stdout:find("\27P", 1, true)
        local last = first ~= nil and result.stdout:find("\27\\", first, true) or nil

        return last ~= nil and result.stdout:sub(first, last + 1) or nil
    end

    local text = result.stdout:gsub("\27%[%?%d+[hl]", ""):gsub("%s+$", "")

    return text ~= "" and text or nil
end

local function picture_data(session, image, sixel)
    if sixel and session.tools.encoder == nil then
        return nil
    end
    -- smaller than a cell of chafa, or its cell not known: drawn in its characters
    if sixel and session.tools.encoder == "chafa" and chafa_size(session, image) == nil then
        return picture_data(session, image, false), "symbols"
    end
    if not sixel and not session.tools.chafa then
        return nil
    end

    local key = string.format("%s|%d|%d|%d|%d|%s", image.src, image.width, image.height,
                              image.cols, image.rows, sixel and "s" or "c")
    local cached = session.sixels[key]

    if cached ~= nil then
        return cached ~= false and cached or nil, sixel and "sixel" or "symbols"
    end

    local data = picture_bytes(run(picture_command(session, image, sixel), MAX_SIXEL_BYTES), sixel)

    -- chafa draws in whole cells of its own and may come out lower than the
    -- place; the raster attributes say the place, so the picture takes all its
    -- rows and the text below goes where the page puts it
    if data ~= nil and sixel and session.tools.encoder == "chafa" then
        data = data:gsub('^(\27P[^q]*q"%d+;%d+;%d+;)%d+', "%1" .. image.height, 1)
    end

    session.sixels[key] = data ~= nil and data or false
    return data, sixel and "sixel" or "symbols"
end

------------------------------------------------------------------------
-- The screen.

local function message_screen(view, lines)
    local out = { "\27[2J" }
    for index, text in ipairs(lines) do
        out[#out + 1] = string.format("\27[%d;1H%s", index, render.clip(text, view.columns))
    end
    return table.concat(out)
end

local MODE_LABELS = {
    text = "text",
    page = "page",
    fit = "fit",
}

-- The viewer's status line: the file, the keys that move the page, the page.
-- The viewer keeps the byte counter to the right of it and cuts the middle
-- out of whatever does not fit, so the name is shortened here instead, and
-- the keys and the page stay whole.
local function shorten(name, room)
    if room < 8 or render.text_columns(name) <= room then
        return name
    end

    local head = (room - 1) // 2
    local tail = room - head - 1
    local ok, offset = pcall(utf8.offset, name, -tail)

    if not ok or offset == nil then
        return render.clip(name, room - 1) .. "~"
    end
    return render.clip(name, head) .. "~" .. name:sub(offset)
end

-- Without the tools there is no page to show and nothing to say about one:
-- the line names what is not installed, and the viewer shows the file.
local function missing_title(session, view)
    local missing = {}

    if not session.tools.pdftohtml then
        missing[#missing + 1] = "no pdftohtml"
    end
    if not session.tools.pdfinfo then
        missing[#missing + 1] = "no pdfinfo"
    end

    local rest = "   " .. table.concat(missing, ", ")

    return shorten(session.display_name, math.max(20, view.columns - 35) - #rest) .. rest
end

local function status_text(session, params, plan, view)
    local keys = "'<' '>': prev/next page  'i': menu"
    local page = string.format("page %d/%d", params.page, session.pages)
    local extra = {}

    if session.query ~= nil then
        extra[#extra + 1] = "[" .. session.query .. "]"
    end
    if plan ~= nil and (plan.mode ~= "text" or (params.zoom or 1.0) ~= 1.0) then
        extra[#extra + 1] = string.format("%s %d%%", MODE_LABELS[plan.mode] or plan.mode,
                                          math.floor((params.zoom or 1.0) * 100 + 0.5))
    end
    -- Nothing can draw the pictures of this page: that is worth saying, and
    -- worth saying even where the line is too short for the rest.
    if plan ~= nil and #plan.images > 0
        and ((view.sixel and session.tools.encoder == nil)
             or (not view.sixel and not session.tools.chafa)) then
        keys = keys .. "   no pictures"
    end

    -- What the viewer leaves for the label: the rest of its status line is
    -- the byte counter and the percentage.
    local room = math.max(20, view.columns - 35)
    local keys_and_page = string.format("  %s   %s", keys, page)
    local rest = keys_and_page

    -- The file is worth more than what is said about the view: it keeps at
    -- least 24 columns before the extras are dropped.
    while true do
        rest = keys_and_page
        if #extra > 0 then
            rest = rest .. "   " .. table.concat(extra, "   ")
        end
        if #extra == 0 or room - #rest >= 24
            or render.text_columns(session.display_name) <= room - #rest then
            break
        end
        table.remove (extra)
    end
    return shorten(session.display_name, room - #rest) .. rest
end

local ZOOM_STEP = 1.25
local ZOOM_MIN = 0.25
local ZOOM_MAX = 4.0

-- Pages read in one search: a run of pdftohtml per eight of them, and the
-- screen stands still while they are read.
local MAX_SEARCH_PAGES = 200

------------------------------------------------------------------------
-- The search.

-- Where in a run of text the needle is, searching from @from; backwards it
-- is the last match that starts before it.
local function match_in(text, needle, from, forward)
    if forward then
        return text:find(needle, from or 1, true)
    end

    local limit = from or #text + 1
    local best_start, best_stop = nil, nil
    local at = 1

    while true do
        local start, stop = text:find(needle, at, true)

        if start == nil or start >= limit then
            break
        end
        best_start, best_stop = start, stop
        at = start + 1
    end
    return best_start, best_stop
end

-- The next match from @from = {page, run, offset}, in reading order.  Pages
-- are read as they are needed; a document too long for one search stops at
-- MAX_SEARCH_PAGES and says so.
local function search_from(session, needle, from, forward)
    local step = forward and 1 or -1
    local page_number = from.page
    local scanned = 0

    while page_number >= 1 and page_number <= session.pages and scanned < MAX_SEARCH_PAGES do
        local page = page_of(session, page_number)

        if page ~= nil and #page.texts > 0 then
            local index = forward and 1 or #page.texts

            if page_number == from.page and from.run ~= nil then
                index = from.run
            end
            while index >= 1 and index <= #page.texts do
                local run = page.texts[index]
                local offset = nil

                if page_number == from.page and index == from.run then
                    offset = from.offset
                end
                local start, stop = match_in(run.text:lower(), needle, offset, forward)

                if start ~= nil then
                    return { page = page_number, run = index, from = start, to = stop }
                end
                index = index + step
            end
        end
        scanned = scanned + 1
        page_number = page_number + step
    end
    return nil, scanned >= MAX_SEARCH_PAGES
end

-- /: what to look for.
local function ask_query(session, params)
    local result, err = mc.ui.dialog {
        title = "Search in the document",
        width = 56,
        controls = {
            { type = "label", text = "Text to look for:" },
            { id = "query", type = "input", value = session.query or "", width = 44,
              history = "lua-pdf-search" },
            { type = "hbox", expand_x = true, controls = {
                { type = "spacer", expand_x = true },
                { id = "ok", type = "button", label = "&OK", default = true },
                { id = "cancel", type = "button", label = "&Cancel", cancel = true },
            } },
        },
    }
    if result == nil then
        if err ~= "cancelled" then
            mc.ui.message("PDF viewer", err)
        end
        return nil
    end
    local query = result.values.query:match("^%s*(.-)%s*$")
    if query == "" then
        return nil
    end
    return query
end

-- The params that show a match, or nil with a word about why there is none.
local function search_step(session, params, forward, restart)
    local needle = session.query

    if needle == nil then
        return nil
    end
    local from
    if restart or params.hit == nil then
        from = { page = params.page, run = nil, offset = nil }
    elseif forward then
        from = { page = params.hit.page, run = params.hit.run, offset = params.hit.from + 1 }
    else
        from = { page = params.hit.page, run = params.hit.run, offset = params.hit.from }
    end

    mc.ui.status("Searching...")
    local hit, stopped = search_from(session, needle:lower(), from, forward)
    mc.ui.status("")
    if hit == nil then
        mc.ui.message("PDF viewer",
                      stopped and string.format("%s: not in the %d pages read.", needle,
                                                MAX_SEARCH_PAGES)
                          or string.format("%s: not found.", needle))
        return nil
    end
    return {
        page = hit.page,
        zoom = params.zoom,
        mode = params.mode,
        hit = hit,
    }
end

-- p: the page to go to, without the rest of the menu.
local function ask_page(session, params)
    local result, err = mc.ui.dialog {
        title = "Go to page",
        width = 40,
        controls = {
            { type = "label", text = string.format("Page (1-%d):", session.pages) },
            { id = "page", type = "input", value = tostring(params.page), width = 8 },
            { type = "hbox", expand_x = true, controls = {
                { type = "spacer", expand_x = true },
                { id = "ok", type = "button", label = "&OK", default = true },
                { id = "cancel", type = "button", label = "&Cancel", cancel = true },
            } },
        },
    }
    if result == nil then
        if err ~= "cancelled" then
            mc.ui.message("PDF viewer", err)
        end
        return nil
    end
    return math.tointeger(tonumber(result.values.page))
end

local viewer = mc.viewer_source.define {
    id = "pdf",
    resize = "rebuild",
    options_key = "i",
    keys = { "gt", "lt", "plus", "minus", "p", "slash", "n", "shift-n" },
    help = { file = "help.md", node = "[PDF Viewer]" },

    open = function(request)
        if request.local_path == nil then
            return nil, "not_supported"
        end
        next_session_id = next_session_id + 1

        local session = {
            local_path = request.local_path,
            display_name = request.display_name or request.local_path,
            id = next_session_id,
            chunks = {},
            chunk_order = {},
            page_cache = {},
            sixels = {},
            tools = {},
            pages = 0,
        }

        session.tools.pdftohtml = have("pdftohtml")
        session.tools.pdfinfo = have("pdfinfo")
        session.tools.chafa = have("chafa")
        if have("img2sixel") then
            session.tools.encoder = "img2sixel"
        elseif session.tools.chafa then
            session.tools.encoder = "chafa"
        end

        local info = session.tools.pdfinfo
            and run("pdfinfo -- " .. quote(session.local_path) .. " 2>/dev/null")
            or nil
        if info ~= nil and info.exit_code == 0 then
            session.pages = tonumber(info.stdout:match("Pages:%s+(%d+)")) or 0
        end
        if session.pages == 0 and session.tools.pdftohtml then
            -- No pdfinfo: pdftohtml counts the pages itself.  The pictures
            -- are ignored and nothing is written out, so this reads the text
            -- of the file once and no more.
            local counted = run(string.format(
                "pdftohtml -xml -i -q -stdout -- %s 2>/dev/null | grep -c '<page number='",
                quote(session.local_path)))

            if counted ~= nil then
                session.pages = tonumber((counted.stdout:gsub("%s+$", ""))) or 0
            end
        end
        if session.pages == 0 and session.tools.pdftohtml then
            session.pages = 1
        end

        if session.tools.pdftohtml then
            local temp = run("mktemp -d -t mc-lua-pdf.XXXXXXXX")

            if temp == nil or temp.exit_code ~= 0 then
                return nil, "cannot create a temporary directory"
            end
            session.dir = (temp.stdout:gsub("%s+$", ""))
        end

        return session
    end,

    initial_params = function(_, params)
        params.page = params.page or 1
        params.zoom = params.zoom or 1.0
        params.mode = params.mode or "text"
        return params
    end,

    prepare = function(session, params, viewport)
        local view = {
            columns = viewport ~= nil and viewport.columns or 80,
            lines = viewport ~= nil and viewport.lines or 25,
        }
        view.sixel = viewport ~= nil and viewport.pixel_width ~= nil
        view.cell_w = view.sixel and math.max(1, viewport.pixel_width // view.columns) or 8
        view.cell_h = view.sixel and math.max(1, viewport.pixel_height // view.lines) or 16

        if not session.tools.pdftohtml then
            -- Nothing to render the pages with: the file goes to the viewer
            -- as it is, and the status line says what is not installed.
            return {
                source = mc.source.file { path = session.local_path },
                title = missing_title(session, view),
                initial_display = "text",
            }
        end

        local body
        local top_row = 0
        local title = status_text(session, params, nil, view)

        -- The pictures are written out unless nothing here can draw them.
        session.want_images = view.sixel and session.tools.encoder ~= nil
            or not view.sixel and session.tools.chafa
        -- The pictures of a page are worth keeping while it is redrawn for a
        -- new size or zoom, and worth forgetting the moment the page turns:
        -- one screenful of sixel is hundreds of kilobytes.
        if session.sixel_page ~= params.page then
            session.sixels = {}
            session.sixel_page = params.page
        end

        params.page = math.max(1, math.min(params.page, math.max(1, session.pages)))
        local page = page_of(session, params.page)

        if page == nil then
            body = message_screen(view, {
                session.display_name,
                "",
                string.format("Page %d cannot be read.", params.page),
            })
        else
            local plan = render.plan(page, view, params)

            body = render.compose(plan, view, function(image)
                local data, kind = picture_data(session, image, view.sixel)

                if data == nil then
                    return nil
                end
                return data, kind
            end)
            title = status_text(session, params, plan, view)
            -- A match below the first screen is scrolled to, with a few rows
            -- of the page above it.
            if plan.hit_row ~= nil then
                top_row = math.max(0, plan.hit_row - 3)
            end
        end

        return {
            source = mc.source.bytes(body),
            title = title,
            raw_path = session.local_path,
            initial_display = "terminal",
            auto_scroll = "top",
            top_row = top_row,
        }
    end,

    -- The keys the viewer has no command for: the page and the zoom.  A key
    -- that would change nothing returns nil, and nothing is rebuilt.
    on_key = function(session, params, key)
        local page = params.page
        local zoom = params.zoom or 1.0

        if not session.tools.pdftohtml then
            return nil
        end

        if key == "gt" then
            if page >= session.pages then
                return nil
            end
            page = page + 1
        elseif key == "lt" then
            if page <= 1 then
                return nil
            end
            page = page - 1
        elseif key == "plus" then
            zoom = math.min(zoom * ZOOM_STEP, ZOOM_MAX)
        elseif key == "minus" then
            zoom = math.max(zoom / ZOOM_STEP, ZOOM_MIN)
        elseif key == "p" then
            local asked = ask_page(session, params)

            if asked == nil or asked == page then
                return nil
            end
            page = math.max(1, math.min(asked, math.max(1, session.pages)))
        elseif key == "slash" then
            local query = ask_query(session, params)

            if query == nil then
                return nil
            end
            session.query = query
            return search_step(session, params, true, true)
        elseif key == "n" then
            return search_step(session, params, true, false)
        elseif key == "shift-n" then
            return search_step(session, params, false, false)
        end

        if page == params.page and zoom == params.zoom then
            return nil
        end
        return { page = page, zoom = zoom, mode = params.mode }
    end,

    -- i: where to go on the page and how large it is drawn.  The viewer owns
    -- the arrows and PgUp/PgDn itself, so the menu is what moves the page.
    options = function(session, params)
        local result, err = mc.ui.dialog {
            title = "PDF viewer",
            width = 52,
            controls = {
                { type = "label", text = string.format("%s: %d page(s)",
                                                       session.display_name, session.pages) },
                { type = "label", text = "Page:" },
                { id = "page", type = "input", value = tostring(params.page), width = 8 },
                { id = "zoom", type = "select", label = "Zoom:",
                  value = string.format("%d", math.floor((params.zoom or 1.0) * 100 + 0.5)),
                  options = {
                      { id = "50", label = "50%" },
                      { id = "75", label = "75%" },
                      { id = "100", label = "100%" },
                      { id = "150", label = "150%" },
                      { id = "200", label = "200%" },
                  } },
                { id = "mode", type = "select", label = "Layout:", value = params.mode,
                  options = {
                      { id = "text", label = "Text lines, pictures in the flow" },
                      { id = "page", label = "The page, scaled to the width" },
                      { id = "fit", label = "The whole page in the window" },
                  } },
                { type = "hbox", expand_x = true, controls = {
                    { id = "prev", type = "button", label = "&Prev" },
                    { id = "next", type = "button", label = "&Next" },
                    { type = "spacer", expand_x = true },
                    { id = "go", type = "button", label = "&Go", default = true },
                    { id = "cancel", type = "button", label = "&Cancel", cancel = true },
                } },
            },
        }
        if result == nil then
            if err ~= "cancelled" then
                mc.ui.message("PDF viewer", err)
            end
            return nil
        end

        local page = params.page
        if result.button == "prev" then
            page = page - 1
        elseif result.button == "next" then
            page = page + 1
        else
            page = math.tointeger(tonumber(result.values.page)) or page
        end

        return {
            page = math.max(1, math.min(page, math.max(1, session.pages))),
            zoom = (tonumber(result.values.zoom) or 100) / 100,
            mode = result.values.mode or params.mode,
        }
    end,

    close = function(session)
        if session.dir ~= nil and session.dir:find("^/") ~= nil then
            run("rm -rf -- " .. quote(session.dir))
            session.dir = nil
        end
    end,
}

local function view_file(request)
    local controller, err = viewer:create(request)

    if controller == nil then
        return nil, err
    end
    return { handled = true, controller = controller }
end

mc.file_handler.register { id = "view", kind = "view", handler = view_file }
mc.file_handler.register { id = "open", kind = "open", handler = view_file }
