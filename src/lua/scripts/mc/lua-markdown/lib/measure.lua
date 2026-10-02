-- How wide a document is where it cannot be wrapped.  The viewer is told
-- from this whether a line has to be broken or can be scrolled sideways.

local cfg = require("config")
local codeblock = require("block.code")
local scanner = require("block.scanner")

local code_width = codeblock.line_width
local mermaid_lines = codeblock.mermaid
local emit_code = codeblock.emit

local split_lines = scanner.split_lines
local fence_of = scanner.fence_of
local read_fence = scanner.read_fence
local read_indented = scanner.read_indented

local M = {}

-- How wide the blocks that cannot be wrapped are: a fenced block, a diagram
-- drawn in place of one, and an indented block of code.  The prose around
-- them is flowed to the screen and never needs this.  Only the fences are
-- rendered, and only those of the first cfg.UNWRAPPED_SCAN bytes, so that the
-- walk stays cheap on a document the viewer renders block by block.
function M.unwrapped_width(text, width_limit, screen)
    local lines = split_lines(#text > cfg.UNWRAPPED_SCAN and text:sub(1, cfg.UNWRAPPED_SCAN) or text)
    local most = 0
    local i = 1

    local function widest(taken)
        for _, line in ipairs(taken) do
            local w = code_width(line)

            if w > most then
                most = w
            end
        end
    end

    local function measure(block, language)
        local out = {}

        emit_code(block, out, width_limit, language, screen)
        widest(out)
    end

    while i <= #lines do
        local line = lines[i]
        local fence = fence_of(line)

        if fence ~= nil then
            local language, code
            local drawn = {}

            language, code, i = read_fence(lines, i, fence)
            if mermaid_lines(table.concat(code, "\n"), language, drawn, width_limit) then
                widest(drawn)
            else
                measure(code, language)
            end
        elseif line:match("^    ") or line:match("^\t") then
            local block

            block, i = read_indented(lines, i)
            measure(block, nil)
        else
            i = i + 1
        end
    end
    return most
end

return M
