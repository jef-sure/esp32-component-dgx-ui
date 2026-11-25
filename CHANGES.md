# Changes

## 0.0.59 - 2026-11-25

- The builder edits the appearance by forms: colours by hex, a picker or a DGX name, styles and looks by their draw's
  fields, drawn at once; a look or style renamed in the pages that name it, undone together.
- The builder's server writes an appearance already there; hello packs its colours for the screen it finds.
- Docs: the builder's appearance forms, the preview's appearanceFormat() and colorNames(), a named colour packed for a
  screen of any depth.

## 0.0.58 - 2026-11-25

- A picture's `scale` in dgx_ui_picture_t: 0 draws it pixel for pixel, 1 a pixel of it as a point of the tree, more as
  so many points; every look of a picture and its widget's size count it.
- Docs: the picture's scale in the API page and the recipe of a picture from flash.

## 0.0.57 - 2026-11-25

- A list or a dropdown picks only within its rows, one row too; with none, nothing.
- A control of no range draws and steps without overflow at any value.
- A held key ends when the focus moves to what does not repeat.
- No crossfade of a hidden place; a square panel turned in any way is drawn again.
- Docs: the rule of a list's and a dropdown's value.

## 0.0.56 - 2026-11-25

- A handler and a context of the names tables say their kind: the library's page action takes only a page's tree;
  `takes` in place of `takes_number`, `page` on a context, `ctx <name> <expr> page` in a names file.
- The loader refuses more than 32,767 widgets before it adds any.
- dgx_ui_c makes trees on a 32-bit computer too.
- Docs: the kinds of a context in the API page and the tool's, and the word `page` of a names file.

## 0.0.55 - 2026-11-25

- The text style's list of fonts is named `fonts`, as the appearance's JSON names it; `variants` is gone.
- Docs: the pages' code and the listing of the style name `fonts`.

## 0.0.54 - 2026-11-24

- The builder's server writes only the files the project's dgx_ui_tree(), dgx_ui_appearance() and dgx_ui_names()
  calls name, and pages already there.
- It refuses a body cut short.
- Docs: the builder from a terminal, what its server writes.

## 0.0.53 - 2026-11-24

- An example to start a project from, examples/hello: a text and a count another task sends once a second, in fonts
  DGX ships and its named colours, for a CYD and a window on the computer.
- A switch or a radio whose handler removes it or frees its tree at a click: nothing of it read after.
- `dgx_ui_dump()` no longer stops a build for a chip on ESP-IDF 5.2 to 5.4.
- Docs: plainer Russian in three pages.

## 0.0.52 - 2026-11-22

- The builder served from a terminal, for a browser that gives a page no folder to open, as VS Code's:
  `tools/dgx_ui_serve.py`, run in a project's folder, prints a link that opens the project at once; reached from this
  computer only, it writes the builder's descriptions, appearances and names files, nothing else.
- DGX ^0.12.3: its colours' names of CSS with CSS's values.
- Docs: the builder from a terminal; the getting-started program in `DGX_LIME` and `DGX_GRAY`.

## 0.0.51 - 2026-11-22

- A bitmap variant of points: `.points = true` in `dgx_ui_font_variant_t` draws every pixel of its font as a square of
  the tree's unit, crisp, and every measure of the text counts it; in an appearance `"points": true`, refused beside
  `"hand"`. The variant is 20 bytes.
- `dgx_ui_variant_line()` and `dgx_ui_textbox_height()` take the tree; NULL for a unit of 1.
- Docs: a font as it is in the API page and in the recipe of one tree for every board.

## 0.0.50 - 2026-11-22

- A text's state for the program: `dgx_ui_text_state()`, whether its letters are still coming and whether its look has
  shown all of it; `dgx_ui_text_whole()`, its letters shown at once, its scroll left to run.
- A modal popup takes a finger's press inside it as it takes its move and its lift: `dgx_ui_pointer()` true for each.
- Docs: a new getting-started page: a round CO2 scale on a GC9A01 fed by an SCD41 sensor, on an ESP32-C3 or in a window
  on the computer, in fonts made at build time with soft edges.

## 0.0.49 - 2026-11-22

