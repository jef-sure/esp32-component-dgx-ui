# The builder of pages

`index.html` here is a builder of dgx-ui's pages: it opens a project's directory, draws its descriptions as the board
draws them, lets their widgets be selected, set and moved, and saves the files back. It is a page opened from the
disk, with no server and nothing to build; nothing leaves the page.

The drawing is [the preview](preview/README.md), dgx-ui itself built to WebAssembly, so what the panel shows is what
the board shows, and every check of a page is the loader's: the builder repeats no rule of the format.

![The builder: the pages and the widgets of a project at the left, a page drawn at twice its size with its slider
selected, and the slider's properties at the right](../docs/images/builder-page.png)

## Opening a project

Open `index.html` in a browser and press "Open a project": in Chrome and Edge, the File System Access API gives the
page the directory, read and written in place. Other browsers take the project's files by "or choose its files", and a
save comes as a download. A browser that gives a page no folder at all, as those inside VS Code, opens the builder
[from a terminal](#from-a-terminal).

The builder reads the project's own declarations:

- `main/CMakeLists.txt` and `components/*/CMakeLists.txt`: every `dgx_ui_tree()` is a page with its `NAMES` and
  `APPEARANCE`, every `dgx_ui_names()` a names file, every `dgx_ui_appearance()` an appearance with its own `NAMES`,
  read as `dgx_ui_c` reads them ([project_include.cmake](../docs/en/api.md#project_includecmake)).
  A call the builder cannot read as it stands is listed as not understood, with its file and line, and never
  guessed: one with a variable, a list or a bracket argument, one within `if()` or another block, a path outside the
  project.
- The other `.json` files of the project that hold `"dgx-ui"` and `"tree"`: pages loaded while the program runs,
  their names file and appearance chosen in the properties; and those that hold `"dgx-ui"` and `"colors"`, the
  appearances such a page may be given.
- The fonts: `dgx_fonts-files.txt` in `build/dgx_fonts/` or `build/<board>/dgx_fonts/`, which DGX writes when the
  program's CMakeLists calls `dgx_fonts_files()` ([fonts of the project](preview/README.md#fonts-of-the-project)).
  Without it the fonts are stand-ins, and the page says so.
- The screen: of the board of the project's preset, as the program sees it at its start:
  `CONFIG_BOARD_DISPLAY_WIDTH`, `_HEIGHT` and `_COLOR_BITS` of the build's `sdkconfig`, or of the project's
  `sdkconfig.<BOARD>` where a preset of [jef-sure/boards](https://github.com/jef-sure/esp32-component-boards) keeps
  it; else `CONFIG_BOARD_HOST_*` there, as an older boards writes them; else the board's entry, `board.json`. With
  no board, one of the list (320x240 of the CYD, 480x272, 480x800, 1024x600, 800x1280), with its depth and its pixels
  a point.
- A project with jef-sure/boards (`managed_components/jef-sure__boards/`, `components/local-boards/`, or a `BOARD` in
  its preset) has boards' page as its main one, which holds this builder: the builder says so in a line above the
  project, with the command of the copy of boards it found.

## From a terminal

A server of the component, run in the project's folder, serves the builder and reads and writes the project for it:

```sh
python3 managed_components/jef-sure__dgx-ui/tools/dgx_ui_serve.py   # dgx-ui from the registry, as Getting started has it
python3 managed_components/dgx-ui/tools/dgx_ui_serve.py             # dgx-ui taken from git
```

It prints a link, `http://127.0.0.1:8741/editor/#project=...`. Opened in a browser of this computer, in VS Code by
"Simple Browser: Show", it shows the builder with the project open at once: no folder to choose, and a save written in
place. Ctrl-C stops the server. `--project DIR` serves another folder, `--port N` another port; when 8741 is taken it
takes a free one and says so, and there the zoom and the language the browser keeps for the page start anew.

- Only this computer reaches it: it listens on 127.0.0.1 alone, and the project's calls carry the token of the link,
  which a browser never sends in an address.
- It reads only what the builder reads: the project's CMakeLists and `CMakePresets.json`, the files its calls name,
  the pages and appearances there, the PNG files the lines of the file of pictures' needs name, the fonts and the
  pictures DGX writes under `build/`, and the board's entry; of a build's `sdkconfig`, and of the project's
  `sdkconfig.<BOARD>` of its preset's board, only the six lines of the screen, `CONFIG_BOARD_DISPLAY_*` and
  `CONFIG_BOARD_HOST_*` (an `sdkconfig` may hold a password). Nothing of a dot, no other `sdkconfig*` and no
  `dependencies.lock`: a file such as `main/secrets.h` is not read.
- It writes only what the project gives the builder: the files its `dgx_ui_tree()`, `dgx_ui_appearance()` and
  `dgx_ui_names()` calls name, read anew at each save, and pages and appearances already there; of `.json` and
  `.txt`, into folders that are there; and a PNG file of a picture the builder imports, beside the file of
  pictures' needs or below it, never over a PNG no need names
  ([A picture from a PNG file](#a-picture-from-a-png-file)). Never a file or folder whose name begins with a dot, nor `CMakeLists.txt`,
  `CMakeCache.txt`, `CMake*.json`, `requirements.txt`, `sdkconfig*` or `dependencies.lock`, in any case of letters;
  nothing under `build/`, `managed_components/` or another folder of a build; nothing outside the project, links
  followed. A file it refuses is not saved, and the builder says why.
- Every page and file it serves is under a Content-Security-Policy: a page calls this server alone, and cannot send
  what it reads to another host by a request, an image or a font. It is not kept out of a frame, as VS Code's browser
  shows it in one; a page that frames it has not the token of the link.
- A project with its own copy of dgx-ui, `managed_components/jef-sure__dgx-ui/` from the registry or
  `managed_components/dgx-ui/` from git, is given the builder's page of that copy, so the builder is of the version
  the project builds with. The copy is served to the browser, never run, and only when its folder lies inside
  `managed_components/`, no file of its `editor/` leads out by a link, its page speaks this server's protocol, and,
  with a `CHECKSUMS.json`, every file matches it. These checks keep out a damaged copy, not a forged one: what holds
  is the browser's sandbox, the policy, and this server's own rules of reading and writing. A copy not taken gets
  the server's own page, and the server says why.
- It runs one program, and only to make a picture the builder imports: the writer of pictures of the project's own
  DGX, `managed_components/jef-sure__dgx/tools/dgx_images.py`, when its bytes match the `CHECKSUMS.json` the component
  manager wrote beside it, and no path a request gives. It runs it by its own Python, isolated (`-I`), in a new empty
  folder, with no environment but `PATH`, for 30 seconds at most, keeping 4 KB of what it prints. A project whose DGX
  is not the registry's copy, taken by a path or from git, has no `CHECKSUMS.json`: the picture is not made, and the
  builder says so.
- It takes only the standard library of Python, so ESP-IDF's Python runs it.

## With an agent

`tools/dgx_ui_mcp.py` is the same server with an agent beside the builder, over MCP: the agent sees the page the
person edits and its verdicts, edits it in the builder, and talks with the person in the builder's dialog.

```sh
python3 managed_components/jef-sure__dgx-ui/tools/dgx_ui_mcp.py --install   # once: the server given to the agent
```

`--install` gives it to Claude Code for every folder, by `claude mcp add -s user dgx-ui`, when Claude Code's command
is found, on the PATH or in its extension of VS Code or Cursor; otherwise it puts it into `.mcp.json` of the folder,
beside the servers named there, and prints what another client's settings take. The agent then starts it in the
folder it works in; a folder with no call of dgx-ui and no description gets no tools. The path given is the path of
the script run: when that copy goes with its project, run `--install` again, or from a clone of dgx-ui.
`--serve` is the builder with no agent, as `dgx_ui_serve.py`.

In a project with [jef-sure/boards](https://github.com/jef-sure/esp32-component-boards) (its copy in
`managed_components/jef-sure__boards/`, `components/local-boards/`, or a `BOARD` in its preset), `--install` gives
nothing: boards' MCP server serves this builder too, on boards' page, and `--install` names its command.
`--install --standalone` gives dgx-ui's own all the same.

With the server comes a skill for the agent,
[`tools/skills/dgx-ui-builder/SKILL.md`](../tools/skills/dgx-ui-builder/SKILL.md): the order of work with the tools,
what the agent must not do (write a project's file on the disk, build, flash, run `--install`), where each format is
told, worked cases and what each refusal of the loader means. When it gives
the server to Claude Code, `--install` copies the skill into `skills/dgx-ui-builder/` of Claude Code's folder,
`~/.claude` or `$CLAUDE_CONFIG_DIR`, with a line naming the version of dgx-ui it came with. A copy, not a link: a
project's copy of dgx-ui may be made again or moved, and the skill stays. `--install` run again writes it afresh,
and leaves a skill of that name of another's as it is. In `.mcp.json` no skill is written, and the path of the
package's is printed for a client that reads skills. The server tells the agent the version it runs and where its
documentation lies (`formats`), so the agent can tell a copy of the skill older than the server.

![The builder's dialog under the properties: the person asks for the title "Volume", the agent answers that it is
done, and the panel shows the title so](../docs/images/builder-agent.png)

| Tool | What it does |
| --- | --- |
| `page_link` | the builder's link, for the person to open |
| `page_state` | the project's pages, names files and appearances, the page open and the selection, the loader's verdicts and where they stop, the stand-ins, the names a page misses, the files edited and not saved |
| `page_image` | the panel as the board draws it, a PNG |
| `read_file` | a description, names file or appearance as the builder holds it, its edits too |
| `edit_file` | the whole text of one of them put in, with why: the verdicts come back |
| `select` | a page opened and a widget selected, for the person to see |
| `formats` | what the loader takes: the keys of a description, an appearance and a names file |
| `wait_for_person`, `say` | the builder's dialog: what the person writes, and what the agent tells |

- The agent edits in the builder, never on the disk. An edit is a step of the file's history, which the person
  undoes with Ctrl+Z; its why stands in the dialog. Saving is the person's.
- The page must be open: the agent gives the person `page_link`, and with no page its tools say so at once.
- The project's texts reach the agent as data: its strings, names and comments as values of JSON, and its tools
  tell it they are no instructions.

## The page

| Part | What it does |
| --- | --- |
| Pages, widgets | the project's pages; the widgets of the one open, as a tree in the order they are added. Under them: add a widget of a kind, delete, move up and down, wrap in a box. |
| The panel | the page drawn by the preview, zoomed 1 to 4 times. "Select": a click selects the widget under it, outlined over the panel. "Try": the finger and the six keys go to the page, as on the board, and the events are listed. |
| Properties | the keys of the selected widget, as the format has them for its kind: words as boxes to tick, numbers and pairs in their ranges, names to pick of the names file's tables, or a new one. An empty number removes its key. |
| Appearance | the appearance of the page open, by forms: [its colours, styles and looks](#the-appearance), drawn on the panel as they are set. |
| Names | the names file of the page open, by forms: [its lines](#the-names-file), and the names the page uses that the file has not. |
| Texts | the page, its names file and its appearance as text, edited by hand; the loader's verdict in words, and the cursor put at the byte it stops at. |
| Dialog | when the builder is served [with an agent](#with-an-agent): what the agent says and does, and what the person writes to it. |

The keys: Ctrl+Z, and Ctrl+Y or Ctrl+Shift+Z, undo and redo, in the file of the part in use: the appearance in
Appearance, the names file in Names, the text shown in Texts, the page elsewhere; every file keeps a history of its own. Ctrl+S saves; Delete
deletes the selected widget; the arrows walk the tree. A refused page is greyed on the panel, and the widget holding the byte the loader stopped at is
marked in the tree.

While a button's action has opened another page on the panel, clicks select nothing: a bar names the page, with
"Open it", the builder on its description, and "Back".

![Texts: the page as text, with a kind misspelled, "slidr", and the loader's verdict above it: the kind is unknown,
at byte 250](../docs/images/builder-texts.png)

## The appearance

The forms show what the format of an appearance has, and the panel draws every change at once; the loader's verdict
stands at the field it names.

![Appearance: the colours with their swatches and hex, the styles, and the look "slider" open, its draw and the
colours of its track, fill and knob](../docs/images/builder-appearance.png)

- **Colours**: a name, two swatches, the colour as written and as the panel shows it at the depth of its screen, and
  the hex. The hex is typed, picked in the browser's picker, or given as the name of one of DGX's colours, and is
  written as `#rrggbb` in any case; when it is one of DGX's colours, its name stands beside it. Paper, ink, fill and
  border are the format's, not renamed and not deleted; a colour added comes last.
- **Styles**: the name, the fonts, the best first, each a font of the project or a variant of its own (a handwritten
  font, or a font's pixels as points), the alignment, the flags and the scroll.
- **Looks**: the name, the draw, with the kinds it draws, the fill, and the parameters of that draw by their types:
  a colour, a colour or what lies under, a number in its range, a word, a box to tick, a font, a hand, bands. A look
  added starts from the library's look of its draw; a draw changed keeps the parameters it shares with the old one.
  A look named by a kind's word, `"slider"`, is the look of every slider that names none.
- What the forms do not take stays as text and is marked so: a key twice, a key unknown, the parameters of a draw
  of the program's own.

The forms never move an entry: a new one comes last, a renamed one stays in its place.

**A name renamed** is carried wherever it is named. A colour is renamed in the appearance's looks; the program's C
may name it too, as `<prefix>_color_<name>` of the made header or by `dgx_ui_appearance_color()`, and the builder
says so. A look or a style is renamed in the appearance and in every page drawn on it, as one step: the builder
lists the pages and the widgets first, and Ctrl+Z in any of those files undoes it in all of them, unless one was
edited after it. It is not renamed to a name the appearance has, nor while a page on the appearance does not read.
A rename or a delete that gives a look a kind's word or takes it away, or that touches the first style, the style of
a text that names none, comes with a warning.

![The look "pane" renamed to "panel": the builder lists the page and the widget it changes, and waits for Rename or
Cancel](../docs/images/builder-rename.png)

**A name deleted**: a look or a style a widget names is not deleted, and the builder lists the widgets; a colour a
look names is refused by the loader, at that look.

## The names file

The Names part shows the names file a row a line, by its tables: the name, the C expression, and the word its table
may end with, a count of pictures, `number` of a handler, `page` of a context, `handwritten` of a font, the function
and description of a draw. A look or an action of the library's goes with no expression: the actions `back`, `page`
and `command`, whose `ctx` is a number, the command a button sends. The C is the program's: the
builder writes it as given, and `dgx_ui_c` checks it when the program is built. An expression with a space or a `#`
waits: it would split the line.

![Names: the value "volume" the page names and the file has not, with its C "&volume" and "Add"; under it the file's
tables, a look no page uses marked so](../docs/images/builder-names.png)

- **The names the page misses** are listed, each with the C `dgx_ui_c`'s rule gives it, `&volume` for a value
  `volume`. "Add" puts one into the file, "Add all" all of them, one step: a names file is begun so. A file a call
  names that is not on the disk yet opens empty, and is written at the save.
- **The lines stay as written.** A line not edited is written as it was, its spaces and comment too. An edited line
  keeps each word at its column when the word before it leaves room, and its comment; a new line goes after the
  last of its table, at its columns. Lines are never moved. A line the reader cannot place is edited as text.
- A name no page on the file uses is marked so, not deleted.

**A name renamed** is carried, as one step undone together, into every page on the names file and every appearance
read on it: the widgets that name it by its table's key, a look's draw, a variant's font. The C stays as it was, and
only the name the pages and appearances use changes; a name of the library's renamed is given the C `dgx_ui_c` gives
it, `&dgx_ui_look_slider` for `slider`. A hand is the library's word and is not renamed. A name is not renamed to one
its table has, nor while a file on the names file does not read, and a name a page or an appearance uses is not
deleted: the builder lists them.

## A picture from a PNG file

Through the server of a terminal alone ([From a terminal](#from-a-terminal)), the builder takes a PNG file as a
picture of the project: DGX makes it a file of pictures when the program is built, the page names it, and the board
draws it from flash ([A picture from a PNG file](../docs/en/cookbook.md#a-picture-from-a-png-file)). A page opened
from the disk does not offer it and says why: the picture is made by DGX's writer, a program of the computer. The
project names a file of pictures' needs first, a line of its CMakeLists:

```cmake
dgx_image_needs_file(images.txt)   # main/CMakeLists.txt, after idf_component_register()
```

![The import of a PNG of a sun: on the panel the page with a logo DGX made at the last build and the sun drawn before it
is taken; on the right, in the Names tab, "sun.png: 3 colours, with holes", its name, 4 bits a pixel, the flash and RAM
it takes, "Take it" and "Cancel"](../docs/images/builder-import.png)

- **A PNG chosen or dropped**, or several, the pictures of an icon. The builder counts their colours, and whether
  some pixels are transparent, and offers the depth DGX would take them at: 1 bit for two colours (one, with a
  transparent background), 4 for 16 (15), 8 for 256 (255), 16 for more. The depth may be changed; `REDUCE` and
  `DITHER` take a picture of more colours to fewer, and `INK` says which colours of one bit are ink.
- **Soft edges**, pixels partly transparent, are seen too, and kept by default ("soft edges, as the PNG has them"). A
  PNG of one colour with soft edges is offered as a mask of 4 bits, `MASK` in its need, drawn in the ink of the look
  `soft_picture`; 8 bits give smoother edges at twice the bytes. One of several colours is offered as 16 bits that
  keep the opacity. With the box unticked, a picture of 16 bits takes `BACKGROUND`, the appearance's paper by
  default ([An icon with soft edges](../docs/en/cookbook.md#an-icon-with-soft-edges)).
- **Made at once.** The server makes the file by the project's DGX, and the preview draws it in the appearance's
  colours, as the board will. What DGX refuses, a picture of 37 colours at 4 bits, is said in DGX's words.
- **Taken, written at the save.** The PNG files into `images/` beside the file of needs, `images/<name>.png`; the
  need at the end of that file, `<name> PNG images/<name>.png BITS <n>`, with `MASK` for a mask; and the line of the
  page's names file, `picture <name> @<name>`. Never a CMakeLists. The name is a name of C, not of `dgx_`, and not a
  need's the file has.
- **The look.** A mask and a picture of 16 bits with soft edges are drawn by `soft_picture` alone. When widgets of the
  page show the picture by another look, the builder says how many and offers "Draw them by soft_picture": it sets
  their look and adds `look soft_picture` to the names file, as one step.
- **After a build** the panel draws every picture DGX made by the project's needs, from `build/dgx_images/`, or
  `build/<board>/dgx_images/` of a preset, as it draws the fonts DGX made.
- Refused in words: a project with no file of needs, a page with no names file, a name taken, and a project whose DGX
  is not the registry's copy.

## Saving

Only the files edited are written; the rest are not touched. A description is written in one form: its keys in one
order, `"children"` last, two spaces of indent, a widget with no children on one line when it goes in 100 columns.
What it holds stays as written: a string or a number keeps its text, and a key unknown, or a key twice, keeps its
place, so the loader goes on saying of it what it says. A description written by hand is put into that form at its
first save, after a word.

An appearance is written in one form too: `"dgx-ui"`, `"name"`, `"editor"`, `"colors"`, `"styles"` and `"looks"` in
that order, an entry a line, broken where it would pass 100 columns. Before an appearance is written, it and every
page drawn on it are read again; those the loader refuses are listed, and the save waits for a word. A names file
is not put into a form: only the lines edited change, and before it is written the pages on it are read again, as
for an appearance.

The builder keeps of its own only the screen the page is made for, in the description's `"editor"` under
`"dgx-ui-builder"`, beside what other tools keep there. The zoom and the language are the browser's, in its local
storage.

## In another page

A page that hosts the builder loads `builder.css` and the scripts of `index.html`, in its order, but `start.js`, and
mounts it on an element of its own:

```html
<div id="builder"></div>
<script>
    dgxUiBuilderMount('#builder', {host: dgxUiBrowserHost, start: {language: 'ru'}});
</script>
```

| Option | What it is |
| ------ | ---------- |
| `host` | the files' host: `dgxUiBrowserHost`, whose folder the person opens; or a project's host already opened, as `await dgxUiServerHost(token, prefix)` gives it, opened at once |
| `open` | a function that gives a project's host, opened at once |
| `start` | what the builder starts with, `{language: 'ru'}` |
| `agent` | `true`: the agent is started once the project is open, by the host's `agent(store)` (the host of `dgxUiServerHost()` has it); `false`: none, and nothing asks the server's `/agent/`; not given: none started |
| `dialog` | `false`: the builder's own dialog is not shown, for a page that talks with the person in its own |
| `boardsLine` | `false`: the line that points to the page of jef-sure/boards is not shown, for that page itself |
| `languageSelector` | `false`: the builder's choice of language is not shown, and `start.language` is taken over a remembered one, for a page that has its own choice |

What `dgxUiBuilderMount()` resolves to has `setLanguage(code)`, `'en'` or `'ru'`, for the hosting page to call when
its own language changes.

The builder's scripts leave nothing global but names that begin with `dgxUi`, `DgxUi` and `DGX_UI_`. The Vue and
Pinia of `vendor/` may be the page's own, of the same versions.

### Under a server of Python

A server of this computer written in Python mounts the builder under a prefix of its own paths, as the page of
jef-sure/boards does (its `tools/boards_mcp.py --serve`), by `mounted()` of the project's copy of
`tools/dgx_ui_serve.py`:

```python
import http.server, importlib.util

spec = importlib.util.spec_from_file_location("dgx_ui_serve", "managed_components/jef-sure__dgx-ui/tools/dgx_ui_serve.py")
serve = importlib.util.module_from_spec(spec)
spec.loader.exec_module(serve)
handle = serve.mounted(project, token, port, "/dgx-ui")

class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if handle(self):
            return
        ...  # the server's own paths

    do_POST = do_GET
```

- `handle(request)` answers `/dgx-ui/editor/...`, the builder's files, and `/dgx-ui/project...`, the project's
  calls, by the rules of [From a terminal](#from-a-terminal), and returns `True`. For any other path it sends
  nothing and returns `False`.
- A request's Host must be `127.0.0.1:<port>`, and a write's Origin `http://127.0.0.1:<port>`: the server listens on
  127.0.0.1 and its link names it, not `localhost`. The project's calls carry the token the hosting page holds, given
  to `dgxUiServerHost(token, '/dgx-ui')`.
- The page loads the builder from `/dgx-ui/editor/`. A Content-Security-Policy of its own, if it has one, allows what
  the builder needs: `script-src 'self' 'unsafe-eval' 'wasm-unsafe-eval'` (Vue compiles the templates in the browser,
  and the preview is WebAssembly), `connect-src 'self'`, `img-src 'self' data: blob:`, `style-src 'self'
  'unsafe-inline'`, `font-src 'self' data:`.
- The agent of the builder is the hosting server's, by `mounted_agent()` of the project's copy of
  `tools/dgx_ui_mcp.py`. That file loads the `dgx_ui_serve.py` beside it as its `serve`, so one load gives both from
  one copy. It is loaded by the name `dgx_ui_serve`, so a process holds one copy of dgx-ui: a second one loaded in
  it would get the first's.

```python
spec = importlib.util.spec_from_file_location("dgx_ui_mcp", "managed_components/jef-sure__dgx-ui/tools/dgx_ui_mcp.py")
agent = importlib.util.module_from_spec(spec)
spec.loader.exec_module(agent)
handle = agent.serve.mounted(project, token, port, "/dgx-ui")
tools, call, handle_agent = agent.mounted_agent(project, token, port, "/dgx-ui", "ui_", said=show_in_dialog)
```

- `handle_agent(request)` answers the page's `/dgx-ui/agent/...` by the same rules, `True`, else `False`: a handler
  asks it and `handle()` before its own paths, on a threaded server (`http.server.ThreadingHTTPServer`), as the page
  waits for the agent's requests up to 25 s.
- `tools` are the builder's tools named with the prefix, `ui_page_state`, `ui_page_image`, `ui_read_file`,
  `ui_edit_file`, `ui_select` and `ui_formats`, for the hosting MCP server's list. `page_link`, `wait_for_person` and
  `say` are not among them: the page and its dialog are the host's.
- `call(name, arguments)` runs one through the builder's page and returns the result of `tools/call` of MCP. With no
  builder open, it says so.
- `said(text)` is the host's: it shows the person the why of each edit, in the dialog of its page. It is called in
  the thread of `call()`, so it is safe from any thread and returns soon; one that raises does not stop the edit,
  and the result says the why was not shown. With no `said`, the why is shown nowhere but where the agent says it.
- `skill_part("ui_", hosted=True)` of the same file gives the builder's skill for the hosting server's own: its tools
  named so, its headings a level down, the page and the dialog the host's.
- The page mounts the builder with `agent: true, dialog: false, boardsLine: false, languageSelector: false`.

A host gives the builder a project's files by paths relative to its directory: `list(directory)`, `read(path)` the
bytes of a file or `null`, `write(path, text)`. `host_browser.js` has two: a directory of the File System Access API,
and files held in memory; `host_server.js` has the third, the server of a terminal.

## Files

| File | What it is |
| --- | --- |
| `index.html` | the builder's own page |
| `app.js`, `builder.css` | the page: its parts and their look, all under the class `dgx-ui-builder` |
| `store.js` | the state and what is done to it: the project, the page open, the selection, the history |
| `description.js`, `history.js` | a description read, changed and written back; undo and redo |
| `appearance.js` | an appearance changed and written back; a colour by its hex or DGX's name, and as a depth shows it |
| `names.js` | a names file as its lines: one changed, added or removed, the others kept as they are |
| `host_browser.js` | the files of a project in a browser |
| `host_server.js` | the files of a project through the server of a terminal, `tools/dgx_ui_serve.py` |
| `agent.js` | the agent's requests taken from that server and answered by the page, and the dialog |
| `start.js` | the builder mounted on `index.html`, and the agent's loop when the link asks for it |
| `lang.js`, `lang/ru.js` | the words of the page, English and Russian |
| `vendor/` | Vue and Pinia, the same files as the editor of [boards](https://github.com/jef-sure/esp32-component-boards) ships, with their licences |
| `preview/` | the preview the panel draws with |

What it takes: 896 KB on the disk, 318 KB when a server sends it with gzip, of which the preview is 480 KB and 193 KB.
