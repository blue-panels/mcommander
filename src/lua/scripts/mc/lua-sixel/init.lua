-- F3 on a picture: sixel where the terminal draws it, chafa's symbols where
-- it does not (params.mode "sixel" or "symbols" forces one). i switches
-- between the picture and its properties.

local next_session_id = 0

-- One line about the picture (identify's format, size, depth and frame
-- count, or file's description), then the picture in the rows left under
-- it: a picture that does not fit whole is not drawn.  Without chafa there
-- is no picture, and a line says what to install.  i shows the full
-- properties instead, the way misc/ext.d/image.sh printed them on F3
-- (exif, or exiftool when exif is missing or fails, then identify).
--
-- $1 file, $2 "sixel", "symbols" or "properties", $3 columns, $4 lines,
-- $5 and $6 the cell in pixels (sixel), $7... the rest of chafa's arguments.
local render_script = [[
file=$1
mode=$2
cols=$3
lines=$4
cell_w=$5
cell_h=$6
shift 6

ident=
if command -v identify >/dev/null 2>&1; then
    # first frame only: an animated GIF has one line per frame
    ident=$(identify -ping -format '%m %wx%h %z-bit, %n frame(s)\n' "$file" 2>/dev/null | head -n 1)
fi

if [ "$mode" = properties ]; then
    props=
    if command -v exif >/dev/null 2>&1; then
        props=$(exif "$file" 2>/dev/null)
    fi
    if [ -z "$props" ] && command -v exiftool >/dev/null 2>&1; then
        props=$(exiftool "$file" 2>/dev/null)
    fi
    [ -n "$props" ] && printf '%s\n' "$props"
    [ -n "$ident" ] && printf '%s\n' "$ident"
    [ -z "$props$ident" ] && echo "Install exif, exiftool or ImageMagick to see the properties here."
    exit 0
fi

[ -z "$ident" ] && ident=$(file -b -- "$file" 2>/dev/null)
[ -z "$ident" ] && ident=$(basename -- "$file")
printf '%s\n' "$ident"
rows=$((lines - 1))
[ "$rows" -lt 1 ] && rows=1

if ! command -v chafa >/dev/null 2>&1; then
    echo "Install chafa to see the picture here."
    exit 0
fi

if [ "$mode" = sixel ]; then
    # chafa with no terminal to ask has a cell of its own size (10x20 in
    # 1.18, 8x8 in older ones under --font-ratio=1/1). A picture one cell
    # big is that cell exactly: its raster attributes say the size.
    set -- --font-ratio=1/1 "$@"
    cell=$(chafa --format=sixel --animate=off --size=1x1 "$@" -- "$file" 2>/dev/null \
        | sed -n 's/.*"1;1;\([0-9][0-9]*\);\([0-9][0-9]*\).*/\1 \2/p' | head -n 1)
    chafa_w=${cell% *}
    chafa_h=${cell#* }
    if [ -z "$cell" ] || [ "$chafa_w" -le 0 ] || [ "$chafa_h" -le 0 ]; then
        chafa_w=8
        chafa_h=8
    fi
    size_w=$((cols * cell_w / chafa_w))
    size_h=$((rows * cell_h / chafa_h))
    [ "$size_w" -lt 1 ] && size_w=1
    [ "$size_h" -lt 1 ] && size_h=1
    size=${size_w}x${size_h}
else
    size=${cols}x${rows}
fi

exec chafa --format="$mode" --animate=off --size="$size" "$@" -- "$file"
]]

-- Whether the picture is drawn in sixel: by the mode chosen, and by whether
-- the terminal draws it, which the viewport says by carrying pixels.
local function want_sixel(params, viewport)
    if params.mode == "symbols" then
        return false
    end
    if params.mode == "sixel" then
        return true
    end
    return viewport.pixel_width ~= nil
end

local viewer = mc.viewer_source.define {
    id = "sixel-image",
    resize = "rebuild",
    options_key = "i",
    help = { file = "help.md", node = "[Image Viewer]" },

    open = function(request)
        next_session_id = next_session_id + 1
        return {
            local_path = request.local_path,
            display_name = request.display_name,
            indicator_id = "render-" .. next_session_id,
        }
    end,

    initial_params = function(_, params)
        params.mode = params.mode or "auto"
        params.show = params.show or "picture"
        return params
    end,

    prepare = function(session, params, viewport)
        local argv
        if params.show == "properties" then
            argv = {
                "sh", "-c", render_script, "lua-sixel",
                session.local_path, "properties",
                tostring(viewport.columns), tostring(viewport.lines), "0", "0",
            }
        elseif want_sixel(params, viewport) then
            -- Without pixels (the mode forced) the cell is taken for 8x16,
            -- the guess vterm makes too.
            local cell_w = viewport.pixel_width and viewport.pixel_width // viewport.columns or 8
            local cell_h = viewport.pixel_height and viewport.pixel_height // viewport.lines or 16
            argv = {
                "sh", "-c", render_script, "lua-sixel",
                session.local_path, "sixel",
                tostring(viewport.columns), tostring(viewport.lines),
                tostring(cell_w), tostring(cell_h),
            }
        else
            argv = {
                "sh", "-c", render_script, "lua-sixel",
                session.local_path, "symbols",
                tostring(viewport.columns), tostring(viewport.lines), "0", "0",
                "--colors=256",
            }
        end
        return {
            source = mc.source.process {
                argv = argv,
                stderr = "separate",
            },
            title = session.display_name,
            raw_path = session.local_path,
            initial_display = "terminal",
            auto_scroll = "top",
        }
    end,

    -- i: the picture or its properties, no dialog.
    options = function(_, params)
        return { mode = params.mode, show = params.show == "properties" and "picture" or "properties" }
    end,

    source_state = function(session, event)
        if event.state == "started" then
            session.generation = event.generation
            mc.ui.indicator {
                id = session.indicator_id,
                area = "viewer",
                text = "Rendering image...",
            }
        elseif session.generation == event.generation then
            mc.ui.indicator_clear(session.indicator_id, "viewer")
            session.generation = nil
        end
    end,

    close = function(session)
        mc.ui.indicator_clear(session.indicator_id, "viewer")
    end,
}

mc.file_handler.register {
    id = "view",
    kind = "view",
    handler = function(request)
        return {
            handled = true,
            controller = viewer:create(request),
        }
    end,
}
