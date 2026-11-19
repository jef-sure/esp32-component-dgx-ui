# Changes

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
