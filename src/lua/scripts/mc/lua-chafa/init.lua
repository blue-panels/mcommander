-- F3 on a picture: one line about it, then chafa's symbols in the rows left.
-- i switches between the picture and its properties.

local next_session_id = 0

-- One line about the picture (identify's format, size, depth and frame
-- count, or file's description), then the chafa rendering in the rows left
-- under it.  With "properties" the full properties instead, the way
-- misc/ext.d/image.sh printed them on F3 (exif, or exiftool when exif is
-- missing or fails, then identify).
--
-- $1 file, $2 "picture" or "properties", $3 columns, $4 lines.
local render_script = [[
file=$1
mode=$2
cols=$3
lines=$4

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

# chafa asks the terminal and waits; its answers would come to mc as keys
probe_off=
chafa --help 2>/dev/null | grep -q -e '--probe=' && probe_off=--probe=off
exec chafa --format=symbols --colors=256 --animate=off --size="${cols}x${rows}" $probe_off -- "$file"
]]

local viewer = mc.viewer_source.define {
    id = "chafa-image",
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
        params.show = params.show or "picture"
        return params
    end,

    prepare = function(session, params, viewport)
        return {
            source = mc.source.process {
                argv = {
                    "sh", "-c", render_script, "lua-chafa",
                    session.local_path, params.show,
                    tostring(viewport.columns), tostring(viewport.lines),
                },
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
