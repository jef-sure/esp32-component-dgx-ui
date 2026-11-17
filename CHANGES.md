# Changes

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
