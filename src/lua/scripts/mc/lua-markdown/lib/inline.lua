-- Inline markup.  The line is cut into tokens: text, code, a formula, a
-- link, or a run of * or _ that may open or close emphasis.  The runs are
-- then paired the way CommonMark pairs them, and the text is written out
-- with the styles that are open at each point.
--
-- What the document defines for this pass is carried in doc, which the
-- caller owns: the link reference definitions by label, the footnotes, and
-- the numbers footnotes get in the order they are first referenced.  Two
-- documents can therefore be rendered at once.

local txt = require("text")
local sgr = require("sgr")
local latex = require("formula.latex")
local env = require("formula.env")

local trim = txt.trim
local NBSP = txt.NBSP

local styled = sgr.styled
local link_start = sgr.link_start
local LINK_END = sgr.LINK_END

local render_math = latex.render
local render_math_inline = env.render_inline
local find_math_end = env.find_end

local M = {}

-- An empty document: no reference definition and no footnote seen yet.
function M.new_document()
    return { refs = {}, notes = {}, note_defs = {}, note_order = {}, note_number = {} }
end

-- The elements of html a markdown text is written with.  A tag of one of
-- them is dropped and what it holds is shown; anything else between angle
-- brackets is text, the way a placeholder such as <year> or <name of author>
-- is meant to be read.
local html_tags = {}
for name in ([[
a abbr address article aside audio b bdi bdo big blockquote body br button
caption center cite code col colgroup dd del details dfn div dl dt em embed
figcaption figure font footer form h1 h2 h3 h4 h5 h6 head header hr html i
iframe img input ins kbd label li link main mark meta nav noscript object ol
p param picture pre q rp rt ruby s samp script section select small source
span strike strong style sub summary sup svg table tbody td template textarea
tfoot th thead title tr tt u ul var video wbr
]]):gmatch("%S+") do
    html_tags[name] = true
end

-- Where a tag of html that starts at i ends, or nil when there is none there.
local function html_tag_end(s, i)
    local comment = s:match("^<!%-%-.-%-%->()", i) or s:match("^<!%a[^<>]*>()", i)

    if comment ~= nil then
        return comment
    end

    local name, e = s:match("^</?(%a[%w]*)[^<>]*>()", i)

    if name ~= nil and html_tags[name:lower()] then
        local rest = s:sub(i, e - 1):match("^</?%a[%w]*(.-)/?>$")

        -- what follows the name is attributes, not words
        if rest == "" or rest:match("^%s+[%a_:][%w_:.-]*%s*=") or rest:match("^%s+[%a_:][%w_:.-]*%s*$") then
            return e
        end
    end
    return nil
end