- Sizes, places, pads and gaps in percent: `DGX_UI_PERCENT_W`, `_H`, `_X`, `_Y`, `_PAD`, `_GAP` and `DGX_UI_PERCENT()`;
  in a description `"50%"`, which a loader before 0.0.49 refuses as SYNTAX.
- A widget's `state` follows its `kind`, its `flags` are 16 bits, and `pad` and `gap` 16 bits, up to 32767 points;
  still 56 bytes.
- dgx_ui_c writes percents; the preview and the builder take them.
- Docs: the recipe of one tree on every board.

## 0.0.48 - 2026-11-22

- The builder of pages, editor/index.html, a first cut opened from the disk: a project's pages found by its
  CMakeLists' calls and drawn by the preview at the board's size, their widgets selected, set, added, moved and
  wrapped with undo; only the files edited are saved.
- The preview reads an appearance on its own names file as dgx_ui_c does (`preview_appearance_on()`,
  `appearance(text, names)`), and its class gives `format()`, `tables()` and `json()`.
- Docs: the recipes, the pictures and the keypad of examples/controls in soft-edged fonts made at build time; the docs
  check needs FreeType's development files, pkg-config and fontconfig.

## 0.0.47 - 2026-11-22

- DGX 0.12.1 at least, whose glyphs give their data by offsets: a font built by hand takes its new form, a glyph's
  `.offset` and the font's `.glyphs` and `.data`.
- The preview of a description in a browser, editor/preview: dgx-ui built to WebAssembly in one file a page loads with
  no server, a page to try it by hand, and the fonts of a project given as the files DGX writes for it.
- dgx_ui_c's reading of a names file in a source of its own, tools/dgx_ui_names.c.
- Docs: the recipe of a line between rows.

## 0.0.46 - 2026-11-21

- DGX 0.12.0 at least, whose fonts are constants. A font in a variant or in a table of fonts is `&Name` where it was
  `Name()` or `Name`: a table of fonts holding `Name` no longer compiles, and `Name()` elsewhere builds with DGX's
  warning until DGX 1.0. A variant and a style may now be constants in flash.
- `dgx_ui_c -A` makes an appearance of constants alone: `<prefix>_start()` is gone, so delete its call, and
  `<prefix>_style[]` is `const`, taken by a `const dgx_ui_text_style_t *`.
- A names file's `font <name> <C name>` names the font's constant, as font2c makes it since DGX 0.12.

## 0.0.45 - 2026-11-21

- `dgx_ui_c -A`: an appearance made C, its looks and their parameters constants, its styles' fonts filled by
  `<prefix>_start()`; `-a` makes a tree, or the loader's tables, on such an appearance, its looks and styles first,
  held to that very appearance by a stamp at compile time.
- A names file names an appearance's draws, fonts and hands too; with `-t` it gives the tables an appearance is loaded
  on as well.
- `dgx_ui_appearance()` in a component's CMakeLists, and `APPEARANCE` for `dgx_ui_tree()` and `dgx_ui_names()`.

## 0.0.44 - 2026-11-21

- The appearance as data, `dgx_ui_appearance.h`: colours by name, text styles and looks in one document of JSON for a
  program's pages, loaded into its memory. Its looks and styles stand before the program's own in the tables a page
  loads on, so a look named by a kind's word restyles every widget of that kind.
- The parameters of every draw of the library, of a text style and of a variant of a font described field by field;
  a program describes its own draw the same way.

## 0.0.43 - 2026-11-21

- A load takes the same stack however deep the description: about 1 KB on an ESP32, where a tree 24 deep took 9 KB.
- A name an error gives, cut at 23 bytes, ends on a whole character.
- DGX 0.11.8 at least: two soft letters meeting in a cell of a 16-bit display behind a bus show as composed in memory.

## 0.0.42 - 2026-11-21

- A description may hold `"editor"`, an object at the top and in any widget for the tools that edit it: any JSON in
  it, which the library and `dgx_ui_c` pass over.
- The top of a description is judged in one order: not JSON, then the version, then a key unknown or twice, then the
  values. A later version is refused as one whatever it holds, and a number where none is read is told by the fault
  of its place, not as out of range.
- A value passed over takes the same stack at any depth.
- `-f` of `dgx_ui_c` and the files named by a description's file refuse a first `-`.

## 0.0.41 - 2026-11-21

