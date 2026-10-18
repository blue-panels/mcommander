mc_lua = shared_module('mc-lua',
  'mc-lua.c',
  c_args: [
    '-DMC_LUA_SYSTEM_SCRIPTS_DIR="@0@"'.format(lua_scripts_dir),
    '-DMC_LUA_SYSTEM_MODULES_DIR="@0@"'.format(lua_modules_dir),
  ],
  dependencies: [glib_dep, lua],
  name_prefix: '',
  name_suffix: 'so',
  implicit_include_directories: false,
  install: true,
  install_dir: runtime_plugins_dir,
)

# the scripts the plugin loads; the viewers that run an external tool are
# installed only when the tool is there
if have_lua_chafa
  install_data(
    'scripts/mc/lua-chafa/lua.ini',
    'scripts/mc/lua-chafa/init.lua',
    'scripts/mc/lua-chafa/help.md',
    install_dir: lua_scripts_dir / 'mc/lua-chafa',
  )
endif

install_data(
  'scripts/mc/lua-sixel/lua.ini',
  'scripts/mc/lua-sixel/init.lua',
  'scripts/mc/lua-sixel/help.md',
  install_dir: lua_scripts_dir / 'mc/lua-sixel',
)

if have_lua_java_class
  install_data(
    'scripts/mc/lua-java-class/lua.ini',
    'scripts/mc/lua-java-class/init.lua',
    install_dir: lua_scripts_dir / 'mc/lua-java-class',
  )
endif

install_data(
  'scripts/mc/lua-dbf/lua.ini',
  'scripts/mc/lua-dbf/init.lua',
  'scripts/mc/lua-dbf/help.md',
  install_dir: lua_scripts_dir / 'mc/lua-dbf',
)

install_data(
  'scripts/mc/lua-markdown/lua.ini',
  'scripts/mc/lua-markdown/init.lua',
  'scripts/mc/lua-markdown/help.md',
  install_dir: lua_scripts_dir / 'mc/lua-markdown',
)

install_data(
  'scripts/mc/lua-markdown/lib/config.lua',
  'scripts/mc/lua-markdown/lib/text.lua',
  'scripts/mc/lua-markdown/lib/sgr.lua',
  'scripts/mc/lua-markdown/lib/spans.lua',
  'scripts/mc/lua-markdown/lib/inline.lua',
  'scripts/mc/lua-markdown/lib/document.lua',
  'scripts/mc/lua-markdown/lib/measure.lua',
  'scripts/mc/lua-markdown/lib/settings.lua',
  'scripts/mc/lua-markdown/lib/mermaid.lua',
  install_dir: lua_scripts_dir / 'mc/lua-markdown/lib',
)

install_data(
  'scripts/mc/lua-markdown/lib/formula/latex.lua',
  'scripts/mc/lua-markdown/lib/formula/env.lua',
  install_dir: lua_scripts_dir / 'mc/lua-markdown/lib/formula',
)

install_data(
  'scripts/mc/lua-markdown/lib/block/scanner.lua',
  'scripts/mc/lua-markdown/lib/block/definitions.lua',
  'scripts/mc/lua-markdown/lib/block/code.lua',
  'scripts/mc/lua-markdown/lib/block/table.lua',
  'scripts/mc/lua-markdown/lib/block/flow.lua',
  install_dir: lua_scripts_dir / 'mc/lua-markdown/lib/block',
)

install_data(
  'scripts/mc/lua-markdown/lib/mermaid/glyphs.lua',
  'scripts/mc/lua-markdown/lib/mermaid/canvas.lua',
  'scripts/mc/lua-markdown/lib/mermaid/flowchart.lua',
  'scripts/mc/lua-markdown/lib/mermaid/class.lua',
  'scripts/mc/lua-markdown/lib/mermaid/pie.lua',
  'scripts/mc/lua-markdown/lib/mermaid/sequence.lua',
  install_dir: lua_scripts_dir / 'mc/lua-markdown/lib/mermaid',
)

install_data(
  'scripts/mc/lua-pdf/lua.ini',
  'scripts/mc/lua-pdf/init.lua',
  'scripts/mc/lua-pdf/help.md',
  install_dir: lua_scripts_dir / 'mc/lua-pdf',
)

install_data(
  'scripts/mc/lua-pdf/lib/pdfxml.lua',
  'scripts/mc/lua-pdf/lib/render.lua',
  install_dir: lua_scripts_dir / 'mc/lua-pdf/lib',
)

if have_lua_readelf
  install_data(
    'scripts/mc/lua-readelf/lua.ini',
    'scripts/mc/lua-readelf/init.lua',
    install_dir: lua_scripts_dir / 'mc/lua-readelf',
  )
endif

install_data(
  'scripts/editor/base64-decode/lua.ini',
  'scripts/editor/base64-decode/init.lua',
  install_dir: lua_scripts_dir / 'editor/base64-decode',
)

install_data(
  'scripts/editor/insert-literal/lua.ini',
  'scripts/editor/insert-literal/init.lua',
  install_dir: lua_scripts_dir / 'editor/insert-literal',
)

install_data(
  'scripts/editor/insert-datetime/lua.ini',
  'scripts/editor/insert-datetime/init.lua',
  install_dir: lua_scripts_dir / 'editor/insert-datetime',
)

install_data(
  'scripts/editor/format-paragraph/lua.ini',
  'scripts/editor/format-paragraph/init.lua',
  install_dir: lua_scripts_dir / 'editor/format-paragraph',
)

install_data(
  'scripts/editor/sort-selection/lua.ini',
  'scripts/editor/sort-selection/init.lua',
  install_dir: lua_scripts_dir / 'editor/sort-selection',
)

install_data(
  'scripts/editor/insert-command-output/lua.ini',
  'scripts/editor/insert-command-output/init.lua',
  install_dir: lua_scripts_dir / 'editor/insert-command-output',
)

install_data(
  'scripts/editor/draw-table/lua.ini',
  'scripts/editor/draw-table/init.lua',
  install_dir: lua_scripts_dir / 'editor/draw-table',
)

# teaching examples: installed for reference only, never loaded by the engine
lua_examples_dir = pkgdatadir / 'lua' / 'examples'

install_data(
  'examples/mc/demo-panel/lua.ini',
  'examples/mc/demo-panel/init.lua',
  install_dir: lua_examples_dir / 'mc/demo-panel',
)

install_data(
  'examples/editor/notify-editor-save/lua.ini',
  'examples/editor/notify-editor-save/init.lua',
  install_dir: lua_examples_dir / 'editor/notify-editor-save',
)