-- The text without the tags of html in it.
local function strip_tags(s)
    local out = {}
    local i = 1

    while i <= #s do
        local lt = s:find("<", i, true)

        if lt == nil then
            out[#out + 1] = s:sub(i)
            break
        end
        out[#out + 1] = s:sub(i, lt - 1)

        local e = html_tag_end(s, lt)

        if e ~= nil then
            i = e
        else
            out[#out + 1] = "<"
            i = lt + 1
        end
    end
    return table.concat(out)
end

local entities = {
    amp = "&", lt = "<", gt = ">", quot = "\"", apos = "'", nbsp = " ",
    copy = "\u{00A9}", reg = "\u{00AE}", trade = "\u{2122}", hellip = "\u{2026}",
    mdash = "\u{2014}", ndash = "\u{2013}", laquo = "\u{00AB}", raquo = "\u{00BB}",
    bull = "\u{2022}", middot = "\u{00B7}", times = "\u{00D7}", deg = "\u{00B0}",
    larr = "\u{2190}", rarr = "\u{2192}", lsquo = "\u{2018}", rsquo = "\u{2019}",
    ldquo = "\u{201C}", rdquo = "\u{201D}",
}

local function is_punct(ch)
    return ch ~= "" and ch:match("^%p$") ~= nil
end

local function is_space(ch)
    return ch == "" or ch:match("^%s$") ~= nil
end

-- Whether a run of * or _ can open or close emphasis, from what is around
-- it (CommonMark's left- and right-flanking rules).
local function flanking(before, after, ch)
    local left = not is_space(after) and (not is_punct(after) or is_space(before) or is_punct(before))
    local right = not is_space(before) and (not is_punct(before) or is_space(after) or is_punct(after))
    if ch == "_" then
        return left and (not right or is_punct(before)), right and (not left or is_punct(after))
    end
    return left, right
end

local tokenize
local pair_emphasis

local function normalize_label(label)
    return (trim(label):gsub("%s+", " "):lower())
end

-- The number of the footnote label refers to, given on its first reference,
-- or nil if the document has no such footnote.
local function footnote_number(doc, label)
    local key = normalize_label(label)
    if doc.notes[key] == nil then
        return nil
    end
    if doc.note_number[key] == nil then
        doc.note_order[#doc.note_order + 1] = key
        doc.note_number[key] = #doc.note_order
    end
    return doc.note_number[key]
end

-- The tokens of s.  Each is { kind, text } for plain text, { "code", text },
-- { "link", tokens, url }, or { "delim", ch, count, can_open, can_close }.
tokenize = function(s, doc)
    local tokens = {}
    local text = {}
    local n = #s
    local i = 1

    local function flush()
        if #text > 0 then
            tokens[#tokens + 1] = { "text", table.concat(text) }
            text = {}
        end
    end

    while i <= n do
        local ch = s:sub(i, i)
        if ch == "\\" and is_punct(s:sub(i + 1, i + 1)) then
            text[#text + 1] = s:sub(i + 1, i + 1)
            i = i + 2
        elseif ch == "`" then
            local _, e = s:find("^`+", i)
            local fence = s:sub(i, e)
            local a, b = s:find(fence, e + 1, true)
            while a ~= nil and s:sub(b + 1, b + 1) == "`" do
                local _, e2 = s:find("^`+", b + 1)
                a, b = s:find(fence, e2 + 1, true)
            end
            if a == nil then
                text[#text + 1] = fence
                i = e + 1
            else
                local code = s:sub(e + 1, a - 1)
                if code:match("^ .* $") and code:match("%S") then
                    code = code:sub(2, -2)
                end
                flush()
                tokens[#tokens + 1] = { "code", code }
                i = b + 1
            end
        elseif ch == "$" then
            local delim = s:sub(i, i + 1) == "$$" and "$$" or "$"
            local a, b = find_math_end(s, i + #delim, delim)
            if a == nil then
                text[#text + 1] = ch
                i = i + 1
            else
                -- a formula is not broken across lines
                local formula = s:sub(i + #delim, a - 1)
                local rendered = formula:find("\\begin{", 1, true) and render_math_inline(formula)
                    or render_math(formula)

                text[#text + 1] = (rendered:gsub(" ", NBSP))
                i = b + 1
            end
        elseif ch == "&" then
            local name, e = s:match("^&(#?%w+);()", i)
            local value
            if name ~= nil then
                if name:match("^#[xX]%x+$") then
                    value = utf8.char(tonumber(name:sub(3), 16))
                elseif name:match("^#%d+$") then
                    value = utf8.char(tonumber(name:sub(2)))
                else
                    value = entities[name]
                end
            end
            if value ~= nil then
                text[#text + 1] = value
                i = e
            else
                text[#text + 1] = ch
                i = i + 1
            end
        elseif ch == "<" and s:match("^<[iI][mM][gG][%s>]", i) then
            -- a picture of html: named, not drawn, like a markdown one
            local tag, e = s:match("^(<[^<>]*>)()", i)

            if tag == nil then
                text[#text + 1] = ch
                i = i + 1
            else
                local src = tag:match("[sS][rR][cC]%s*=%s*\"([^\"]*)\"")
                    or tag:match("[sS][rR][cC]%s*=%s*'([^']*)'")
                local alt = tag:match("[aA][lL][tT]%s*=%s*\"([^\"]*)\"")
                    or tag:match("[aA][lL][tT]%s*=%s*'([^']*)'")

                text[#text + 1] = "[" .. (alt or "") .. "]"
                if src ~= nil and src ~= "" then
                    text[#text + 1] = " <" .. src .. ">"
                end
                i = e
            end
        elseif ch == "<" then
            local url, e = s:match("^<(%a[%w+.-]*:[^%s<>]*)>()", i)
            if url == nil then
                url, e = s:match("^<([%w.+-]+@[%w.-]+)>()", i)
            end
            local tag_end = url == nil and html_tag_end(s, i) or nil
            if url ~= nil then
                flush()
                local target = url:find(":", 1, true) and url or "mailto:" .. url
                tokens[#tokens + 1] = { "link", { { "text", url } }, target }
                i = e
            elseif tag_end ~= nil then
                if s:sub(i, tag_end - 1):match("^<[Bb][Rr]%s*/?>$") then
                    text[#text + 1] = "\n"
                end
                i = tag_end
            else
                text[#text + 1] = ch
                i = i + 1
            end
        elseif ch == "!" and s:sub(i + 1, i + 1) == "[" then
            -- a picture is not drawn: its text and where it comes from are
            local alt, target, e = s:match("^!%[([^%]]*)%]%(([^%s)]*)[^)]*%)()", i)
            local ref

            if alt == nil then
                alt, ref, e = s:match("^!%[([^%]]*)%]%[([^%]]*)%]()", i)
                if alt ~= nil then
                    target = doc.refs[normalize_label(ref ~= "" and ref or alt)]
                end
            end
            if alt == nil then
                alt, e = s:match("^!%[([^%]]*)%]()", i)
                if alt ~= nil then
                    target = doc.refs[normalize_label(alt)]
                end
            end
            if alt ~= nil then
                text[#text + 1] = "[" .. alt .. "]"
                if target ~= nil and target ~= "" and target ~= alt then
                    text[#text + 1] = " <" .. target .. ">"
                end
                i = e
            else
                text[#text + 1] = ch
                i = i + 1
            end
        elseif ch == "[" then
            local note_label, note_end = s:match("^%[%^([^%]%s]+)%]()", i)
            local number = note_label ~= nil and footnote_number(doc, note_label) or nil
            if number ~= nil then
                text[#text + 1] = "[" .. number .. "]"
                i = note_end
            else
                local label, e = s:match("^%[([^%[%]]*)%]()", i)
                local url
                if label ~= nil then
                    local target, e2 = s:match("^%(([^%s)]*)[^)]*%)()", e)
                    if target ~= nil then
                        url = target
                        e = e2
                    else
                        -- [text][ref], [text][] or a bare [ref]
                        local ref, ref_end = s:match("^%[([^%]]*)%]()", e)
                        if ref ~= nil then
                            e = ref_end
                            url = doc.refs[normalize_label(ref ~= "" and ref or label)]
                        else
                            url = doc.refs[normalize_label(label)]
                        end
                    end
                end
                if label ~= nil and label ~= "" and (url ~= nil or e > i + #label + 2) then
                    flush()
                    local label_tokens = tokenize(label, doc)
                    pair_emphasis(label_tokens)
                    tokens[#tokens + 1] = { "link", label_tokens, url }
                    i = e
                else
                    text[#text + 1] = ch
                    i = i + 1
                end
            end
        elseif ch == "*" or ch == "_" or ch == "~" then
            local _, e = s:find("^" .. (ch == "*" and "%*+" or ch == "_" and "_+" or "~+"), i)
            local before = s:sub(i - 1, i - 1)
            local after = s:sub(e + 1, e + 1)
            local can_open, can_close = flanking(before, after, ch)
            if can_open or can_close then
                flush()
                tokens[#tokens + 1] = { "delim", ch, e - i + 1, can_open, can_close }
            else
                text[#text + 1] = s:sub(i, e)
            end
            i = e + 1
        else
            -- Keep ordinary text together, including all bytes of UTF-8 characters.
            local e = s:find("[\\`$&<!%[*_~]", i + 1) or (n + 1)
            text[#text + 1] = s:sub(i, e - 1)
            i = e
        end
    end
    flush()
    return tokens
end

-- Pair the delimiter runs: each closer takes the nearest opener of the same
-- character; two or more on both sides make strong emphasis, one makes
-- emphasis.  Runs left unpaired are text.
pair_emphasis = function(tokens)
    for c = 1, #tokens do
        local closer = tokens[c]
        while closer[1] == "delim" and closer[5] and closer[3] > 0 do
            local opener
            for o = c - 1, 1, -1 do
                local t = tokens[o]
                if t[1] == "delim" and t[2] == closer[2] and t[4] and t[3] > 0 then
                    local odd = (t[5] or closer[4]) and (t[3] + closer[3]) % 3 == 0
                        and not (t[3] % 3 == 0 and closer[3] % 3 == 0)
                    if not odd then
                        opener = t
                        break
                    end
                end
            end
            if opener == nil then
                break
            end
            local use
            if closer[2] == "~" then
                use = 3  -- strike: one or two tildes, all of the run at once
                opener[3], closer[3] = 1, 1
            else
                use = (opener[3] >= 2 and closer[3] >= 2) and 2 or 1
            end
            opener[3] = opener[3] - use
            closer[3] = closer[3] - use
            opener.opens = opener.opens or {}
            closer.closes = closer.closes or {}
            table.insert(opener.opens, use)
            table.insert(closer.closes, 1, use)
        end
    end
end

local render_tokens

render_tokens = function(tokens, style, out)
    local bold, italic, strike = 0, 0, 0
    local function current()
        return {
            bold = style.bold or bold > 0,
            under = style.under,
            italic = style.italic or italic > 0,
            strike = style.strike or strike > 0,
            heading = style.heading,
        }
    end
    for _, t in ipairs(tokens) do
        if t[1] == "text" then
            out[#out + 1] = styled(t[2], current())
        elseif t[1] == "code" then
            out[#out + 1] = styled(t[2], { under = true, heading = style.heading })
        elseif t[1] == "link" and (t[3] == nil or t[3] == "") then
            -- a link without a target is only underlined
            local s = current()
            s.under = true
            render_tokens(t[2], s, out)
        elseif t[1] == "link" then
            -- the viewer underlines the text of an OSC 8 link itself
            local label = {}
            render_tokens(t[2], current(), label)
            local lead, body, tail = table.concat(label):match("^( *)(.-)( *)$")
            out[#out + 1] = lead .. link_start(t[3]) .. body .. LINK_END .. tail
            local plain = {}
            render_tokens(t[2], {}, plain)
            local shown = t[3]:gsub("^mailto:", "")
            if table.concat(plain) ~= shown then
                out[#out + 1] = styled(" <" .. t[3] .. ">", current())
            end
        else
            if t.closes then
                for _, use in ipairs(t.closes) do
                    if use == 3 then
                        strike = strike - 1
                    elseif use == 2 then
                        bold = bold - 1
                    else
                        italic = italic - 1
                    end
                end
            end
            if t[3] > 0 then
                out[#out + 1] = styled(t[2]:rep(t[3]), current())
            end
            if t.opens then
                for _, use in ipairs(t.opens) do
                    if use == 3 then
                        strike = strike + 1
                    elseif use == 2 then
                        bold = bold + 1
                    else
                        italic = italic + 1
                    end
                end
            end
        end
    end
end

local function inline(s, style, doc)
    local tokens = tokenize(s, doc)
    pair_emphasis(tokens)
    local out = {}
    render_tokens(tokens, style or {}, out)
    return table.concat(out)
end

M.normalize_label = normalize_label
M.footnote_number = footnote_number
M.render = inline
M.strip_tags = strip_tags

return M
