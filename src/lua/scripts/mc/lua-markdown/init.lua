-- F3 on a markdown file: lib/document.lua turns the text into the
-- nroff-style text the viewer paints: headings and **bold** as overstruck
-- letters, `code`, *italic* and links as underlined, $LaTeX$ as Unicode
-- symbols, tables aligned in columns with box-drawing rules.  F8 shows the
-- file itself.

-- The first screen costs the same whatever the size, so the limit is the
-- 64 MB the host keeps for one source, not the time to render.  A file over
-- it is shown as it is; the handler of mc.ext.ini is slower still, so it
-- must not pick the file up instead.
local MAX_FILE_SIZE = 64 * 1024 * 1024

-- Overstrike makes the text longer than the file: stop below the host's limit
-- with a line that says so, rather than let the source fail on it.
local MAX_RENDERED = 60 * 1024 * 1024

local cfg = require("config")
local md = require("document")
local measure = require("measure")
local settings = require("settings")

settings.load()

------------------------------------------------------------------------
-- F3 on a .md file.

-- Paragraphs are wrapped to the width of the viewer, up to cfg.MAX_WIDTH
-- columns on a wide screen, so the text is rendered again when the window
-- changes size; a width seen before is served from the session.
local viewer = mc.viewer_source.define {
    id = "markdown",
    resize = "rebuild",
    help = { file = "help.md", node = "[Markdown Viewer]" },
    open = function(request)
        -- one record per width of the screen the document was rendered at
        request.cache = {}
        return request
    end,

    -- i: the settings of the viewer.  They change what every width looks
    -- like, so what was rendered before is thrown away.
    options_key = "i",
    options = function(session, params)
        if not settings.dialog() then
            return nil
        end
        session.cache = {}
        return { revision = ((params ~= nil and params.revision) or 0) + 1 }
    end,

    prepare = function(session, _, viewport)
        if session.text == nil then
            return {
                source = mc.source.file { path = session.raw_path },
                title = session.title .. "  (too large to render)",
                initial_display = "text",
                auto_scroll = "top",
            }
        end
        local screen = viewport.columns
        local width = math.min(screen, cfg.MAX_WIDTH)
        local opts = { width = width, screen = screen }
        local cached = session.cache[screen]
        local source

        if cached == nil then
            cached = {}
            session.cache[screen] = cached
        end
        if cached.text ~= nil then
            opts.max_line = cached.widest
            source = mc.source.bytes(cached.text)
        elseif mc.source.generator == nil then
            cached.text = md.render(session.text, opts)
            cached.widest = opts.max_line
            source = mc.source.bytes(cached.text)
        else
            local next_block = md.blocks(session.text, opts)
            local pieces = {}
            local lines = 0
            local done = false
            -- Fill the first screen before handing the rest to the event loop.
            repeat
                local chunk = next_block()
                if chunk == nil then
                    done = true
                else
                    pieces[#pieces + 1] = chunk
                    lines = lines + select(2, chunk:gsub("\n", ""))
                end
            until done or lines >= viewport.lines
            local initial = table.concat(pieces)
            if done then
                cached.text = initial
                cached.widest = opts.max_line
                source = mc.source.bytes(initial)
            else
                local produced = #initial
                source = mc.source.generator {
                    initial = initial,
                    next = function()
                        if pieces == nil then
                            return nil
                        end
                        local chunk = next_block()
                        if chunk == nil then
                            cached.text = table.concat(pieces)
                            cached.widest = opts.max_line
                            pieces = nil
                            return nil
                        end
                        produced = produced + #chunk
                        if produced > MAX_RENDERED then
                            pieces = nil
                            return "\nThe document is longer than the viewer keeps."
                                .. "  The rest is not rendered.\n"
                        end
                        pieces[#pieces + 1] = chunk
                        return chunk
                    end,
                }
            end
        end
        -- What is wider than the screen but no wider than a diagram is laid
        -- out is left whole and scrolled sideways; the prose is wrapped to
        -- the screen already.
        -- The widest block is looked up before the first screen is rendered,
        -- because a diagram halfway down the file counts as well.
        if cached.unwrapped == nil then
            cached.unwrapped = measure.unwrapped_width(session.text, width, screen)
        end
        local widest = math.max(cached.unwrapped, opts.max_line or 0)
        local wrap = nil
        if widest > viewport.columns and widest <= cfg.DIAGRAM_WIDTH then
            wrap = false
        end
        return {
            source = source,
            title = session.title,
            raw_path = session.raw_path,
            initial_display = "nroff",
            auto_scroll = "top",
            wrap = wrap,
        }
    end,
    close = function() end,
}

local function read_file(path)
    local file, err = io.open(path, "rb")
    if file == nil then
        return nil, err
    end
    local size = file:seek("end")
    if size == nil or size > MAX_FILE_SIZE then
        file:close()
        return nil, "too large"
    end
    file:seek("set", 0)
    local text = file:read("a")
    file:close()
    if text == nil then
        return nil, "cannot read"
    end
    return text
end

local function view_file(request)
    if request.local_path == nil then
        return nil, "not_supported"
    end
    local text, err = read_file(request.local_path)
    if text == nil then
        mc.log.info(request.display_name .. ": " .. err)
        -- A file too large to render still belongs here: handing it back
        -- would start the much slower handler of mc.ext.ini on it.
        if err ~= "too large" then
            return nil, "not_supported"
        end
    end
    local controller, create_err = viewer:create {
        text = text,
        title = request.display_name,
        raw_path = request.local_path,
    }
    if controller == nil then
        return nil, create_err
    end
    return { handled = true, controller = controller }
end

mc.file_handler.register { id = "view", kind = "view", handler = view_file }

-- The same settings, reached from Manage Plugins for a reader who is not
-- looking at a document right now.
if mc.settings ~= nil then
    mc.settings(function()
        settings.dialog()
    end)
end
