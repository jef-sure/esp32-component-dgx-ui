# Checks of the documentation

The pages of `README.md`, `README-ru.md`, `docs/en` and `docs/ru` are checked against the headers, and their pictures
are drawn by dgx-ui itself.

```sh
python3 test/docs/check_docs.py          # DGX and DGX-DI are taken from checkouts beside this one
python3 test/docs/check_docs.py <DGX> <DGX-DI> <build dir>
```

It checks that:

- the C code of every page compiles against `include/`, with the stubs of `test/docs/stubs` for FreeRTOS, and with
  the fonts the cookbook's needs name, `dgx_fonts.h` and `dgx_fonts.c` made by DGX's `font2c/dgx_fonts.py` of
  `fonts.needs` and `fonts.rules` here, as a program's build makes them: soft-edged, 4 bits a pixel;
- the files the cookbook prints for its recipe of a description made C, the description, the names file and the
  program's header, are made C by `dgx_ui_c` itself, built here by `tools/dgx_ui_c.py`, and what it makes compiles;
  so are those of the recipe of an appearance made C: the appearance, the lines it adds to the names file and to
  the header, and the page of the first recipe made a second time on that appearance, under the prefix `sound_day`,
  which the run of the recipes then adds and holds to the appearance's looks and style;
- the recipes of a picture from a PNG file and of an icon with soft edges are built as a project builds them: their
  `images.txt`, with the PNG files this checker writes, a sun in the appearance's colours, a drop of one colour and a
  ball of several with soft edges, is made into files of pictures by DGX's own `tools/dgx_images.py build`, with
  `dgx_assets.S` and `dgx_assets.h`; their pages are made C by `dgx_ui_c -g` on the appearance, and compile; the run of
  the recipes assembles the assets and draws the pages, their pictures `imported.png` and `soft-icons.png`;
- every `dgx_ui_` and `DGX_UI_` name a page prints is in a header or in `project_include.cmake`;
- the API pages name every public function, type and constant of the headers and every call of
  `project_include.cmake`;
- the links between the pages lead to files that are there, and a link to a heading to a heading that is there, on
  those pages and on the pages of `editor/`, `tools/` and this one;
- a number a page writes beside the name of a constant, as in "`DGX_UI_POPUP_DEPTH`, 8", is the header's value;
- a struct a page lists in C is the header's, member by member and in order; a listing whose last member is followed
  by a comment of its own, as `/* the rest is the library's */`, lists the first members only;
- the tables of the headers, in How it works and in the API page, name every header of `include/`;
- the cookbook's list at the top is its recipes, in their order, each leading to its own heading;
- a size a page marks is the one the headers assert for a 32-bit chip: `28 bytes<!-- sizeof dgx_ui_node_t -->`, or a
  product, `1792 bytes<!-- sizeof dgx_ui_node_t * DGX_UI_BLOCK -->`. The mark is a comment, not seen on the page;
  the size is an `_Static_assert` of the header, which a chip's build holds to the struct. A size written with no
  mark is not checked;
- a version a page names is the manifests': "Version 0.0.52" and `version: "0.0.52"` are dgx-ui's own, "DGX 0.12.13 or
  a later" the least DGX that `idf_component.yml` asks for, `` `jef-sure/dgx-di` 0.2.0 `` the least DGX-DI the
  examples ask for.

The code blocks of a page, in their order, are one file of C: a block is whole declarations and functions, and the
Russian pages hold the same blocks as the English ones.

Making those fonts asks of the machine what a program's needs ask of it: a C compiler, the development files of
FreeType, `pkg-config` and fontconfig, and one of the families of `fonts.rules`.

`run_cookbook.c` runs the recipes of the cookbook with the program's own functions stubbed, the made files of that
recipe and the fonts compiled in: the tree its function adds is held against the one loaded of the same file. Given a directory,
it also draws there the pictures of the ring scale, `ring.ppm` and `ring-layout.ppm`, and those of
`recipe_pictures.c`, a part of it: a picture for each recipe of a thing seen, made of the recipe's own widgets and
functions on a screen of 320x240 at 2 pixels a point, in the colours of the other pictures. What a scene adds of its
own, a caption beside a widget, it says where it adds it.

```sh
make -C test/host -f ../docs/run.mk run-cookbook            # the checker, then the recipes run
make -C test/host -f ../docs/run.mk run-cookbook OUT=/tmp   # and the pictures of the recipes drawn there
```

The pictures of `pictures.c` and of the run of the recipes are PPM, made PNG of the same pixels, a palette of 256
colours holding every level of the soft edges. `getting-started.png` is drawn by `picture_start.c`, the
getting-started page's program, in the fonts DGX makes of `start.needs` as that program's CMakeLists asks for them:

```sh
make -C test/host -f ../docs/run.mk pictures OUT=/tmp/pictures   # first, getting-started, settings, popup, lines, list, dropdown, background, states, scroll
python3 -c "from PIL import Image; import sys; [Image.open(f'/tmp/pictures/{n}.ppm').convert('P', palette=Image.ADAPTIVE, colors=256).save(f'docs/images/{n}.png', optimize=True) for n in sys.argv[1:]]" list dropdown
```

The run of the recipes draws `ring`, `ring-layout`, `nine`, `rows`, `line`, `face`, `slider`, `stepper`, `radio`,
`level`, `icon`, `picture-button`, `picture`, `log`, `font`, `soft`, `hand`, `night`, `show`, `bar`, `slide`, `field`,
`own-look`, `own-widget`, `canvas`, `graph` and `turn`; the same line, given its directory, makes them PNG.

`docs/images/strip.png` is the example `examples/controls` itself, built for the computer and stopped while the PIN is
typed:

```sh
cd examples/controls
SDL_VIDEODRIVER=offscreen DGX_HOST_SCRIPT=script.txt DGX_HOST_QUIT_MS=4950 DGX_HOST_SHOT=strip.ppm \
    ./build/host_cyd_s028r/controls.elf
```

`pictures.c` draws the other pictures of `docs/images` (as PPM, to be turned into PNG). Both are built as the tests of `test/host` are, with the sources
of dgx-ui and of DGX for the computer.

The pictures of the builder, `docs/images/builder-*.png`, are the builder itself, `editor/index.html` in a headless
Chrome, on the project of `test/hook/project` held in the page's memory; `test/editor/shots.js` says what each shows
and what it holds them to. They are made, and written only when both of its runs give the same bytes, by

```sh
make -C test/host builder-shots                     # into docs/images
make -C test/host builder-shots OUT=/tmp/shots      # elsewhere
```

Another machine's fonts may draw their letters otherwise, so the target is not one of the tests.
