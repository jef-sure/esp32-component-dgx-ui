# The builder of pages

`index.html` here is a builder of dgx-ui's pages: it opens a project's directory, draws its descriptions as the board
draws them, lets their widgets be selected, set and moved, and saves the files back. It is a page opened from the
disk, with no server and nothing to build; nothing leaves the page.

The drawing is [the preview](preview/README.md), dgx-ui itself built to WebAssembly, so what the panel shows is what
the board shows, and every check of a page is the loader's: the builder repeats no rule of the format.

## Opening a project

Open `index.html` in a browser and press "Open a project": in Chrome and Edge, the File System Access API gives the
page the directory, read and written in place. Other browsers take the project's files by "or choose its files", and a
save comes as a download.

The builder reads the project's own declarations:

- `main/CMakeLists.txt` and `components/*/CMakeLists.txt`: every `dgx_ui_tree()` is a page with its `NAMES` and
  `APPEARANCE`, every `dgx_ui_names()` a names file, every `dgx_ui_appearance()` an appearance with its own `NAMES`,
  read as `dgx_ui_c` reads them ([project_include.cmake](../docs/en/api.md#project_includecmake)).
  A call the builder cannot read as it stands is listed as not understood, with its file and line, and never
  guessed: one with a variable, a list or a bracket argument, one within `if()` or another block, a path outside the
  project.
- The other `.json` files of the project that hold `"dgx-ui"` and `"tree"`: pages loaded while the program runs,
  their names file and appearance chosen in the properties.
- The fonts: `dgx_fonts-files.txt` in `build/dgx_fonts/` or `build/<board>/dgx_fonts/`, which DGX writes when the
  program's CMakeLists calls `dgx_fonts_files()` ([fonts of the project](preview/README.md#fonts-of-the-project)).
  Without it the fonts are stand-ins, and the page says so.
- The screen: of the board of the project's preset when the build says it, else one of the list (320x240 of the CYD,
  480x272, 480x800, 1024x600, 800x1280), with its depth and its pixels a point.

## The page

| Part | What it does |
| --- | --- |
| Pages, widgets | the project's pages; the widgets of the one open, as a tree in the order they are added. Under them: add a widget of a kind, delete, move up and down, wrap in a box. |
| The panel | the page drawn by the preview, zoomed 1 to 4 times. "Select": a click selects the widget under it, outlined over the panel. "Try": the finger and the six keys go to the page, as on the board, and the events are listed. |
| Properties | the keys of the selected widget, as the format has them for its kind: words as boxes to tick, numbers and pairs in their ranges, names to pick of the names file's tables, or a new one. An empty number removes its key. |
| Texts | the page, its names file and its appearance as text, edited by hand; the loader's verdict in words, and the cursor put at the byte it stops at. |

The keys: Ctrl+Z, and Ctrl+Y or Ctrl+Shift+Z, undo and redo; Ctrl+S saves; Delete deletes the selected widget; the
arrows walk the tree. A refused page is greyed on the panel, and the widget holding the byte the loader stopped at is
marked in the tree.

While a button's action has opened another page on the panel, clicks select nothing: a bar names the page, with
"Open it", the builder on its description, and "Back".

## Saving

Only the files edited are written; the rest are not touched. A description is written in one form: its keys in one
order, `"children"` last, two spaces of indent, a widget with no children on one line when it goes in 100 columns.
What it holds stays as written: a string or a number keeps its text, and a key unknown, or a key twice, keeps its
place, so the loader goes on saying of it what it says. A description written by hand is put into that form at its
first save, after a word.

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
and files held in memory.

## Files

| File | What it is |
| --- | --- |
| `index.html` | the builder's own page |
| `app.js`, `builder.css` | the page: its parts and their look, all under the class `dgx-ui-builder` |
| `store.js` | the state and what is done to it: the project, the page open, the selection, the history |
| `description.js`, `history.js` | a description read, changed and written back; undo and redo |
| `host_browser.js` | the files of a project in a browser |
| `lang.js`, `lang/ru.js` | the words of the page, English and Russian |
| `vendor/` | Vue and Pinia, the same files as the editor of [boards](https://github.com/jef-sure/esp32-component-boards) ships, with their licences |
| `preview/` | the preview the panel draws with |

What it takes: 768 KB on the disk, 285 KB when a server sends it with gzip, of which the preview is 467 KB and 188 KB.
