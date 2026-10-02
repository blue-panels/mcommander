-- Tables: each column as wide as its widest cell, squeezed to the screen
-- when they do not all fit, the cells then wrapped inside their column.

local cfg = require("config")
local spans = require("spans")
local inl = require("inline")
local scanner = require("block.scanner")

local units_of = spans.of_text
local units_width = spans.width
local sgr_line = spans.line
local wrap_units = spans.wrap

local inline = inl.render
local split_row = scanner.split_row
local is_table_sep = scanner.is_table_sep

local BOX_H, BOX_V, BOX_X = cfg.BOX_H, cfg.BOX_V, cfg.BOX_X

local M = {}

local function render_table(lines, out, width_limit, doc)
    local rows = {}
    local align = {}
    local maxc = 0
    for _, line in ipairs(lines) do
        local fields = split_row(line)
        if is_table_sep(line) then
            for c, f in ipairs(fields) do
                if f:match("^:%-+:$") then
                    align[c] = "center"
                elseif f:match(":$") then
                    align[c] = "right"
                end
            end
        else
            rows[#rows + 1] = fields
            if #fields > maxc then
                maxc = #fields
            end
        end
    end
    if #rows == 0 then
        return
    end

    -- Each column as wide as its widest cell; when that does not fit the
    -- screen, the columns that take more than a fair share of what is left
    -- give up width equally, and their cells are wrapped to it.
    local units = {}
    local colw = {}
    for c = 1, maxc do
        colw[c] = 0
    end
    for r, row in ipairs(rows) do
        units[r] = {}
        for c = 1, maxc do
            -- a <br> in a cell starts a line of its own in it
            local lines = {}
            for rendered in (inline(row[c] or "", { bold = r == 1 }, doc) .. "\n"):gmatch("(.-)\n") do
                lines[#lines + 1] = units_of(rendered)
                if units_width(lines[#lines]) > colw[c] then
                    colw[c] = units_width(lines[#lines])
                end
            end
            units[r][c] = lines
        end
    end
    local remaining = width_limit - (maxc - 1) * 3
    local total = 0
    for c = 1, maxc do
        total = total + colw[c]
    end
    if total > remaining then
        local flex = {}
        for c = 1, maxc do
            flex[#flex + 1] = c
        end
        local settled = true
        while settled and #flex > 0 do
            local share = remaining // #flex
            settled = false
            for k = #flex, 1, -1 do
                local c = flex[k]
                if colw[c] <= share then
                    remaining = remaining - colw[c]
                    table.remove(flex, k)
                    settled = true
                end
            end
        end
        for k, c in ipairs(flex) do
            local share = remaining // #flex + (k <= remaining % #flex and 1 or 0)
            colw[c] = math.max(share, cfg.MIN_COLUMN)
        end
    end

    local cells = {}
    local cell_sgr = {}
    for r = 1, #rows do
        cells[r] = {}
        cell_sgr[r] = {}
        for c = 1, maxc do
            cells[r][c] = {}
            for _, u in ipairs(units[r][c]) do
                for _, seg in ipairs(units_width(u) > colw[c] and wrap_units(u, colw[c]) or { u }) do
                    cells[r][c][#cells[r][c] + 1] = seg
                end
            end
            cell_sgr[r][c] = {}
        end
    end

    local rule = {}
    for c = 1, maxc do
        rule[c] = BOX_H:rep(colw[c])
    end
    rule = table.concat(rule, BOX_H .. BOX_X .. BOX_H)

    for r = 1, #rows do
        local height = 1
        for c = 1, maxc do
            if #cells[r][c] > height then
                height = #cells[r][c]
            end
        end
        for k = 1, height do
            local parts = {}
            for c = 1, maxc do
                local cell = cells[r][c][k] or {}
                local pad = colw[c] - units_width(cell)
                local left, right = 0, pad
                if align[c] == "right" then
                    left, right = pad, 0
                elseif align[c] == "center" then
                    left = pad // 2
                    right = pad - left
                end
                parts[c] = (" "):rep(left) .. sgr_line(cell, cell_sgr[r][c]) .. (" "):rep(right)
            end
            out[#out + 1] = table.concat(parts, " " .. BOX_V .. " ")
        end
        out[#out + 1] = rule
    end
end

M.render = render_table

return M
