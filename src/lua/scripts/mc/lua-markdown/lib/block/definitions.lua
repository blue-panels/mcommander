-- What a document says before it is drawn: the link reference definitions
-- and the footnotes, taken out of the lines into a document of their own,
-- and the html a file written for the web carries, turned into the markdown
-- that says the same.

local txt = require("text")
local inl = require("inline")
local scanner = require("block.scanner")

local trim = txt.trim

local normalize_label = inl.normalize_label
local strip_tags = inl.strip_tags

local is_blank = scanner.is_blank
local fence_of = scanner.fence_of

local M = {}

-- The html a document written for the web carries, turned into the markdown
-- that says the same: a heading, a list item, a rule, a row of cells.  What
-- is left of a tag the inline pass drops.
local function html_line(line)
    local body = trim(line)
    local tag, text

    tag, text = body:match("^<[hH](%d)[^<>]*>(.*)$")
    if tag ~= nil then
        return ("#"):rep(math.min(tonumber(tag), 6)) .. " " .. text:gsub("</[hH]%d>%s*$", "")
    end
    if body:match("^<[hH][rR]%s*/?>$") then
        return "---"
    end
    text = body:match("^<[lL][iI][^<>]*>(.*)$")
    if text ~= nil then
        return "- " .. text:gsub("</[lL][iI]>%s*$", "")
    end
    text = body:match("^<[sS][uU][mM][mM][aA][rR][yY][^<>]*>(.*)$")
    if text ~= nil then
        return "**" .. text:gsub("</[sS][uU][mM][mM][aA][rR][yY]>%s*$", "") .. "**"
    end
    if body:match("^<[tT][rR][^<>]*>") then
        -- the cells of a row become the cells of a markdown table
        local cells = {}

        for cell in body:gmatch("<[tTdDhH]+[^<>]*>([^<]*)") do
            if trim(cell) ~= "" then
                cells[#cells + 1] = trim(cell)
            end
        end
        if #cells > 0 then
            return "|" .. table.concat(cells, "|") .. "|", #cells
        end
    end
    -- a line of tags alone says nothing
    if body ~= "" and strip_tags(body):match("^%s*$") then
        return nil
    end
    return line
end

-- Take the link reference definitions and the footnotes out of the lines,
-- into a new document, and drop the lines that hold only an HTML comment;
-- code blocks are left alone.  A footnote goes on over the lines indented
-- under it.  Returns the lines that are left and the document.
local function collect_definitions(lines)
    local kept = {}
    local fence
    local note
    local dropped = false -- a definition was taken out since the last text
    local html_table = false
    local doc = inl.new_document()

    -- a blank line left over where a definition was taken out is not kept
    local function keep(line)
        if not is_blank(line) then
            dropped = false
        elseif dropped and (#kept == 0 or is_blank(kept[#kept])) then
            return
        end
        kept[#kept + 1] = line
    end

    for _, line in ipairs(lines) do
        local label, rest

        if fence == nil and line:find("<", 1, true) ~= nil then
            local converted, cells = html_line(line)

            if converted == nil then
                dropped = true
                goto continue
            end
            line = converted
            if cells ~= nil and not html_table then
                -- the first row of a table is its header; the line that says
                -- so is what the markdown table needs next
                html_table = true
                keep(line)
                line = ("|---"):rep(cells) .. "|"
            end
        elseif html_table and trim(line) == "" then
            html_table = false
        end
        if fence == nil then
            label, rest = line:match("^ ? ? ?%[%^([^%]%s]+)%]:%s*(.*)$")
        end
        if fence == nil and label == nil and line:match("^%s*<!%-%-.-%-%->%s*$") then
            note = nil
            dropped = true
        elseif fence ~= nil then
            local close = line:match("^ ? ? ?([`~]+)%s*$")
            if close ~= nil and close:sub(1, 1) == fence:sub(1, 1) and #close >= #fence then
                fence = nil
            end
            keep(line)
        elseif label ~= nil then
            local key = normalize_label(label)
            note = { rest }
            dropped = true
            if doc.notes[key] == nil then
                doc.notes[key] = note
                doc.note_defs[#doc.note_defs + 1] = key
            end
        elseif note ~= nil and not is_blank(line) and (line:match("^    ") or line:match("^\t")) then
            note[#note + 1] = trim(line)
        else
            note = nil
            local ref, url = line:match("^ ? ? ?%[([^%]^][^%]]*)%]:%s*<?([^%s>]+)>?")
            if ref ~= nil then
                local key = normalize_label(ref)
                dropped = true
                if doc.refs[key] == nil then
                    doc.refs[key] = url
                end
            else
                fence = fence_of(line)
                keep(line)
            end
        end
        ::continue::
    end
    return kept, doc
end


M.collect = collect_definitions

return M
