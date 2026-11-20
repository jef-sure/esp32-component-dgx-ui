# dgx_ui_c: a description made C

`dgx_ui_c` reads a description of a tree in JSON, the format of `dgx_ui_load.h`, and writes C: the widgets of the
very tree `dgx_ui_load()` builds of it, as constants, and a function that adds them. A program then shows the tree
with no loader and no tables of names, and the description stays the source: it is made C again when it changes.

The tool is a program of the computer. It is not part of the component's package, and nothing of it goes on a chip.

## Build

```sh
make -C tools                      # tools/build/dgx_ui_c
make -C tools DGX=<path to DGX>    # another copy of DGX
```

DGX is taken as the host tests take it: from the managed components of the project dgx-ui sits in, else from a
checkout beside dgx-ui's (`../esp32-component-dgx`). It needs a C compiler of the computer; the library's looks are
read from `include/dgx_ui_paint.h` when it is built.

## Use

```sh
tools/build/dgx_ui_c [-n <names file>] [-i <header>]... [-d <directory>] <description> <name>
```

It writes `<name>.c` and `<name>.h` into the directory (`.` when none is said). `<name>` is a name of C, the prefix
of all the files declare. `-i` names a header of the program the `.c` includes, the one that declares what the
names lead to. The header declares:

```c
int settings_add(dgx_ui_t *ui, int parent);  /* the tree added under parent (-1: the root); its top, or -1 */
extern int settings_title;                   /* an id, by the rule: the widget's index once added */
extern dgx_ui_text_style_t settings_style;   /* by the rule, when a text names no style */
```

The widgets are constants in flash; the text boxes are in RAM, a constant text over its string literal, a text of
`"text_size"` over a buffer of that size, filled by the add function with the description's text each time it adds
the tree. When the tree has no room for a widget, the function removes what it added and writes no id, as the
loader does.

## How a name becomes C

The description is read by the loader itself, on tables of stand-ins made of the names it uses: every check and
every default is the loader's, and the tree it lays is printed, each stand-in turned back into the C it stands for.
A description the loader refuses is refused with the loader's error, its byte and its name:

```
dgx_ui_c: page.json: byte 50: DGX_UI_LOAD_UNKNOWN "loudness"
```

A names file gives the program's tables, a line a name, `#` to the end of a line a comment:

```
# table   name       C expression           a count of pictures, or "number"
value     volume     &settings.volume
picture   dots       dots                   2
on        step       on_step                number
on        back       dgx_ui_load_on_back
style     text       &text_style
id        title      title_index
```

The tables are `look`, `value`, `picture`, `style`, `on`, `take`, `ctx` and `id`. A picture's count is 1 when none is
said; `number` marks a handler whose ctx is a number. The expression of an id is an int of the program's the add
function writes the widget's index to; the others are constant expressions, as the widgets are constants. A table
the file names is the whole truth for it, as the program's table is for the loader: a name it has not is refused,
an icon's range past its pictures and a number for a ctx its handler does not take too.

A table the file does not name is made of the names the description uses, by this rule:

| Table | C |
| --- | --- |
| look | a word of the library's own looks `&dgx_ui_look_<word>`; any other name `&<name>`, the program's own look |
| value | `&<name>`: `"settings.volume"` is `&settings.volume` |
| picture | `<name>`, an array of as many pictures as the widget reads |
| style | `&<name>`; a text that names none `&<prefix>_style`, which the program defines |
| on | `back` and `page` the library's actions, `dgx_ui_load_on_back` and `_page`; any other `<name>` |
| take | `<name>` |
| ctx | `&<name>`; an integer `(void *)(intptr_t)<n>` |
| id | `int <prefix>_<id>`, declared by the header |

A name the program has not is then the C compiler's to find, and a name the rule cannot make C, one with `-`, is
refused with the name. Without the program's tables some of the loader's checks cannot be made; the first comment
of each file made says which: the pictures' counts, which handlers take a number in their ctx, which looks the
program has, and the style a text that names none takes.

## Tests

`make -C test/host run-test_generate`, a part of the host suite (`test/host/test_generate.py` and
`test_generate.c`): descriptions of every kind and key, trees made by chance by the names file and by the rule, and
the descriptions the loader refuses. Each file made is compiled and its tree added beside the loaded one: the same
widget by widget and field by field, the same ids, drawn the same pixel by pixel. Each refusal is the loader's, the
same code, byte and name.