- `dgx_ui_tree()` and `dgx_ui_names()` in a component's CMakeLists: a description made C, or the loader's tables of a
  names file, at build time, compiled into that component; `dgx_ui_c` comes in the package and is built once for the
  project by the computer's C compiler.
- `dgx_ui_c -t`: the loader's tables of a names file, so that one file names a program's objects for the loader and
  for the trees made C.

## 0.0.40 - 2026-11-21

- A description may name its tree, `"name"`, and `dgx_ui_load_need()` gives the name out; a description of a later
  version is refused as one, before a key of the top this version does not know.
- `tools/dgx_ui_c`, a program of the computer, not in the package: a description made C, the constants of the very
  tree `dgx_ui_load()` builds of it and a function that adds them, named by the tree's name.
- The examples `controls` and `thin_paint` draw letters with soft edges, in fonts DGX makes at build time from a face
  of the computer.

## 0.0.39 - 2026-11-21

- A tree from a description in JSON, `dgx_ui_load()`: every pointer is a name of the program's tables, the widgets are
  laid in the program's memory, and the whole is checked before the first widget is added; `dgx_ui_load_need()` tells
  the bytes, the nodes and the depth it takes. A description reaches only what the tables give: a number for `ctx` only
  to a handler marked to take one, pictures no fewer than the widget shows, an id of the table on one widget.

## 0.0.38 - 2026-11-20

- The user's idle, `dgx_ui_idle()`: after so long with no finger, key or button, home is told by a message of the
  program's number, 1 when it comes and 0 when the user wakes it; the input that wakes it is spent unless the program
  says it acts; `dgx_ui_wake()` gives its moment.

## 0.0.37 - 2026-11-20

- A program with no finger no longer carries the finger's repeat: its step is named by the press that begins it, as the
  key's is by `dgx_ui_buttons()`.

## 0.0.36 - 2026-11-20

- A button of `DGX_UI_REPEAT` is told ACTIVATED when the finger goes down on it, then every 100 ms from 500 ms on
  while it is held, by `dgx_ui_time()`, and not when it goes up; the key ACTIVATE held on it with the focus repeats too.

## 0.0.35 - 2026-11-20

- A picture button, `dgx_ui_look_picture_button`: a button, a switch or a radio with a picture of 1 bit on the
  button's face, pressed and framed as a button; a switch shows its second picture while on, a radio while chosen.
- A button, a switch or a radio with a picture and no size of its own now asks for the picture's size and 5 points more
  across and down, whatever look draws it; before, it asked for none and took what its row or column gave it.

## 0.0.34 - 2026-11-20

- Home's pages can lie over the box of one of its widgets, `dgx_ui_page_place()`: the rest of home, a bar shared by the
  pages, stays on the screen, takes the finger and keeps its ticker; the keys go to the page; a transition moves the
  place alone.
- A dropdown's list opens over the top of the chain its tree is in and closes alone: a popup over the dropdown's tree
  that a finger outside does not close now stays under the list instead of being closed by it.

## 0.0.33 - 2026-11-20

- While a modal popup is open, the trees under it show no mark of the focus, and the mark comes back where the focus
  is when it closes; the focus itself stays. A look reads the bits it shows by `dgx_ui_bits_shown()`.

## 0.0.32 - 2026-11-20

- Needs DGX 0.11.6, whose hard pen gives the same pixels in a screen moved by whole pixels: handwriting drawn through
  the strips is now the same as handwriting drawn whole.

## 0.0.31 - 2026-11-20

- The ring scale draws whole about five times faster and a step three to five times faster, with the same pixels: the
  ring's part of a row is cut into runs of one class instead of being classed pixel by pixel. About 3.6 KB.
- `host/dgx_ui_host.mk` lists the looks of the radio and the checkbox, the level bar and the ring scale, so a program
  built on a computer with make can name them.

## 0.0.30 - 2026-11-20

- A ring scale, `dgx_ui_look_ring`: the value of a widget's place as an arc of a ring, coloured by bands of value,
  widgets inside it; every pixel once, the same pixels in any part and any strip, and when the value moves only the
  arc between the two values is written. About 3.3 KB for a program that names it.
- Bands colour by the value shown or by place (`bands_by`), in the level bar too.

## 0.0.29 - 2026-11-20

