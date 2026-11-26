# dgx_ui_c: a description made C

`dgx_ui_c` reads a description of a tree in JSON, the format of `dgx_ui_load.h`, and writes C: the widgets of the
very tree `dgx_ui_load()` builds of it, as constants, and a function that adds them. A program then shows the tree
with no loader and no tables of names, and the description stays the source: it is made C again when it changes.

From a names file, the program's vocabulary, it also writes the tables of names `dgx_ui_load()` takes, so a program
that loads descriptions and one that has them made C read the same names from one place.

The tool is a program of the computer. It comes in the component's package as source and is built on the computer;
nothing of it goes on a chip.

A description can be looked at before it is made C: [the preview](../editor/preview/README.md) draws it in a
browser as the board draws it, on the same names file, and says where the loader refuses it.

Beside it here, `dgx_ui_serve.py` serves the builder of pages from a terminal, for a browser that gives a page no
folder to open: [From a terminal](../editor/README.md#from-a-terminal). `dgx_ui_mcp.py` serves it with an agent
beside it, over MCP: [With an agent](../editor/README.md#with-an-agent).

## From CMake

A program built by ESP-IDF does not run the tool itself. Three calls in the CMakeLists of a component, after
`idf_component_register()`, build it once for the project and run it when the program is built:

```cmake
dgx_ui_tree(sound.json NAMES vocab.txt HEADERS program.h)   # sound.c, sound.h: int sound_add(dgx_ui_t *ui, int parent)
dgx_ui_names(vocab.txt HEADERS program.h)                   # vocab.c, vocab.h: const dgx_ui_load_names_t vocab_names
dgx_ui_appearance(day.json NAMES vocab.txt HEADERS program.h)   # day.c, day.h: const dgx_ui_appearance_t day_appearance
```

The calls are in the API reference, [project_include.cmake](../docs/en/api.md#project_includecmake), and the
cookbook has [a recipe](../docs/en/cookbook.md#a-description-made-c-when-the-program-is-built). The rest of this
page is the tool as the calls run it, and as it is run by hand.

## Build

```sh
python3 tools/dgx_ui_c.py <path to DGX> <directory>    # <directory>/dgx_ui_c: the one way it is built
make -C tools                                          # the same into tools/build, DGX found as the host tests find it
make -C managed_components/jef-sure__dgx-ui/tools BUILD=$PWD/build/tools   # in a project: DGX found beside it
```

In a project the tool is best made outside `managed_components/`, which is the component manager's to replace: an
update of dgx-ui takes its folder away, and a tool made in it with it. The calls of `project_include.cmake` make it
in the build by themselves.

The tool is `tools/dgx_ui_c.c`, `tools/dgx_ui_names.c`, which reads a names file, and nine sources of the library,
compiled against dgx-ui's headers and DGX's `include/` and `host/stubs/`: no source of DGX, no library but C's. The
compiler is the computer's: `$CC` unless it is a chip's, else `cc`. The library's looks are read from
`include/dgx_ui_paint.h` when it is built.

## Use

```sh
tools/build/dgx_ui_c [-n <names file>] [-i <header>]... [-d <directory>] [-p <prefix>] [-f <files' name>] <description>
tools/build/dgx_ui_c -n <names file> [-i <header>]... [-d <directory>] [-f <files' name>] -t <prefix>
tools/build/dgx_ui_c [-n <names file>] [-i <header>]... [-d <directory>] [-p <prefix>] [-f <files' name>] -A <appearance>
```

The first makes a tree C, the second the loader's tables of a names file,
[below](#the-loaders-tables-from-the-names-file), the third an appearance, [below](#an-appearance-made-c) too. A tree
and the tables are made on an appearance by `-a <appearance>` with the first two.

For a description it writes `<prefix>.c` and `<prefix>.h` into the directory (`.` when none is said). The prefix, a
name of C, begins all the files declare. It is the tree's name, the `"name"` of the description: one that says
`"name": "settings"` gives `settings.c`, `settings.h` and `settings_add()`. `-p` gives another prefix, and is needed
when the description has no name, or one that is not a name of C, `"wifi-setup"`; the files then say both names in
their first lines. A prefix may not begin with `dgx_`, as the libraries' names and headers do. `-i` names a header
of the program the `.c` includes, the one that declares what the names lead to. `-f` names the two files when they
are not to be named by the prefix: `dgx_ui_tree()` names them by the description's file, which CMake knows without
reading it, and leaves the prefix to the tree's name. Such a name is of letters, digits, `_`, `-` and `.`, neither
`-` nor `.` first.

A file that is already there is written over only when the tool made it, and made it of the same kind of source, by
its first lines: a tree named `main` beside the program's `main.c` is refused, and so is a tree over the tables of
a names file. Neither file is then written; `-p`, `-t` or `-d` gives the files another place.

The header declares:

```c
int settings_add(dgx_ui_t *ui, int parent);  /* the tree added under parent (-1: the root); its top, or -1 */
extern int settings_title;                   /* with no names file: an id, the widget's index once added */
extern dgx_ui_text_style_t settings_style;   /* with no names file: the style of a text that names none */
```

The widgets are constants in flash, `<prefix>_widgets`, with their parents in `<prefix>_parents`. The text boxes are
in RAM, `<prefix>_texts`: a constant text over its string literal, a text of `"text_size"` over so many bytes of
`<prefix>_buffers`, filled by the add function with the description's text each time it adds the tree. The four
arrays are the file's own, `static`; one that would be empty is not made. When the tree has no room for a widget,
the function removes what it added and writes no id, as the loader does.

## How a name becomes C

The description is read by the loader itself, on tables of stand-ins: every check and every default is the
loader's, and the tree it lays is printed, each stand-in turned back into the C it stands for. A description the
loader refuses is refused with the loader's error, its byte and its name:

```
dgx_ui_c: page.json: byte 50: DGX_UI_LOAD_UNKNOWN "loudness"
```

An editor's object, `"editor"` at the top of a description or in a widget, is passed over as the loader passes it:
the files made are the same with it and without.

What the names stand for comes from a names file, or, with none, from a rule.

### The names file

A names file is the program's vocabulary: what a description may name, and the C each name leads to. A line is a
name, `#` to the end of a line a comment:

```
# table   name       C expression           a count of pictures, or "number"
look      slider
look      tall       &tall_slider
value     volume     &settings.volume
picture   dots       dots                   2
style     text       &text_style
on_event  step       on_step                number
on_event  back
take      co2        take_co2
ctx       settings   &settings_page  page
id        title      title_index
```

- The tables are `look`, `value`, `picture`, `style`, `on_event`, `take`, `ctx` and `id`.
- A name is a name of the format: letters, digits, `_`, `.` and `-`, at most 23. A name twice in a table is refused.
- The expression is C written with no spaces. A look of the library's goes without one, `look slider` is
  `&dgx_ui_look_slider`, and so do the actions `on_event back` and `on_event page`, `dgx_ui_action_back` and `_page`.
- A picture's count is 1 when none is said; `number` marks a handler whose ctx is a number, and `page` a context
  that is a page's tree, the only context the action `page` takes.
- The expression of an id is an int of the program's, which the add function writes the widget's index to; the
  others are constant expressions, as the widgets are constants.
- A table has at most 65535 names, the ids' `DGX_UI_LOAD_MAX_IDS`, 256.
- The builder of pages edits a names file a line at a time and keeps every other line as written:
  [The names file](../editor/README.md#the-names-file).

With a names file every table is the file's, as the program's tables are for the loader. A name the file has not
is refused; so is an icon's range past its pictures, a number for a ctx its handler does not take, and a context not
a page's for the action `page`. A table the file does not name is empty: a text finds no style, a widget no look, and
an id the file has not is passed over, as the loader passes it over, with no int made for it. The first style of the
file is the one a text takes that names none.

### With no names file: the rule

The tables are made of the names the description uses:

| Table | C |
| --- | --- |
| look | a word of the library's own looks `&dgx_ui_look_<word>`; any other name `&<name>`, the program's own look |
| value | `&<name>`: `"settings.volume"` is `&settings.volume` |
| picture | `<name>`, an array of as many pictures as the widget reads |
| style | `&<name>`; a text that names none `&<prefix>_style`, which the program defines |
| on_event | `back` and `page` the library's actions, `dgx_ui_action_back` and `_page`; any other `<name>` |
| take | `<name>` |
| ctx | `&<name>`; an integer `(void *)(intptr_t)<n>` |
| id | `int <prefix>_<id>`, declared by the header |

A name the program has not is then the C compiler's to find, and a name the rule cannot make C, one with `-`, is
refused with the name and its byte. So is a name that would be one the tool makes itself: an id may not be `add`,
`style`, `widgets`, `parents`, `texts` or `buffers`, and a name of any table may not lead with one of the four
arrays, `<prefix>_texts` or `<prefix>_texts.first`. A names file gives such a name its expression, or `-p` the tree
another prefix.

Without the program's tables some of the loader's checks cannot be made, and the first comment of each file made
says which: the pictures' counts, which handlers take a number in their ctx and which contexts are pages, which looks
the program has, and the
style a text that names none takes.

## The loader's tables from the names file

```sh
tools/build/dgx_ui_c -n vocab.txt -i program.h -t vocab
```

writes `vocab.c` and `vocab.h` from the names file alone: the tables of `dgx_ui_load.h`, constants, each entry a
line of the file in the file's order, and

```c
extern const dgx_ui_load_names_t vocab_names;  /* for dgx_ui_load_need() and dgx_ui_load() */
```

A table the file does not name is `NULL` with the count 0. The arrays are the file's own, `static`, named by the
fields they fill: `vocab_looks`, `vocab_values`, `vocab_pictures`, `vocab_styles`, `vocab_handlers`, `vocab_takes`,
`vocab_contexts`, `vocab_ids`. `-t` takes no description and no `-p`; its prefix is a name of C that does not begin
with `dgx_`.

The file then means one thing to both: with `-n vocab.txt` the tool's word on a description is the loader's on
`vocab_names`, the same tree, or the same error at the same byte with the same name. A program may load a
description while it is being worked on and have it made C for its release, with one vocabulary.

## An appearance made C

```sh
tools/build/dgx_ui_c -n vocab.txt -i program.h -A day.json
```

writes `day.c` and `day.h` from an appearance of `dgx_ui_appearance.h`, read by the appearance's own loader on
stand-ins, as a tree is: a refusal is the loader's code, byte and name. The prefix is the appearance's `"name"`,
or `-p`. The header declares:

```c
extern const dgx_ui_appearance_t day_appearance;   /* its colours, and the tables of its looks and styles */
extern const dgx_ui_look_t       day_look[2];      /* its looks, constants in flash with their parameters */
extern const dgx_ui_text_style_t day_style[1];     /* its styles, constants as their variants and fonts are */
enum { day_color_paper = 0, day_color_ink = 1, day_color_fill = 2, day_color_border = 3, day_color_accent = 4 };
enum { day_stamp = 0x416f7392 };                   /* of its looks' and styles' names in their order */
```

A colour whose name is not a name of C has no constant: the header lists it in a comment, and
`dgx_ui_appearance_color()` finds it. In `day.c` a colour that is one of DGX's named colours is written by its name,
`DGX_ORANGE(DGX_RGB_24)` for `#ffa500`, and any other as its number, `0x123456`. A value with several names takes the
first in `dgx_colors.h`: `DGX_CYAN` for `#00ffff`, not `DGX_AQUA`. The names are read from the `dgx_colors.h` of
the DGX the tool is built with, and the numbers are the same either way. The tables hold the appearance's looks and
styles first, then the looks and styles of the names file.

What an appearance may name comes from the names file, by three more tables and its pictures, or with none from a
rule:

```
picture   card       card_pieces 9            # pictures, as a page's: the same line serves both
draw      slider                              # a draw of the library's, with no function
draw      dial       draw_dial  &dial_described   # the program's own: its function, and its description for the tables of -t
font      small      dgx_font_small           # a font of DGX by its constant; with none, dgx_font_<name>
font      script     dgx_font_script handwritten   # a handwritten font: its variant must say a hand
hand      handwriting                         # handwriting or handwriting_canvas, the library's
```

With no names file every draw of the library and both hands may be named, a font `small` is `dgx_font_small`, a
picture `card` is the array `card` of 65535 pictures, as a page's by the rule, and every font is taken for a bitmap
one: the file made says that the hands of handwritten fonts were not checked. A
draw of the program's own may get a look with a fill; a look that gives it `"params"` is refused, in the tool's
words, since its parameters are described in the program's C, which the tool does not read.

**A tree, or the tables, on an appearance**: `-a <appearance>`, with `-an <its names file>` when the appearance was
made on one, `-ap <its prefix>` when it was made with `-p`, and `-af <its files' name>` when with `-f`. The
appearance's looks and styles stand before those of the names file or of the rule, and are written
`&day_look[0]`, `&day_style[0]`; the file made includes `day.h` and holds its stamp by a `_Static_assert`, so a
file kept while its appearance changed stops the compile: "day.h is of another appearance than this file was made
on: make it again". A text that names no style takes the appearance's first. With `-t`, the tables' looks and styles
begin with the appearance's, and a names file with `draw`, `font` or `hand` lines also gives
`const dgx_ui_appearance_names_t <prefix>_appearance_names`, the tables an appearance is loaded on, its pictures
the same table as the pages'.

## Tests

`make -C test/host run-test_generate`, a part of the host suite (`test/host/test_generate.py` and
`test_generate.c`): descriptions of every kind and key, trees made by chance by the names file and by the rule, and
the descriptions the loader refuses. Each file made is compiled and its tree added beside the loaded one: the same
widget by widget and field by field, the same ids, drawn the same pixel by pixel. Each refusal is the loader's, the
same code, byte and name. The tables made of the names file are held against tables written by hand, entry by
entry, and every case is loaded on them as well.

`make -C test/host run-test_generate_appearance` does the same for appearances: one of every draw of the library,
appearances made by chance from the library's descriptions of its draws, and the same spoiled. Each is made C,
compiled, and held against its load on the tables `-t` makes, field by field and pixel by pixel on a page of a widget
for each look; a tree made on one appearance is shown to stop at compile time against another.
