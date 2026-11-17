# Checks of the documentation

The pages of `README.md`, `README-ru.md`, `docs/en` and `docs/ru` are checked against the headers, and their pictures
are drawn by dgx-ui itself.

```sh
python3 test/docs/check_docs.py          # DGX and DGX-DI are taken from checkouts beside this one
python3 test/docs/check_docs.py <DGX> <DGX-DI> <build dir>
```

It checks that:

- the C code of every page compiles against `include/`, with the stubs of `test/docs/stubs` for FreeRTOS;
- every `dgx_ui_` and `DGX_UI_` name a page prints is in a header;
- the API pages name every public function, type and constant of the headers;
- the links between the pages lead to files that are there.

The code blocks of a page, in their order, are one file of C: a block is whole declarations and functions, and the
Russian pages hold the same blocks as the English ones.

`run_cookbook.c` runs the recipes of the cookbook with the program's own functions stubbed; given a directory, it also
draws there the pictures of the ring scale, `ring.ppm` and `ring-layout.ppm`, by the very code of the page.
`pictures.c` draws the other pictures of `docs/images` (as PPM, to be turned into PNG). Both are built as the tests of `test/host` are, with the sources
of dgx-ui and of DGX for the computer.