- A level bar, `dgx_ui_look_level`: the value of a widget's place as the filled part of its box from one edge, its
  colour by bands of value (`dgx_ui_band_t`); when the value moves, only the part that changed is written. About
  570 bytes for a program that names it.

## 0.0.28 - 2026-11-20

- A tree's root is told `DGX_UI_SHOWN` when the tree comes to the screen and `DGX_UI_COVERED` when a page covers it or
  it closes, in pairs, so that a page keeps its ticker only while it is seen.
- A root's `on()` is told two more events: one that took every call for `DGX_UI_BACK` must look at `what`.
- `dgx_ui_free()` closes the popups over a tree, or the tree itself when it is open as one, first. A tree is 104 bytes,
  was 96; about 650 bytes more for a program with popups, nothing for one without.

## 0.0.27 - 2026-11-20

- The focus is never shown in the accent: a button, a dropdown, a radio and a checkbox show it by a frame of a point
  round the widget (a button's round its face), a switch and a slider by their knob drawn hollow, in the colour
  `.focused` of their parameters, the ink by default. `.focused = DGX_UI_PAPER` shows no focus, for a program of the
  finger alone.
- A finger on a slider's or a switch's knob widens it by a point. A field's caret is in the ink.
- A button, a dropdown, a radio and a checkbox are by default a point higher above and below their text, the room of
  the frame; a radio's and a checkbox's mark stands two points in from the box's edges.
- About 50 bytes more for every program, 550 for one with a button, a dropdown, a radio or a checkbox, and 100 to 300
  for each look of a control it names.

## 0.0.26 - 2026-11-20

- The painter's look at the variables at the start of a pass takes about half the time: 17 us for a hundred widgets on
  an ESP32 at 240 MHz, 5 us on a P4.

## 0.0.25 - 2026-11-20

- A text box over a string constant, `DGX_UI_TEXT_CONST()`, and `dgx_ui_text_written()` for a buffer the program wrote
  itself.

## 0.0.24 - 2026-11-20

- Radio buttons, `DGX_UI_RADIO`: the buttons of a group name one variable, each its own number in `.step`, and the one
  whose number the variable holds is chosen; a click or the key writes its number. Their look `dgx_ui_look_radio`.
- A checkbox: a switch with a text, drawn by `dgx_ui_look_checkbox`.
- Both looks show the focus and a finger on them. A program that names neither carries neither; the checkbox's look is
  about 1.5 KB, the radio's beside it 220 bytes more, a program without them about 70 bytes more.
- A widget hidden, itself or by a box above it, whose variable is written no more sends the painter down to it at
  every pass.

## 0.0.23 - 2026-11-20

- A widget changed while something lay over it (the keyboard's band, a popup, a page, a dropdown's list) is drawn once
  when that goes, and so is the place of a popup closed and the band gone in one pass; they were drawn twice. About
  150 bytes more.

## 0.0.22 - 2026-11-20

- A widget's value lives in the program's memory: the widget names its place, `.value = &variable` (an `int32_t`), and
  the painter looks at every place at the start of a pass. The program writes its variable and calls nothing; saving
  settings is writing its own array. `dgx_ui_value()` and `dgx_ui_value_of()` read a value within the bounds, the
  memory left as it is.
- A program of 0.0.21 changes three things:
  - a first value, `.state = {.value = 5}`, becomes a variable of 5 and `.value = &variable`;
  - a handler's `node->state_value.value` becomes its own variable, or `dgx_ui_value(ui, i)`;
  - `state_value.bits` becomes `state.bits`.
- A slider, switch, list or dropdown with no place shows 0, nothing changes it, and its look says so in the log.
- A widget is 56 bytes of flash, was 52; the smallest program about 180 bytes of code more, a program of controls about
  650.

## 0.0.21 - 2026-11-20

- The statistics count a pass of `dgx_ui_changed()` too (`dgx_ui_changed_measured()`), the frames of a transition with
  the rate the eye sees (`dgx_ui_transition_frame_measured()`), and each widget of a tree the program names in an
  array of its own (`dgx_ui_widget_stats_t`); `dgx_ui_paint_stats_restart()` begins anew. The line says "its drawing".

## 0.0.20 - 2026-11-19

- The statistics of painting, `dgx_ui_stats.h`: `dgx_ui_paint_measured()` in the place of `dgx_ui_paint()` counts the
  passes and those that drew, their time, the looks' share and the slowest draw; `dgx_ui_paint_stats_log()` writes them
  in a line. A program that names none of it carries none.
- The pages and `dgx_ui_dropdown.h` say of the dropdown's list, the setters and on() what the code does since 0.0.15
  and 0.0.19.

## 0.0.19 - 2026-11-19

- Handwriting into a canvas: a second paint of the same tree no longer takes strokes from the display's canvas, nor
  keeps an old text's strokes in its own.
- A dropdown's options set while its list is open: the list shows them at once and keeps its window.
- Two trees drawn in turn by one paint with the same `now_ms`: `dgx_ui_wake()` after the last of them gives the least
  moment of all; it gave only the last tree's.
- Two trees of two tasks: the key back and `dgx_ui_page_back_by()` on one no longer disturb the other.
- The examples sleep by `dgx_ui_wake()`. Every program about 100 bytes more; handwriting into a canvas about 190 more.

## 0.0.18 - 2026-11-19

- `dgx_ui_look_lines`: while letters come, a pass draws only the row they came to; a text of several lines set in the
  middle of its box was drawn whole at every pass. About 150 bytes more.

## 0.0.17 - 2026-11-19

- Handwriting in lines, `dgx_ui_look_hand_lines`: a text laid word by word at a variant's height, a bitmap variant of
  its ladder drawn as `dgx_ui_look_lines` draws it; letters coming written line after line; an important text scrolled,
  one that is not cut with "..." in the same hand; straight or into the widget's canvas.
- The look of lines lays its lines by a source shared with it: about 30 bytes more.

## 0.0.16 - 2026-11-19

- Transitions between pages, `dgx_ui_transition.h`: `dgx_ui_page_open_by()` and `dgx_ui_page_back_by()` slide or
  crossfade from two pictures of the screen the program gives; the loop calls `dgx_ui_transition_frame()` in place of
  the pass while it runs. Without the pictures a page changes at once. A program that names none of it carries none.

## 0.0.15 - 2026-11-19

- Pages: `dgx_ui_page_open()` and `dgx_ui_page_back()`, a popup on the whole screen closed by the key back, its root
  told `DGX_UI_BACK` first. The popup flags `DGX_UI_POPUP_BACK` and `DGX_UI_POPUP_SCREEN`, and `dgx_ui_popup_back()`.
- With a dropdown's list open, the key back closes the list and no longer comes to the program.
- A modal popup that back does not close keeps back the program's: it no longer reaches the trees under it.
- A program with keys and no popup carries about 80 bytes more, one with popups and keys about 280.

## 0.0.14 - 2026-11-19

- `dgx_ui_changed()`: the rectangles that changed told to a function of the program and nothing drawn, so that the
  tree is a layer in another compositor's strips, which composes them by `dgx_ui_paint_area_to()`. A program that does
  not call it carries none of it.

## 0.0.13 - 2026-11-19

- Handwriting in the one-line look of a text: a variant of a handwritten font names `&dgx_ui_handwriting` (straight,
  a hard pen) or `&dgx_ui_handwriting_canvas` (into a widget's canvas, a soft pen too); the variant's new members
  `joined`, `pen` and `hand`, so every variant is 16 bytes, was 8. Letters coming are written stroke by stroke at the
  pace of a pen, a frame every `DGX_UI_HAND_FRAME_MS`.
- A program that names no handwriting carries none of it: about 100 bytes more in the text look.

## 0.0.12 - 2026-11-18

- A program that reads no buttons no longer carries the keys: about 1.4 KB less.

## 0.0.11 - 2026-11-18

- Widgets of the program's own: a look names a behaviour, `dgx_ui_look_t::behavior` (`down`, `move`, `up`, `key`),
  and the core works a `DGX_UI_OWN` widget drawn by it as one of its kinds: the hit, the press, the focus (with
  `DGX_UI_BEHAVIOR_FOCUS`), the click, the keys. Another component's widget is one constant, as a look is.
- `dgx_ui_user_value()` and `dgx_ui_tell()`: a value set and an event told as the user would cause them, for any widget.
- `struct dgx_ui_look_t` moved to `dgx_ui.h`; every look is 4 bytes larger.
- Fixed: a widget that `on()` puts in the slot of a list just clicked is no longer told `DGX_UI_ACTIVATED`.
- The mirror recipe draws the keyboard and the pad of DGX-DI 0.1.7 into its strips.

## 0.0.10 - 2026-11-18

- Time as "when": a look that asks for time names its moment, `dgx_ui_wake_in(paint, ms)`, and the paint's `wake_ms`
  after a pass is the least of them (0 the next pass, for a look that names none; `DGX_UI_WAKE_NONE` when nobody
  asks). `dgx_ui_wake(paint, ui, now_ms)` gives the loop how long it may wait, a held key's repeat counted, so a loop
  on a battery sleeps till then; the wait is a tick at least. The look of lines names its next letter and its next
  line up. `dgx_ui_paint()` and `dgx_ui_time()` return what they did: a loop written before works as before.
- `dgx_ui_dump(ui)`: the tree in the log, a line a widget indented by its depth (number, kind, place, value, state,
  marks, tag, the start of its text), each popup over it, and how many nodes it uses, with the size its array should
  have when blocks had to be taken. In a file of its own. The example controls prints its places by it.
- `dgx_ui_paint_area_to(ui, paint, scr, colors, x, y, w, h)`: a rectangle composed onto another screen, its corner at
  that screen's (0, 0), for a mirror of the display in strips; the paint's screen, colours and origin given back
  after, its canvases its own.

## 0.0.9 - 2026-11-18

- A list of more rows than its box takes scrolls, as DGX-GE's list does: it shows a window of them, as many as go in
  at a line of its text or at its `step` (now the least height of a row, in points, for a list), the first one shown
  its text box's `top`; a list whose rows all go in is drawn as before, row for row. The window follows the pick: the
  keys, the finger and `dgx_ui_set_value()` bring it to the pick (a mark, `DGX_UI_MARK_WINDOW`, for the look to do it
  when it draws), and `dgx_ui_text_scroll()` moves it, the pick staying. Promised for 0.0.4 and left out of it.
- The finger on a list that scrolls, as on DGX-GE's list: put down it picks nothing; moved up or down half a row's
  height, it moves the pick a row for each row's height it goes, up going on down the list, the window following; let
  go without having moved, a click picks the row it went down on. A list whose rows all show picks the row under the
  finger as in 0.0.4.
- The look of a list that scrolls draws a bar on its right edge, `dgx_ui_list_look_t::bar` (colour 3 by default, seen
  on the fill of a frame too), its length the window's share of the rows and its place the window's.
- A dropdown's list longer than the screen scrolls in place of its rows squeezed, its rows as high as the dropdown, its
  window at the option chosen, as many rows as the larger room under it or above it takes: it never lies over the
  dropdown. The warning of the squeeze is gone.
- A list's text set anew keeps its window: its rows are its range, not its text.

## 0.0.8 - 2026-11-18

- A canvas of a widget of `DGX_UI_CANVAS`, `dgx_ui_canvas(paint, ui, i, &made)`: an 8-bit virtual screen as large as
  its box, kept by the paint from pass to pass for a look that draws its picture on and on (digits that morph, a text
  written stroke by stroke); `made` says it is new and empty. Its box of another size, a new one; none to be had, the
  old one kept and marked `DGX_UI_MARK_HARD_SIZE`; none at all, `DGX_UI_NO_MEMORY` told once for that size after the
  pass. Freed when its widget leaves the tree, when its tree is not drawn by the paint's pass, and by
  `dgx_ui_paint_free()`. A program whose looks ask for no canvas does not carry it.
- `dgx_ui_canvas_put(paint, ui, i, ink)`: the canvas on the screen at the top left of its box, the rest of the box its
  paper: indices of the painter's table (`ink` -1, 0 the paper), or amounts of an ink over the paper, as a soft pen
  writes them; over a drawn background mixed with it where the screen can be read.
- A composing (`dgx_ui_paint_area()`, where a popup or the keyboard's band was, a drawn background changed) draws its
  looks at the time of the last pass, the paint's new `now_ms`, not at 0: a text that scrolls or whose letters come
  was composed at the wrong line, or all its letters there. A program that composes with a paint of its own alone
  sets `now_ms`.
- A message kept by a widget of one tree and then offered to a second tree (two displays) stays kept: before, the
  second `dgx_ui_take()` forgot it, and the loop's `dgx_ui_msg_done()` released the data the widget still held. The
  second tree's widgets are offered it as any message.

## 0.0.7 - 2026-11-18

- A drawn background, set by `dgx_ui_background_set(paint, draw, ctx)`: a function of the program's that puts a
  rectangle of its picture under the tree (a map, the frame of a game under a dialog), in the paint's
  `background_draw` and `background_ctx`. The call names the library's laying of it, so a program that never calls it
  does not carry it: about 0.5 KB, and some 0.3 KB in every program for the looks to know of it. Every look lays it
  where its paper is what lies under it and draws over it: letters without a field, set where the field sets them, a
  picture of 1 bit by its ones, one of 4 or 8 bits with its index 0 leaving the background. A frame that fills shows
  its fill; a popup's root that does not fill shows the picture. `background` stays the colour a look is told as its
  paper.
- `dgx_ui_lay_paper()`: the paper of a look laid, a colour or the drawn background; for a look of the program's own.
  The painter part of the paint has `on_background`.
- `dgx_ui_background_changed(ui, paint, x, y, w, h)`: the picture changed in a rectangle, laid there at once where it
  shows, the widgets on it drawn again over it; nothing within a widget that fills, under a popup or under the
  keyboard's band.
- The strips take a widget on a drawn background on a display of 16 bits too: the piece of the picture and the
  letters leave once, the soft edges of the letters mixed with the picture. A strip takes no more rows than its part,
  the rest of the buffer left to a look of its own (a picture of 4 or 8 bits in a strip).
- `dgx_ui_paint_area()` lays the drawn background where the root does not lie, as the composing where a popup or the
  keyboard's band was.

## 0.0.6 - 2026-11-18

- The strips, `dgx_ui_strips`, named by the program in the paint's new member `strips`: on a display behind a bus of
  18 or 24 bits, where a field of text is filled before its letters are written and blinks, every widget without
  children is drawn whole in strips of a virtual screen of the display's depth over the paint's buffer, each strip
  sent once: every pixel once. A widget changed is sent whole; a widget too wide for a row of the buffer, or with
  children, goes straight, as without the strips. They need the virtual screens of DGX; a program that does not name
  them does not carry them (the strips about 630 bytes, and some 5 KB of DGX's transfers).
- The build file for a PC, `host/dgx_ui_host.mk`, lists the looks of the list, the dropdown and indexed pictures,
  the dropdown's popup and the strips: before, a program on a PC that named one did not link.

## 0.0.5 - 2026-11-18

- A buffer of the program's in the paint, `buffer` and `buffer_size`, for the looks that draw through one.
- The look of indexed pictures, `dgx_ui_look_indexed`: a PICTURE or an ICON of a `dgx_image_t` of 4 or 8 bits, its
  values through the picture's map indices of the painter's table, its 0 the paper. Drawn into an 8-bit virtual screen
  over the buffer a strip of rows at a time, only the rows the clip shows, and sent through a table of the painter's
  colours; every pixel of the box once. A picture of 1 bit as `dgx_ui_look_picture` draws it, so an icon of pictures of
  several depths has one look. It needs the virtual screens of DGX, a buffer of a row and a 16-bit screen.

## 0.0.4 - 2026-11-18

- The look of a list, `dgx_ui_look_list`: its rows the lines of its text box, row r the line r and its value min + r,
  the rows sharing the height of the box, the row picked on a bar (colour 3), of colour 1 while the list has the focus,
  its rows of colour 2 while it is disabled. Another pick redraws the row picked before and the row picked now, and
  nothing else.
- A finger moved along a list picks the row under it, as a slider's knob follows the finger: the rows stand and the
  bar goes with the finger. Gone from the row it went down on, it is dragged: let go, no click. Before, a list moved
  as a page under the finger, the pick going the other way.
- The look of a dropdown, `dgx_ui_look_dropdown`: the option chosen (the line value - min of its text box) on a flat
  face of the colour of its state, and an arrow pointing down.
- A dropdown's list, `dgx_ui_dropdown_open()` (`dgx_ui_dropdown.h`), called on its DGX_UI_ACTIVATED: a popup under
  the dropdown, above it, or moved up to fit the screen, its rows as high as the dropdown; modal and closed by a finger
  outside. The row chosen sets the dropdown's value and tells it DGX_UI_CHANGED; the option it had, chosen again, tells
  nothing. One `dgx_ui_dropdown_t` serves every dropdown of a program.
- The example controls has a dropdown of modes beside its switch, and its script chooses one.

## 0.0.3 - 2026-11-17

- The look of a field (`dgx_ui_look_input`) sets its text in the middle of the field when the style says
  `DGX_UI_TEXT_MIDDLE`, as a label does.
- `DGX_UI_EDIT`: a field (INPUT) is told it on a click, by the rule of a button's click, and on `DGX_UI_KEY_ACTIVATE`
  while it has the focus; the program opens its editor on it. The focus no longer opens one: the walk of the keys
  passes the fields, a swipe that begins on a field opens nothing, a second click opens it again.
- A secret field: `dgx_ui_input_look_t::secret` draws a star for each letter, the text box keeping the letters (a
  password, a PIN).
- A widget whose `on(DGX_UI_FOCUS)` opens a popup over it (an editor of a field) is not left pressed under it: the
  finger that touched it is spent.
- An example of the controls worked by a finger, the keys and DGX-DI's keyboard on the display, `examples/controls`.
  A field is edited in a strip at the top of the display with a keypad of its own under it: a copy of the field's
  text, given to the field on done and dropped on cancel (the keypad's, or BACK).
  The board comes from jef-sure/boards: the CYD with its touch, calibrated by the board's data, and the computer
  showing the CYD's display in a window, the mouse its finger and the arrows and Enter its buttons; a script plays a
  run of every control there.

## 0.0.2 - 2026-11-17

- A box and a frame of a column or a row fill only what their children do not cover: drawn whole, every pixel of
  the screen is written once. A widget hidden, shown, added or removed no longer redraws the box and all its
  children: the parent is marked `DGX_UI_MARK_AROUND`, fills around its children, and only those that moved or came
  are drawn. On a display behind a bus nothing blinks. A box of neither a column nor a row is drawn whole, as before.
  `dgx_ui_fill_around()` is public, for a look of the program's own that holds children (a ring scale around the
  screen with the values inside it).
- The painter goes down only where something is to be drawn: a widget marked, or its state changed, marks the widgets
  above it `DGX_UI_MARK_BELOW`, and a pass skips every branch without it. A pass with nothing changed looks at the
  root alone. The widget under a finger is looked for only in the widgets the point lies in.
- A widget is drawn and touched only within its parent's box: what of it lies outside, laid larger than the box or
  put out of it, is cut off. A frame's border is drawn around its children too: a child laid over it lies over it.
- `on()` gets the widget's node: `void (*on)(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)`.
  The value the finger or a key set is `node->state_value.value`, with no call to look it up.
- `dgx_ui_button_keys()`: the key each button of a game pad gives, by the program's table (left and right moving the
  focus in a row of buttons).
- `DGX_UI_REST`: a child of a column or a row takes what the box has left after the others, the gaps and the pads;
  several share it, the last taking the odd pixels. A row of a name and a value at the right edge, or a column with
  buttons at the bottom, on any screen without sizes counted by hand.
- The draw functions of the looks are public: `dgx_ui_draw_box`, `_frame`, `_text`, `_lines`, `_button`, `_input`,
  `_switch`, `_slider`, `_picture`, `_rgb565`. A look in the program's own colours is a constant that lies in flash:
  `static const dgx_ui_look_t mine = {.draw = dgx_ui_draw_button, .params = &my_colours, .fill = -1};`.
- A widget shown with no look, which is never drawn, is told of once a tree in the log, with what to give it.
- Popups one over another: a popup is a tree, and may open its own, up to `DGX_UI_POPUP_DEPTH` (8) over a tree. A
  dialog, a list from it hanging out below it, a question over the list. Each tree is drawn around every popup over
  it; a finger down on a popup or on one over it is theirs, the top one first; keys and letters go to the top, messages
  to every tree. Closing a popup closes all over it. `dgx_ui_popup_open()` returns false when it refuses: the tree
  itself, a tree with a popup of its own, or too deep. In the paint, `popup_cover` became `popup_over`.

## 0.0.1 - 2026-11-17

The first version: a tree of widgets on DGX fed by messages, a painter that writes only what changed, the looks of
the controls, a popup, the touch of DGX-DI.
