# Changes

## 0.0.1 - 2026-11-17

- A widget is two parts: what the program says of it, `dgx_ui_widget_t`, constant and never written by the library,
  so it may lie in flash (52 bytes on a 32-bit chip); and its node in the tree, `dgx_ui_node_t`, in RAM (28 bytes).
  The tree keeps a pointer to the widget: give a static const object, or the program's own memory.
- Messages: a number of the program's and up to 12 bytes by value, or data by reference done with once; every
  widget offered a message in the order of the tree, one may claim it or keep its data; decoders, and
  `DGX_UI_MESSAGE` binding a type to a number; sending through a queue of FreeRTOS, from an interrupt too; a ticker
  that keeps at most one tick in the queue; code points typed into a field.
- The core: a tree of nodes in the program's array and in blocks of 64 past it, linked as in the DOM; the layout
  in rows and columns, made again only when the screen, the widgets or their sizes change; the state of a widget
  kept twice, as it is and as it was drawn; the pointer, the keys of a dialog, the buttons of a game pad with the
  repeat of a held key, and the focus. The program's changes tell on() nothing: it hears what the user did.
- A click: the widget a finger went down on stays pressed till the finger goes up, wherever it goes; let go within
  `DGX_UI_CLICK_MS` (500), not swiped nor dragged, it is a click, wherever it was let go. The pointer carries its time
  and a swipe told by the adapter.
- The text box: the string in the program's buffer, compared when it is set and cut on a whole letter.
- A text of several lines, `dgx_ui_look_lines`, named by a widget that wants it: laid word by word each time it is
  drawn, an important text scrolled a line every scroll_ms, one that is not cut with an ellipsis, letters appearing
  one by one where they will stand. The look of one line stays the default of a text, so a program of labels does
  not carry it.
- The painter: only what changed, in one batch, each widget under a clip of its box, the screen's own clip given
  back, the keyboard's band of DGX-DI left alone; the looks of a box, a frame and a text, a text set in a field of
  DGX 0.11.4 so that a value drawn again writes every pixel once; the font picked from the variants of a style,
  a text too long cut with an ellipsis. A rectangle of the screen composed from the root down, into a buffer too.
- The looks of a button (a face over a shadow, pressed onto it, focused, disabled), a switch, a slider, a picture or
  an icon of 1 bit (opaque: drawn again it does not blink) and of RGB565. A control names its look, so a program
  carries only the looks it uses; each writes every pixel of its box once in any state.
- A popup over a tree: a tree of its own in its rectangle, the tree under it drawn around it, the pointer and the
  keys to it first, modal or closed by a finger outside; where it lay drawn again when it closes.
- The touch of DGX-DI (`dgx_ui_di.h`, when DGX-DI is there): its first finger as the pointer of a tree, its gestures
  sorted between the widgets and the program; a widget dragged owns the finger; a widget swiped, from an edge too, is
  no click. A field typed into is drawn with a
  caret where the next letter goes.
- Rectangles of a screen (`dgx_ui_rect_t`): their common part, and the parts of a box outside a cut.
- Host tests; an example for the linux target of ESP-IDF, in a window of DGX.
