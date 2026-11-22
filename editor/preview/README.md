# The preview: a description drawn in a browser as the board draws it

`dgx_ui_preview.js` is dgx-ui itself, its loader and its painter with DGX's drawing under them, built to WebAssembly:
one file a page loads by a `<script>`, from a disk as well as from a server. It takes a page's description, an
appearance and a names file as texts, and the project's fonts as the files DGX writes of them. It gives back the
pixels of a panel of the board's size, depth and unit, the loader's verdict on each text, and the widgets as the tree
has them after its layout. The page drives it: it gives
the time, the finger and the keys, and nothing runs between its calls.

It is made for a page that edits descriptions. The loader that refuses a text here is the one that runs on the board,
and it stops at the same byte with the same code and name.

## Trying it

Open `index.html` of this directory in a browser, from the disk. It holds a page, an appearance, a names file and the
pages a button opens as texts to change, and a panel drawn anew at each change. "Font files" takes the directory
where a project's build laid its fonts, `build/dgx_fonts/` (see [Fonts of the project](#fonts-of-the-project)). The
mouse is the finger; under the panel are the six keys and a value to write; beside it the widget under the finger,
the events and the stand-ins.

## In a page

```html
<script src="dgx_ui_preview.js"></script>
<script>
(async () => {
    const preview = await DgxUiPreview.create({pageText: name => pages[name]});
    preview.screen(320, 240, 16, 1);            // the board's panel: width, height, depth, pixels a point
    preview.names(null);                        // no names file: every name the page uses
    await preview.fonts(index, read);           // the project's fonts, as DGX writes them: [{name, verdict}]
    preview.appearance(appearanceText);         // or null: no appearance
    const verdict = preview.page(text);         // {code, offset, name, said}: code 0, taken
    if (preview.paint(performance.now())) context.putImageData(preview.image(), 0, 0);
})();
</script>
```

In Node the same file is `const DgxUiPreview = require('./dgx_ui_preview.js')`.

A preview is a copy of the module with memory of its own: two of them on a page do not touch each other.

### The calls

| Call | What it does |
| --- | --- |
| `DgxUiPreview.create({pageText, log})` | A preview, by a promise. `pageText(name)` gives the text of the page a button's action `"page"` opens by its `"ctx"`, or `null`. `log(line)` takes the lines the library logs; `console.log` when none is given. |
| `version()` | What the module is built of: `{dgx_ui, dgx, toolchain, sources}`. |
| `screen(width, height, depth, pixelsAPoint)` | The panel: 1 to 4096 each way, the depth 16, 18 or 24, and the pixels a point of the board's program, 1 to 16, what it gives `dgx_ui_init()`. 0, or -2: refused, and then there is no panel, nothing is drawn and `frame()` is `null` until a call is taken. |
| `names(text)` | The program's names file, or `null`: then every name a text uses is taken, as `dgx_ui_c` takes them with no names file. A verdict; a file refused counts as none. |
| `font(cName, bytes)` | The file of a font, a `.dgxf` of DGX, for the name of C the font is taken by: `null` lets that name's file go. The bytes are a `Uint8Array`, an `ArrayBuffer` or a view of one, and are copied. A verdict. |
| `fonts(index, read)` | Every font of `dgx_fonts-files.txt`, the index DGX writes beside a project's fonts. `read(file)` gives the bytes of a file of the index's directory, or a promise of them. A promise of `[{name, verdict}]`, a line of the index each. |
| `appearance(text, names)` | The appearance, or `null` for none, read on the names as `dgx_ui_c -a` reads it: with `names` left out on the page's names file; with `names` a text, on that file of its own, as a `dgx_ui_appearance()` with `NAMES`; with `null`, by the rule, as one without. A verdict, the own file's refusal first. |
| `page(text)` | The page, or `null` for none. A verdict. |
| `paint(nowMs)` | The time given and the page drawn where it changed: `true` when pixels were written. |
| `wake(nowMs)` | The milliseconds after which a widget asks to be drawn again; `Infinity` when none does. |
| `frame()` | The panel's pixels, red, green, blue and 255 each, row after row: a view of the module's memory, to be read before the next call. `null` with no panel. |
| `image()` | The frame as an `ImageData`, a copy. |
| `changed()` | The rectangle written since the last call, `{x, y, w, h}`; `w` is 0 when nothing was. |
| `pointer(x, y, down, nowMs)` | The finger, as a touch panel reports it. |
| `key(key)` | A key of a dialog: 0 next, 1 previous, 2 activate, 3 back, 4 more, 5 less, the order of `DGX_UI_KEY_*`. |
| `buttons(held, nowMs)` | The buttons held, as `dgx_ui_buttons()` takes them. |
| `value(name, value)` | A value of the names written, as the program writes it; `false` when no page has a value of that name. |
| `shown()` | The page on the screen: `""` for the page given to `page()`, the name of a page a button opened over it, `null` with no page. The three calls below speak of that page. |
| `count()`, `widget(i)` | Its widgets: `{kind, parent, x, y, w, h, shown, bits, value, text}`, the kind and the bits as `dgx_ui.h` numbers them, `shown` false for a widget hidden, under a hidden one, or cut away whole by a parent. |
| `widgetAt(x, y)` | The widget a finger there would take, -1 for none. |
| `events()` | What happened since the last call, a line each. |
| `notes()` | The stand-ins the pages in use take, a line each. |
| `format()` | The format of a page as the loader reads it: `{version, kinds, keys}`, each key with its type, the kinds it means something for, its words or its range, the table of its names. A page that edits descriptions draws its controls by it. |
| `tables()` | The names of the tables the page given is read on, `{look, value, picture, style, on, take, ctx, id}`, the appearance's looks and styles first. |
| `json(text)` | Whether a text, or bytes, is JSON as the loader's reader reads it: 1, 0, or 2 for one deeper than it goes. |

