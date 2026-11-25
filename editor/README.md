# The builder of pages

`index.html` here is a builder of dgx-ui's pages: it opens a project's directory, draws its descriptions as the board
draws them, lets their widgets be selected, set and moved, and saves the files back. It is a page opened from the
disk, with no server and nothing to build; nothing leaves the page.

The drawing is [the preview](preview/README.md), dgx-ui itself built to WebAssembly, so what the panel shows is what
the board shows, and every check of a page is the loader's: the builder repeats no rule of the format.

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
- The screen: of the board of the project's preset when the build says it, else one of the list (320x240 of the CYD,
  480x272, 480x800, 1024x600, 800x1280), with its depth and its pixels a point.

## From a terminal

A server of the component, run in the project's folder, serves the builder and reads and writes the project for it:

```sh
python3 managed_components/jef-sure__dgx-ui/tools/dgx_ui_serve.py   # dgx-ui from the registry
python3 managed_components/dgx-ui/tools/dgx_ui_serve.py             # dgx-ui taken from git, as Getting started does
```

It prints a link, `http://127.0.0.1:8741/editor/#project=...`. Opened in a browser of this computer, in VS Code by
"Simple Browser: Show", it shows the builder with the project open at once: no folder to choose, and a save written in
place. Ctrl-C stops the server. `--project DIR` serves another folder, `--port N` another port; when 8741 is taken it
takes a free one and says so, and there the zoom and the language the browser keeps for the page start anew.

- Only this computer reaches it: it listens on 127.0.0.1 alone, and the project's calls carry the token of the link,
  which a browser never sends in an address.
- It writes only what the project gives the builder: the files its `dgx_ui_tree()`, `dgx_ui_appearance()` and
  `dgx_ui_names()` calls name, read anew at each save, and pages and appearances already there; of `.json` and
  `.txt`, into folders that are there. Never a file or folder whose name begins with a dot, nor `CMakeLists.txt`,
  `CMakeCache.txt`, `CMake*.json`, `requirements.txt`, `sdkconfig*` or `dependencies.lock`, in any case of letters;
  nothing under `build/`, `managed_components/` or another folder of a build; nothing outside the project, links
  followed. A file it refuses is not saved, and the builder says why.
- A project with dgx-ui from the registry is served by its own copy, `managed_components/jef-sure__dgx-ui/`, whatever
  copy of the script was run: the builder is of the version the project builds with.
- It takes only the standard library of Python, so ESP-IDF's Python runs it.

## The page

| Part | What it does |
| --- | --- |
| Pages, widgets | the project's pages; the widgets of the one open, as a tree in the order they are added. Under them: add a widget of a kind, delete, move up and down, wrap in a box. |
| The panel | the page drawn by the preview, zoomed 1 to 4 times. "Select": a click selects the widget under it, outlined over the panel. "Try": the finger and the six keys go to the page, as on the board, and the events are listed. |
| Properties | the keys of the selected widget, as the format has them for its kind: words as boxes to tick, numbers and pairs in their ranges, names to pick of the names file's tables, or a new one. An empty number removes its key. |
| Appearance | the appearance of the page open, by forms: [its colours, styles and looks](#the-appearance), drawn on the panel as they are set. |
| Texts | the page, its names file and its appearance as text, edited by hand; the loader's verdict in words, and the cursor put at the byte it stops at. |

The keys: Ctrl+Z, and Ctrl+Y or Ctrl+Shift+Z, undo and redo, in the file of the part in use: the appearance in
Appearance, the text shown in Texts, the page elsewhere; every file keeps a history of its own. Ctrl+S saves; Delete
deletes the selected widget; the arrows walk the tree. A refused page is greyed on the panel, and the widget holding the byte the loader stopped at is
marked in the tree.

While a button's action has opened another page on the panel, clicks select nothing: a bar names the page, with
"Open it", the builder on its description, and "Back".

## The appearance

The forms show what the format of an appearance has, and the panel draws every change at once; the loader's verdict
stands at the field it names.

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

**A name deleted**: a look or a style a widget names is not deleted, and the builder lists the widgets; a colour a
look names is refused by the loader, at that look.

## Saving

Only the files edited are written; the rest are not touched. A description is written in one form: its keys in one
order, `"children"` last, two spaces of indent, a widget with no children on one line when it goes in 100 columns.
What it holds stays as written: a string or a number keeps its text, and a key unknown, or a key twice, keeps its
place, so the loader goes on saying of it what it says. A description written by hand is put into that form at its
first save, after a word.

An appearance is written in one form too: `"dgx-ui"`, `"name"`, `"editor"`, `"colors"`, `"styles"` and `"looks"` in
that order, an entry a line, broken where it would pass 100 columns. Before an appearance is written, it and every
page drawn on it are read again; those the loader refuses are listed, and the save waits for a word.

The builder keeps of its own only the screen the page is made for, in the description's `"editor"` under
`"dgx-ui-builder"`, beside what other tools keep there. The zoom and the language are the browser's, in its local
storage.

## In another page

A page that hosts the builder loads the same scripts as `index.html` and mounts it on an element of its own:

```html
<div id="builder"></div>
<script>
    dgxUiBuilderMount('#builder', {host: dgxUiBrowserHost, start: {language: 'ru'}});
</script>
```

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
| `host_browser.js` | the files of a project in a browser |
| `host_server.js` | the files of a project through the server of a terminal, `tools/dgx_ui_serve.py` |
| `lang.js`, `lang/ru.js` | the words of the page, English and Russian |
| `vendor/` | Vue and Pinia, the same files as the editor of [boards](https://github.com/jef-sure/esp32-component-boards) ships, with their licences |
| `preview/` | the preview the panel draws with |

What it takes: 839 KB on the disk, 304 KB when a server sends it with gzip, of which the preview is 473 KB and 191 KB.