A verdict is `{code, offset, name, said}`. For a text taken `code` is 0. For a page or an appearance refused it is the
loader's `DGX_UI_LOAD_*`, with the byte and the name it stopped at: the errors of
[`dgx_ui_load.h`](../../docs/en/api.md#dgx_ui_loadh). For a names file refused it is -1, with the reason in `said`
in the words `dgx_ui_c` refuses it with, `names:2: the look "slider" twice`. For a font file refused it is -3, with
the reason in `said`.

A page is read on the names, the fonts and the appearance, and an appearance on the names and the fonts: when one of
them is given, the others are read again, so the picture does not depend on the order of the calls. A verdict is of
its call: after another names file, font or appearance, give the page again to have its verdict on them. Giving them
in the order names, fonts, appearance, page has every verdict right at once.

A fault inside the module stops that preview: the call throws an `Error` whose message begins `the preview stopped`,
and so does every call after it; `create()` makes a new one. A `pageText` that throws does not stop it: the page is
not opened, and the error goes to `log`.

### The events

| Line | What happened |
| --- | --- |
| `told <widget> <what>` | A handler of the page given was told: `<what>` is `ACTIVATED`, `CHANGED`, `FOCUS`, `BACK`, `NO_MEMORY`, `EDIT`, `SHOWN` or `COVERED`. |
| `told <widget> <what> page <name>` | The same on the page `<name>` a button opened. |
| `unopened <name> no-text` | `pageText` had no text for the page `<name>`. |
| `unopened <name> refused <code> <byte> "<name>"` | The loader refused its text; the quoted name runs to the line's end. |
| `unopened <name> no-room 8` | Eight pages are loaded and kept already. |
| `lost <count>` | The last line: so many lines more found no room. About 4 KB of lines are kept until they are read. |

### What stands in

What is C of the program is not in a description, so the preview draws a stand-in for it, and `notes()` says each one
in use:

| The program's | What stands in |
| --- | --- |
| a look or a draw of its own | a frame with the look's name |
| a picture | a crossed square of 16 pixels |
| a style of its own | Terminus 12: the measures of the text, and with them the layout, may differ from the board's |
| a font of an appearance with no file given, not one of the five built in | Terminus 12, or font0ss for one the names file calls handwritten; the measures may differ as above |
| a handler | nothing is done; what it was told is a line of the events |
| a take | takes nothing |
| a value | a place of its own, 0 until `value()` writes it |

The actions `back` and `page` are the library's own and work: a page is found by its name through `pageText`, loaded
once and kept, eight of them at most, until the page, the names or the appearance is given again.

The fonts built in are `TerminusTTFMedium12`, `DejaVuSansBook32`, `LiberationSansRegular12`, `DroidSansRegular9` and
the handwritten `font0ss`, as DGX ships them. A page with no font files still draws with them.

### Fonts of the project

A board draws with the fonts its build makes: DGX's own, the fonts made for the project's needs, its fonts of C. With
`dgx_fonts_files()` in the program's CMakeLists, DGX also writes each of them as a file, `build/dgx_fonts/<name>.dgxf`,
and beside them `dgx_fonts-files.txt`, a line `<name in C> <file>` for every name a font is taken by. `fonts()` gives
the preview all of them; the preview then draws the text in the very font of the board.

```js
const results = await preview.fonts(indexText, file => directory.getFileHandle(file).then(handle => handle.getFile())
                                                                .then(found => found.arrayBuffer()));
```

- A font is found by its name in C: the names file's `font small TerminusTTFMedium12` takes `TerminusTTFMedium12`,
  and with no names file the rule takes a font `small` as `dgx_font_small`, as the index names it.
- A file wins over a font built in of the same name: the project may make `TerminusTTFMedium12` of its own needs.
- Files live until a file is given again for the name, or `font(name, null)` lets it go; changing the names or the
  appearance keeps them. A file refused leaves the one given before for that name.
- A file knows its kind, and the loader follows it, as on the board: a handwritten font in a variant with no hand is
  refused `DGX_UI_LOAD_MISSING` `"hand"`. Where the names file says otherwise, the notes say so: `dgx_ui_c`, making
  C of the appearance, follows the names file, and its C would draw otherwise than the board that loads it.
- Refused, with the words in `said`: a name of C longer than 63 bytes; a file of a later format of DGX, of the other
  byte order, or not a font, spoiled or cut. `fonts()` refuses in its result, without throwing, a line that is not a
  name and a file, a name given twice, a file outside the index's directory, and a read that fails or gives no bytes.

### What it takes

The file is 464 KB, 187 KB when a server sends it with gzip. A preview takes 0.4 MB and its frame, four bytes a
pixel: 0.7 MB at 320x240, 4.3 MB at 1280x800, 64 MB at 4096x4096. A font file takes its own size and some 70 bytes:
the 16 fonts of a project, 162 KB, took 0.2 MB. Its memory does not shrink: after a large panel a small one holds
what the large one took, until the preview is let go. Pages and fonts given again and again do not grow it.

## Making it again

```
sudo apt install clang-21 lld-21 wasi-libc libclang-rt-21-dev-wasm32
make DGX=<the registry's copy of DGX>           # dgx_ui_preview.js and licenses/ made again here
make DGX=<the registry's copy of DGX> check     # made in a directory of its own and held to the files here
```

The toolchain is Ubuntu's. DGX is the registry's copy at the version dgx-ui's manifest asks for at least, a project's
`managed_components/jef-sure__dgx`; a checkout or another version is refused. The same sources and toolchain give
the same bytes, with no path and no time in them. The head of `dgx_ui_preview.js` names both and a hash of every
source, and `version()` gives the same.

| File | What it is |
| --- | --- |
| `dgx_ui_preview.js` | the module's bytes and the class over them: what a page loads |
| `dgx_ui_preview.h`, `dgx_ui_preview.c` | the module's own calls, C over dgx-ui; they build for a computer as well |
| `preview_class.js` | the class `DgxUiPreview` over those calls |
| `Makefile`, `preview_build.py` | the build |
| `index.html` | the page to try it by hand |
| `licenses/` | the licences of what is in the module |

## Licences

The module is dgx-ui and DGX, MIT, with five fonts of DGX and the parts of wasi-libc and compiler-rt it links, each
under its own licence. [`licenses/README.md`](licenses/README.md) lists them, and `licenses/taken.txt` what the link
took.
