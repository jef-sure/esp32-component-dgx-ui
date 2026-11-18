# API reference

*[По-русски](../ru/api.md)*

This page lists every public type, function and constant of dgx-ui, header by header. How they work together is
explained in [How it works](how-it-works.md); the [Cookbook](cookbook.md) shows them at work.

| Header | What it holds |
| ------ | ------------- |
| [`dgx_ui.h`](#dgx_uih) | rectangles, the state, a widget and its node, the tree, the layout, the pointer, the keys, the focus, a popup |
| [`dgx_ui_text.h`](#dgx_ui_texth) | fonts and styles of a text, the text box, setting a text |
| [`dgx_ui_msg.h`](#dgx_ui_msgh) | a message, offering it to a tree, decoders, code points typed |
| [`dgx_ui_post.h`](#dgx_ui_posth) | sending a message through a queue of FreeRTOS |
| [`dgx_ui_ticker.h`](#dgx_ui_tickerh) | a message every so many milliseconds |
| [`dgx_ui_paint.h`](#dgx_ui_painth) | the painter, the looks and their parameters |
| [`dgx_ui_dropdown.h`](#dgx_ui_dropdownh) | the list of a dropdown, opened as a popup |
| [`dgx_ui_di.h`](#dgx_ui_dih) | the touch of DGX-DI as the pointer of a tree |

All public names begin with `dgx_ui_` or `DGX_UI_`. Every header has `extern "C"` for C++. Version 0.0.11: the names
and the signatures may still change.

One task owns a tree. Every call below is made by that task, except those of `dgx_ui_post.h`, which any task may
call.

## dgx_ui.h

### Rectangles

```c
typedef struct dgx_ui_rect_t {
    int16_t x, y;
    int16_t w, h;
} dgx_ui_rect_t;
```

A rectangle of a screen in pixels. One of no width or no height is empty.

| Call | What it does |
| ---- | ------------ |
| `dgx_ui_rect_t dgx_ui_rect(int x, int y, int w, int h)` | A rectangle made of its corner and its size. |
| `bool dgx_ui_rect_empty(const dgx_ui_rect_t *r)` | Whether it holds no pixel. |
| `bool dgx_ui_rect_intersect(const dgx_ui_rect_t *a, const dgx_ui_rect_t *b, dgx_ui_rect_t *out)` | The pixels two rectangles have in common, in `out`, which may be `a` or `b`; `false` and all zeros when there are none. |
| `int dgx_ui_rect_subtract(const dgx_ui_rect_t *box, const dgx_ui_rect_t *cut, dgx_ui_rect_t out[4])` | The parts of `box` outside `cut`: up to four rectangles that do not overlap, the band above first. It returns how many: 0 when the box is empty or covered. `x + w` and `y + h` of the box do not pass 32767. |

### The state

```c
typedef struct dgx_ui_state_t {
    uint8_t bits;
    int16_t value;
} dgx_ui_state_t;
```

| Bit | Meaning |
| --- | ------- |
| `DGX_UI_HIDDEN` | not shown, takes no place in its row or column, not hit, not focused |
| `DGX_UI_DISABLED` | shown as its look draws it so, not hit, not focused |
| `DGX_UI_FOCUSED` | the keys go to it |
| `DGX_UI_PRESSED` | pressed, by a finger on it or by the program |

`value` is the value of a control, the row of a list, the picture of an icon.

### A widget

```c
typedef struct dgx_ui_widget_t {
    uint8_t        kind;
    uint8_t        flags;
    uint8_t        pad, gap;
    int16_t        at_x, at_y;
    int16_t        want_w, want_h;
    int16_t        min, max, step;
    int16_t        tag;
    dgx_ui_state_t state;
    const dgx_ui_picture_t *picture;
    dgx_ui_textbox_t       *text;
    const dgx_ui_look_t    *look;
    bool (*take)(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg);
    void (*on)(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);
    void *ctx;
    void *own;
} dgx_ui_widget_t;
```

What the program says of a widget. The library never writes it; it is 52 bytes on a 32-bit chip. Zero is the default
of every member.

| Member | Meaning |
| ------ | ------- |
| `kind` | what it is: see the kinds below |
| `flags` | how it is built: see the flags below |
| `pad`, `gap` | of a box: points from its edges to its children, and between them |
| `at_x`, `at_y` | where it asks to be, in points from the inner edge of its parent; negative: from the far edge. In a column only `at_x` is used, in a row only `at_y` |
| `want_w`, `want_h` | its size in points; 0: the size of its content, or across the stacking all of the box |
| `min`, `max`, `step` | the range of a control and its step; the first and the last row of a list, and in `step` the least height of its rows in points, 0 a line of its text; the values of an icon's pictures |
| `tag` | the program's number of it, found by `dgx_ui_find()`; 0 for none |
| `state` | the state it starts with: `DGX_UI_HIDDEN`, `DGX_UI_DISABLED`, the first value |
| `picture` | a picture: one; an icon: an array for the values `min..max` |
| `text` | its text box, in RAM |
| `look` | how it is drawn; `NULL`: the look of its kind, which a box, a frame and a label have. Any other widget with no look is not drawn, and the painter logs it once |
| `take` | called with every message offered to the tree; `true` claims the message |
| `on` | told what the user did to it: see the events below. It gets the widget's node as it is now: `node->state_value.value` is the value the user set |
| `ctx` | given to `on()` |
| `own` | the data of a widget of the program's own kind |

**Kinds**

| Kind | What it is |
| ---- | ---------- |
| `DGX_UI_BOX` | holds children: stacks them as a column or a row, else they sit where they ask |
| `DGX_UI_FRAME` | a box with a fill and a border |
| `DGX_UI_LABEL` | a text |
| `DGX_UI_PICTURE` | one picture |
| `DGX_UI_ICON` | a picture picked by the value from an array; outside `min..max`, none |
| `DGX_UI_LIST` | rows, one picked: the value, `min` the first row. With more rows than it takes, it shows a window of them and scrolls. Its look is `dgx_ui_look_list` |
| `DGX_UI_SWITCH` | value 0 or 1 |
| `DGX_UI_SLIDER` | value `min..max` by `step` |
| `DGX_UI_BUTTON` | clicked: activated |
| `DGX_UI_INPUT` | a field typed into |
| `DGX_UI_DROPDOWN` | a button that shows the option chosen: its options the lines of its text, its value the one chosen. Activated, the program opens its list by `dgx_ui_dropdown_open()` |
| `DGX_UI_OWN` | drawn by a look of the program's, its data in `own`; worked by the finger and the keys when its look names a behaviour |

**Flags**

| Flag | Meaning |
| ---- | ------- |
| `DGX_UI_COLUMN` | a box: its children one under another |
| `DGX_UI_ROW` | a box: its children side by side |
| `DGX_UI_STRETCH` | as wide as its column, or as high as its row, whatever its own size |
| `DGX_UI_SWIPE` | a slider moved by the way the finger goes from where it went down, its width the whole range; a tap on it activates it |
| `DGX_UI_DRAG` | a button dragged up or down as well as clicked, as the knob of a scroll bar: `on()` is told `DGX_UI_CHANGED` at every move |
| `DGX_UI_REST` | along the stacking of a column or a row it takes all the box has left after the other children, the gaps and the pads; several share it equally; never less than 0 |
| `DGX_UI_CANVAS` | the widget has a canvas of its own, a picture kept from pass to pass: `dgx_ui_canvas()` |

**Events**: what `on()` is told in `what`.

| Event | When |
| ----- | ---- |
| `DGX_UI_ACTIVATED` | a button, a row of a list or a dropdown was clicked or activated by a key; "done" was typed into a field |
| `DGX_UI_CHANGED` | the user changed the value: by the finger, a key, or a letter typed |
| `DGX_UI_FOCUS` | the focus came to it |
| `DGX_UI_BACK` | "cancel" was typed into a field |
| `DGX_UI_NO_MEMORY` | the canvas of the widget could not be had: told once for that size, after the pass; it is tried again whenever its look asks |
| `DGX_UI_EDIT` | a field was clicked, or activated by a key while it has the focus: the program opens its editor, a keyboard |

`on()` is told only what the user did, never what the program set. It may change and remove widgets, the one it is
called for too.

### A picture

```c
typedef struct dgx_ui_picture_t {
    const dgx_image_t *image;
    const uint16_t    *rgb565;
    const uint8_t     *map;
    int16_t            width, height;
} dgx_ui_picture_t;
```

A picture of DGX in `image`, drawn by `dgx_ui_look_picture` when it is of 1 bit; or rows of RGB565, high byte first, in
`rgb565` with their `width` and `height`, drawn by `dgx_ui_look_rgb565`. A picture of 4 or 8 bits in `image` is drawn
by `dgx_ui_look_indexed`: its values go through `map` to indices of the painter's table, the value 1 the entry
`map[0]`; without a map the value is the index; a pixel 0 is the paper.

`void dgx_ui_picture_size(const dgx_ui_picture_t *p, int *w, int *h)` gives the size of either.

### A node

```c
typedef struct dgx_ui_node_t {
    const dgx_ui_widget_t *widget;
    int16_t                parent;
    int16_t                child, next;
    int16_t                x, y, w, h;
    dgx_ui_state_t         state_value;
    dgx_ui_state_t         state_drawn;
    uint8_t                marks;
} dgx_ui_node_t;
```

A widget in a tree, 28 bytes of RAM on a 32-bit chip. The program reads it and does not write it.

| Member | Meaning |
| ------ | ------- |
| `widget` | what the program said of it; `NULL`: a free slot |
| `parent`, `child`, `next` | the numbers of its parent, its first child and its next sibling; -1 for none |
| `x`, `y`, `w`, `h` | where it was laid, in pixels of the screen |
| `state_value` | its state as it is: `state_value.value` is the value of a control |
| `state_drawn` | its state as it was last drawn: the painter's |
| `marks` | the library's: see below |

| Mark | Meaning |
| ---- | ------- |
| `DGX_UI_MARK_CHANGED` | its content changed |
| `DGX_UI_MARK_WHOLE` | to be drawn whole |
| `DGX_UI_MARK_FIT` | its text is to be fitted to its field again |
| `DGX_UI_MARK_TIME` | its look asked for time: it is called again at the next pass |
| `DGX_UI_MARK_BELOW` | a widget under it is to be drawn: the painter goes down only where this is set |
| `DGX_UI_MARK_WINDOW` | a list's window is to be brought to its pick when it is drawn next: its value was set, or its height changed |
| `DGX_UI_MARK_AROUND` | a child of it moved, came or went: a box or a frame that stacks its children fills around them, and only the children marked themselves are drawn |
| `DGX_UI_MARK_HARD_SIZE` | its canvas keeps the size it has: a larger one could not be had |

### The tree

`dgx_ui_t` is a tree and what the pointer and the keys are doing to it, 96 bytes. The program fills it by
`dgx_ui_init()` and changes it by the calls only. Members a program may read: `focus`, the number of the focused
widget, and `pressed`, the one the pointer is held on, both -1 for none; `popup`, the popup open over it; `level`,
how many popups lie under it, 0 for a tree that is no popup.

`DGX_UI_BLOCK` is 64: the nodes past the program's array come in blocks of so many.

| Call | What it does |
| ---- | ------------ |
| `void dgx_ui_init(dgx_ui_t *ui, dgx_ui_node_t *first, int first_size, int unit)` | A tree in the program's array of `first_size` nodes; `NULL` and 0 when all nodes are to come in blocks. `unit` is the pixels of a point. |
| `void dgx_ui_free(dgx_ui_t *ui)` | Every widget removed and the blocks given back. |
| `int dgx_ui_add(dgx_ui_t *ui, int parent, const dgx_ui_widget_t *w)` | A widget added as the last child of `parent`; the first one added is the root, whatever `parent` is. It returns its number; -1 for no such parent or no memory for a block. The tree keeps the pointer: the widget must live as long as it is in the tree. |
| `int dgx_ui_insert(dgx_ui_t *ui, int parent, int before, const dgx_ui_widget_t *w)` | The same, before the child `before`; -1: as the last. |
| `void dgx_ui_remove(dgx_ui_t *ui, int i)` | A widget removed with everything under it. A focus or a press inside it is dropped, and nothing is told. Its number is given to a widget added later. |
| `int dgx_ui_find(const dgx_ui_t *ui, int parent, int tag)` | The first widget of a tag under `parent`, in the order of the tree; `parent` -1: in all of the tree. -1 when there is none. |
| `dgx_ui_node_t *dgx_ui_node(const dgx_ui_t *ui, int i)` | The node of a number; `NULL` past the tree. |
| `const dgx_ui_widget_t *dgx_ui_widget(const dgx_ui_t *ui, int i)` | The widget of a number; `NULL` for a number that holds none. |

### The layout

| Call | What it does |
| ---- | ------------ |
| `void dgx_ui_set_root(dgx_ui_t *ui, int x, int y, int w, int h)` | The rectangle the root is laid in, in pixels. Without it the root takes all of the screen. |
| `void dgx_ui_relayout(dgx_ui_t *ui)` | The tree is laid again before it is next drawn or hit: after the program changed a widget it keeps in its own RAM. |
| `void dgx_ui_screen(dgx_ui_t *ui, int width, int height, int turn)` | The screen the tree is shown on. The painter calls it at every pass; a program calls it in a test without a painter. Another size or `turn` lays the tree again and draws it whole. |
| `bool dgx_ui_layout(dgx_ui_t *ui)` | The tree laid now if it has to be; `true` when it was. The painter and the pointer call it. |

### Changing the state

None of these tells `on()`.

| Call | What it does |
| ---- | ------------ |
| `void dgx_ui_set_value(dgx_ui_t *ui, int i, int value)` | The value set, kept within `min..max` when `min < max`. An icon's is not bound: outside its range it shows no picture. |
| `void dgx_ui_set_state(dgx_ui_t *ui, int i, unsigned bits, bool on)` | `DGX_UI_DISABLED` or `DGX_UI_PRESSED` set or cleared. Disabled, a widget loses the focus, and a press of the finger on it ends. |
| `void dgx_ui_show(dgx_ui_t *ui, int i, bool shown)` | Shown or hidden; its row or column is laid again. Hidden, it loses the focus and a press of the finger. |
| `void dgx_ui_redraw(dgx_ui_t *ui, int i)` | Drawn whole at the next pass, with everything under it. |
| `bool dgx_ui_hidden(const dgx_ui_t *ui, int i)` | Whether it is hidden, itself or by a widget above it. |

### The pointer

```c
typedef struct dgx_ui_pointer_t {
    int16_t  x, y;
    bool     down, pressed, released;
    bool     swipe;
    uint32_t ms;
} dgx_ui_pointer_t;
```

The pointer as it is now: where, in pixels of the screen; `down` now; `pressed` and `released` since the last look;
`swipe`: a swipe was told now; `ms`: the time of the program's clock, the one given to `dgx_ui_time()`.

| Call | What it does |
| ---- | ------------ |
| `bool dgx_ui_pointer(dgx_ui_t *ui, const dgx_ui_pointer_t *pointer)` | The pointer given to the tree, or to its popup first. The widget it went down on is pressed until it goes up, wherever it goes. Let go within `DGX_UI_CLICK_MS`, not swiped and not dragged, it is a click: a button, a row of a list or a dropdown is activated, a switch turned, a field told `DGX_UI_EDIT`, a widget of the program's own told `DGX_UI_ACTIVATED` after its behaviour's `up`. A swipe that begins on a field opens no editor. A widget whose `on(DGX_UI_FOCUS)` opens a popup over it is not pressed: the finger is spent. It returns `true` when a widget took it. |
| `int dgx_ui_hit(dgx_ui_t *ui, int x, int y)` | The deepest widget under a pixel that answers a pointer; -1 for none. A widget is hit, as it is drawn, only within its parent's box. |
| `bool dgx_ui_holding(const dgx_ui_t *ui)` | Whether a widget of the tree or of its popup holds the pointer. |
| `bool dgx_ui_dragging(const dgx_ui_t *ui)` | Whether the widget that holds it is dragged: a slider, a list, a button with `DGX_UI_DRAG`, a widget of the program's own whose `move` said so. Such a widget owns the finger. |

`DGX_UI_CLICK_MS` is 500. `DGX_UI_KNOB` is 3: the width of a slider's knob in points. `DGX_UI_ROW_GAP` is 12: the top
and the bottom twelfth of a list's row belong to no row.

### Keys and the focus

| Key | What it does |
| --- | ------------ |
| `DGX_UI_KEY_NEXT`, `DGX_UI_KEY_PREV` | the focus to the next or the previous widget that takes it; in a list the rows first. The walk passes the fields and opens no editor |
| `DGX_UI_KEY_ACTIVATE` | the focused button, row or dropdown activated, a switch turned, a field told `DGX_UI_EDIT`; a widget of the program's own that did not take the key told `DGX_UI_ACTIVATED` |
| `DGX_UI_KEY_MORE`, `DGX_UI_KEY_LESS` | the focused slider a step up or down, a switch on or off |
| `DGX_UI_KEY_BACK` | taken by no widget of the core: the program's |

`DGX_UI_KEYS` is how many keys there are. A focused widget of the program's own is asked first, for every key, by
its behaviour's `key`: [A look and a behaviour](#a-look-and-a-behaviour).

| Call | What it does |
| ---- | ------------ |
| `bool dgx_ui_key(dgx_ui_t *ui, int key)` | A key given to the tree, or to its popup first; `true` when a widget took it. |
| `bool dgx_ui_buttons(dgx_ui_t *ui, uint32_t held, uint32_t now_ms)` | The buttons held now. Each that went down is its key; next, previous, more and less repeat while held. Call it at every tick with all the buttons held, also when the program itself acts on one: a button it did not see go down is a new press at the next call. `true` when a widget took a key. |
| `bool dgx_ui_time(dgx_ui_t *ui, uint32_t now_ms)` | The time step of the logic: a held key repeats when its time comes. `true` while such a key is held: the loop asks for time. |
| `void dgx_ui_focus(dgx_ui_t *ui, int i)` | The focus put on a widget, which is told `DGX_UI_FOCUS`; -1: on none. A widget that cannot take it (hidden, disabled, of the program's own without `DGX_UI_BEHAVIOR_FOCUS`): on none. |

A held key repeats `DGX_UI_REPEAT_FIRST_MS`, 500 ms, after it went down, and every `DGX_UI_REPEAT_MS`, 100 ms, after.

### Which key a button gives

`dgx_ui_buttons()` takes the buttons as bits of a game pad, the same as in DGX-DI and DGX-GE, and turns each button
into a key by a table.

| Button | Its key by default | What the key does |
| ------ | ------------------ | ----------------- |
| `DGX_UI_PAD_UP` | `DGX_UI_KEY_PREV` | the focus to the widget before |
| `DGX_UI_PAD_DOWN` | `DGX_UI_KEY_NEXT` | the focus to the widget after |
| `DGX_UI_PAD_LEFT` | `DGX_UI_KEY_LESS` | the focused slider a step down, a switch off |
| `DGX_UI_PAD_RIGHT` | `DGX_UI_KEY_MORE` | the focused slider a step up, a switch on |
| `DGX_UI_PAD_A` | `DGX_UI_KEY_ACTIVATE` | the focused button, row or dropdown activated, a switch turned, a field told `DGX_UI_EDIT` |
| `DGX_UI_PAD_BACK`, `DGX_UI_PAD_B` | `DGX_UI_KEY_BACK` | nothing in a widget: the program's |
| `DGX_UI_PAD_START` | none | the button is the program's |

The default fits a column of settings: up and down go from one to the next, left and right change the one in focus.

It does not fit buttons that stand in a row. Take a dialog with three buttons side by side, "Back", "OK" and "Next",
the focus on "Back". The user presses right to get to "OK". Right is "more"; a button has no value to make more of,
so nothing happens. The focus moves by up and down only, which nobody looking at a row would guess.

A table of the program's own gives left and right the keys that move the focus:

```c
/* three buttons in a row, "Back", "OK", "Next": left and right move the focus along the row */
static const uint8_t row_keys[8] = {
    DGX_UI_KEY_PREV,     /* DGX_UI_PAD_LEFT:  the focus to the button on the left; by default LESS */
    DGX_UI_KEY_NEXT,     /* DGX_UI_PAD_RIGHT: the focus to the button on the right; by default MORE */
    DGX_UI_KEY_PREV,     /* DGX_UI_PAD_UP:    the focus back, as by default */
    DGX_UI_KEY_NEXT,     /* DGX_UI_PAD_DOWN:  the focus on, as by default */
    DGX_UI_KEY_ACTIVATE, /* DGX_UI_PAD_A:     the focused button activated, as by default */
    0xff,                /* DGX_UI_PAD_START: no key, the button is the program's, as by default */
    DGX_UI_KEY_BACK,     /* DGX_UI_PAD_BACK:  back, which no widget takes: the program's, as by default */
    DGX_UI_KEY_BACK,     /* DGX_UI_PAD_B:     the same */
};
```

`void dgx_ui_button_keys(dgx_ui_t *ui, const uint8_t *keys)` gives the tree such a table: after
`dgx_ui_button_keys(&ui, row_keys)` right on "Back" moves the focus to "OK".

- The table has 8 entries, one for each button, in the order of the bits `DGX_UI_PAD_*`: LEFT, RIGHT, UP, DOWN, A,
  START, BACK, B.
- An entry is a `DGX_UI_KEY_*`, or 0xff: the button gives no key and stays the program's.
- The tree keeps the pointer and copies nothing: give a constant.
- It may be given again at any time, as when a dialog of a column gives way to a dialog of a row. `NULL` gives the
  default back.
- It is the table of the tree given to `dgx_ui_buttons()`. The keys it gives go to the popups over that tree as any
  key does.

### A look and a behaviour

```c
struct dgx_ui_look_t {
    bool (*draw)(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
    const void              *params;
    int16_t                  fill;
    const dgx_ui_behavior_t *behavior;
};

typedef struct dgx_ui_behavior_t {
    uint8_t flags;
    void (*down)(dgx_ui_t *ui, int i, int x, int y);
    bool (*move)(dgx_ui_t *ui, int i, int x, int y);
    void (*up)(dgx_ui_t *ui, int i, int x, int y, bool click);
    bool (*key)(dgx_ui_t *ui, int i, int key);
} dgx_ui_behavior_t;
```

A look is how a widget is drawn. `draw` draws the widget `i` in its state and returns `true` to be called again at
the next pass; `params` is the look's own type; `fill` is the index of the colour it leaves under the widget's
children, -1: what lay there. `behavior` is the behaviour of a widget of `DGX_UI_OWN` drawn by this look; `NULL`:
the widget is only drawn. The looks of the library name none. The rules a `draw` keeps are in
[A look of your own](cookbook.md#a-look-of-your-own).

A behaviour is how a widget of `DGX_UI_OWN` answers the finger and the keys. With it the core finds the widget
under the finger, presses it and tells a click, as it does for its own kinds; without it the widget is only drawn.
A behaviour named by the look of a widget of another kind is not called. Every function may be `NULL`. `x` and `y`
are in the widget's own pixels, from its top left corner; while the finger is held they may lie outside the widget.

| Member | When it is called |
| ------ | ----------------- |
| `flags` | `DGX_UI_BEHAVIOR_FOCUS`: the widget takes the focus, and the keys with it. Without it the widget is of the finger alone: a press on it leaves the focus where it was, the keys pass it, and `dgx_ui_focus()` on it leaves nobody focused |
| `down` | the finger went down on the widget; `DGX_UI_PRESSED` is set and the focus given by then |
| `move` | at every look of the pointer while the widget holds the finger. `true`: a drag, so there is no click when it is let go, and `dgx_ui_dragging()` is true: a swipe from an edge is not the program's |
| `up` | the finger let go; `click`: not dragged, not swiped, within `DGX_UI_CLICK_MS`. After a click the core tells `on()` `DGX_UI_ACTIVATED`, so `on()` sees a value `up` has set |
| `key` | a key while the widget has the focus; it is asked first, for every key; `true`: taken. What it does not take is the core's: next and previous move the focus on, `DGX_UI_KEY_ACTIVATE` tells `DGX_UI_ACTIVATED` (with no `up`), back is the program's |

| Call | What it does |
| ---- | ------------ |
| `void dgx_ui_user_value(dgx_ui_t *ui, int i, int value)` | The value of a widget set as the user sets it: within `min`..`max`, the widget marked, `on()` told `DGX_UI_CHANGED` when it changed and nothing when it is the same. For a behaviour's code, and for any widget: a program's test acts as the user by it. |
| `void dgx_ui_tell(dgx_ui_t *ui, int i, int what)` | An event told to a widget's `on()`, the node as it is now. For a behaviour whose data in `own` changed, and for any widget. |

Code points typed go to a `DGX_UI_INPUT` only. The widget is hit in its whole rectangle. The recipe:
[A widget of your own that the finger and the keys work](cookbook.md#a-widget-of-your-own-that-the-finger-and-the-keys-work).

### A popup

| Call | What it does |
| ---- | ------------ |
| `bool dgx_ui_popup_open(dgx_ui_t *ui, dgx_ui_t *popup, int x, int y, int w, int h, unsigned flags)` | A tree opened over another in a rectangle of the screen, in pixels; a popup that tree already had open is closed first. `ui` may itself be a popup: popups stand one over another. `false`, and nothing opened: the popup is the tree itself, it has a popup of its own open or is open elsewhere, or `DGX_UI_POPUP_DEPTH` popups already lie under the tree. |
| `void dgx_ui_popup_close(dgx_ui_t *ui)` | The popup of that tree closed, with every popup over it; the trees under are drawn again there at the next pass. |

| Flag | Meaning |
| ---- | ------- |
| `DGX_UI_POPUP_MODAL` | it takes every pointer and key; back stays the program's |
| `DGX_UI_POPUP_CLOSE_OUTSIDE` | a finger that goes down outside it, and outside every popup over it, closes it |

`DGX_UI_POPUP_DEPTH` is 8: at most so many popups stand one over another over a tree.

### Other

`int dgx_ui_textbox_height(const dgx_ui_textbox_t *box)`: the height a text box asks for, its lines times the line of
its first font. The layout calls it.

`void dgx_ui_dump(const dgx_ui_t *ui)`: the tree written to the log, a line a widget, indented by its depth: its
number, kind, place and size, value, state, marks, tag and the beginning of its text; then the popups over it; then
how many nodes are in use, how many the program's array holds and how many blocks of 64 nodes, 1792 bytes each, were
taken past it, with the size of an array that needs none. A file of its own: a program that does not call it does
not carry it. It changes nothing.

## dgx_ui_text.h

### Fonts and styles

```c
typedef struct dgx_ui_font_variant_t {
    dgx_font_t *font;
    int16_t     height;
    uint8_t     narrow;
} dgx_ui_font_variant_t;

typedef struct dgx_ui_text_style_t {
    const dgx_ui_font_variant_t *variants;
    uint8_t                      count;
    uint8_t                      flags;
    dgx_font_align_t             align;
    uint16_t                     scroll_ms;
} dgx_ui_text_style_t;
```

A variant is a font a text may be drawn in: a font of DGX of bitmaps or of dots. `height` and `narrow` are for
handwritten fonts, which are not drawn yet; leave them 0.

A style lists the variants, the best first, with their `count`. `align` is `DGX_FONT_ALIGN_LEFT`, `_CENTER` or
`_RIGHT` of DGX. `scroll_ms` is how long an important text shows a line before it scrolls up; 0: it is scrolled by
hand.

| Flag | Meaning |
| ---- | ------- |
| `DGX_UI_TEXT_MIDDLE` | a text in a smaller variant stands in the middle of the height, not on the base line of the first variant |
| `DGX_UI_TEXT_IMPORTANT` | a text too long for its lines scrolls rather than being cut; for `dgx_ui_look_lines` |

`int dgx_ui_variant_line(const dgx_ui_font_variant_t *v)`: the height of a line of a variant.

### The text box

```c
struct dgx_ui_textbox_t {
    char                      *buf;
    uint16_t                   size;
    uint8_t                    lines;
    uint8_t                    variant;
    const dgx_ui_text_style_t *style;
    uint16_t                   top;
    uint8_t                    letter_ms;
    uint8_t                    flags;
    uint32_t                   since_ms;
    uint16_t                   letters_drawn;
    uint16_t                   total_lines, total_letters;
};
```

The string of a widget and how it is laid, 28 bytes. The program gives `buf`, `size`, `lines` and `style` through
`dgx_ui_textbox_init()` and may read `buf`. The rest is the library's: the variant the text was fitted in, the first
line shown, and what the look of lines counts. Its flags `DGX_UI_TEXTBOX_START`, `DGX_UI_TEXTBOX_CAME`,
`DGX_UI_TEXTBOX_ENDED` and `DGX_UI_TEXTBOX_COUNTED` are the library's too.

| Call | What it does |
| ---- | ------------ |
| `void dgx_ui_textbox_init(dgx_ui_textbox_t *box, char *buf, int size, int lines, const dgx_ui_text_style_t *style)` | A text box over the program's buffer of `size` bytes, empty, of so many lines. A text set is cut to `size - 1` bytes. |
| `bool dgx_ui_text_printf(dgx_ui_t *ui, int i, const char *format, ...)` | The text of a widget set as by `printf()`, cut to its buffer on a whole letter of UTF-8. `true` when it changed. With a buffer of up to 256 bytes the same text changes nothing; a longer buffer is taken as changed every time. |
| `bool dgx_ui_text_vprintf(dgx_ui_t *ui, int i, const char *format, va_list args)` | The same with a `va_list`. |
| `bool dgx_ui_text_set(dgx_ui_t *ui, int i, const char *text)` | The text set as it is. |
| `void dgx_ui_text_appear(dgx_ui_t *ui, int i, int letter_ms)` | The text appears letter by letter from the next pass, a letter every so many ms; 0: all at once. A text set later appears too. For `dgx_ui_look_lines`. |
| `void dgx_ui_text_scroll(dgx_ui_t *ui, int i, int lines)` | A text that is not important moved so many lines up, or down when negative, as far as there are lines. For `dgx_ui_look_lines`. Of a list that scrolls: its window moved so many rows, within its rows, the pick staying; the next change of the value brings the window back to the pick. |

Setting a text draws and measures nothing: the painter fits and draws it at its next pass.

## dgx_ui_msg.h

```c
struct dgx_ui_msg_t {
    int     what;
    uint8_t flags;
    uint8_t count;
    union {
        uint8_t bytes[DGX_UI_MSG_BYTES];
        struct {
            void  *data;
            size_t size;
            void (*done)(void *data, size_t size);
        } ref;
    };
};
```

A message, 20 bytes on a 32-bit chip: the item of the program's queue. `what` is the program's number. By value,
`count` bytes lie in `bytes`, up to `DGX_UI_MSG_BYTES`, 12. By reference, `flags` has `DGX_UI_MSG_REF` and `ref` holds
the data, its size, and the function called when the message is done with. `DGX_UI_MSG_KEPT` marks a message a widget
kept.

| Call | What it does |
| ---- | ------------ |
| `bool dgx_ui_msg_value(dgx_ui_msg_t *msg, int what, const void *data, size_t size)` | A message by value filled; `false` when the data is larger than 12 bytes. |
| `void dgx_ui_msg_ref(dgx_ui_msg_t *msg, int what, void *data, size_t size, void (*done)(void *, size_t))` | A message by reference filled. |
| `bool dgx_ui_take(dgx_ui_t *ui, dgx_ui_msg_t *msg)` | The message offered to every widget of the tree in its order, the hidden ones too, then to those of its popup, until one claims it. It returns `true` when one did. Nothing is released. |
| `void dgx_ui_msg_done(dgx_ui_msg_t *msg)` | The message done with: `done()` is called, once, for data by reference. Nothing for a message by value or one a widget kept. The loop calls it once for every message. |
| `void dgx_ui_msg_keep(dgx_ui_t *ui, dgx_ui_msg_t *copy)` | Called in a widget's `take()`: the data stays the widget's after the walk. The message is copied into `copy`, which the widget gives to `dgx_ui_msg_done()` when it is done. Keeping claims the message. A second call, or one outside a `take()`, gives a message of no data. A message offered to a second tree after a widget of the first kept it is offered there as any other, read and not kept, and the loop's `dgx_ui_msg_done()` still does nothing. |
| `esp_err_t dgx_ui_msg_copy(const dgx_ui_msg_t *msg, int what, void *out, size_t size)` | The data of a message of the number `what` copied out. `ESP_OK`; `ESP_ERR_NOT_FOUND` for another number; `ESP_ERR_INVALID_SIZE` for this number with data of another size, which is logged. |
| `const void *dgx_ui_msg_data(const dgx_ui_msg_t *msg, size_t *size)` | The data of a message and its size, where they lie; valid until the message is done with. |
| `bool dgx_ui_typed(dgx_ui_t *ui, const uint32_t *code_points, int count)` | Code points typed, to the focused field of the popup or of the tree. `true` when a field took them. |

`DGX_UI_DECODER(name, number, type)` defines `esp_err_t msg_decode_<name>(const dgx_ui_msg_t *msg, type *out)`: the
data of a message of that number, of the size of that type, copied out.

The code points of editing, as the keyboard of DGX-DI types them: `DGX_UI_CP_ERASE` (U+0008) takes the last letter
off, `DGX_UI_CP_DONE` (U+000A) tells the field `DGX_UI_ACTIVATED`, `DGX_UI_CP_CANCEL` (U+001B) tells it
`DGX_UI_BACK`.

## dgx_ui_post.h

Sending, through a queue of FreeRTOS the program makes with items of `sizeof(dgx_ui_msg_t)`. No call waits: when the
queue is full the message is lost and the call returns `false`.

| Call | What it does |
| ---- | ------------ |
| `bool dgx_ui_post(QueueHandle_t q, int what, const void *data, size_t size)` | A message by value, up to 12 bytes, from any task. |
| `bool dgx_ui_post_ref(QueueHandle_t q, int what, void *data, size_t size, void (*done)(void *data, size_t size))` | A message by reference. The message owns the data until `done()` is called, in the task that owns the tree. When it is not sent, `done()` is not called and the data is still the sender's. |
| `bool dgx_ui_post_const(QueueHandle_t q, int what, const void *data, size_t size)` | A message by reference to data that lives for ever. |
| `bool dgx_ui_post_from_isr(QueueHandle_t q, int what, const void *data, size_t size, BaseType_t *woken)` | A message by value from an interrupt; `*woken` as `xQueueSendFromISR()` sets it. It lies in IRAM. |

`DGX_UI_MESSAGE(name, number, type)` ties a type to a number: it defines
`bool msg_post_<name>(QueueHandle_t q, const type *value)` and the decoder `msg_decode_<name>()`. A type larger than
12 bytes does not compile.

## dgx_ui_ticker.h

```c
typedef struct dgx_ui_ticker_t {
    void            *timer;
    QueueHandle_t    queue;
    int              what;
    volatile uint8_t on_its_way;
} dgx_ui_ticker_t;
```

A message of a number every so many milliseconds, at most one of them in the queue: the next is sent only after
`dgx_ui_msg_done()` of the one before. On a chip the timer is an `esp_timer`; on the linux target of ESP-IDF, a timer
of FreeRTOS.

| Call | What it does |
| ---- | ------------ |
| `bool dgx_ui_ticker_start(dgx_ui_ticker_t *ticker, QueueHandle_t queue, int what, uint32_t period_ms)` | Ticks from now on. The struct is zeroed before the first start; a static one is. `false`: no timer could be made, or the ticker runs already. |
| `void dgx_ui_ticker_stop(dgx_ui_ticker_t *ticker)` | No more ticks. The struct must live until `on_its_way` is 0 after this: a tick still in the queue writes into it when it is done with. |

## dgx_ui_paint.h

### The painter

```c
typedef struct dgx_ui_paint_t {
    dgx_screen_t   *scr;
    const uint32_t *colors;
    uint16_t        color_count;
    uint8_t         background;
    dgx_ui_background_draw_t         background_draw;
    void           *background_ctx;
    const dgx_ui_drawn_background_t *drawn;
    void           *buffer;
    size_t          buffer_size;
    const dgx_ui_strips_t *strips;
    dgx_ui_rect_t   cover;
    int16_t         ox, oy;
    /* the painter's */
    uint8_t         paper;
    bool            on_background;
    bool            whole;
    bool            composing;
    uint8_t         part, parts;
    const dgx_ui_t *popup_over;
    dgx_screen_t   *rgb565;
    dgx_screen_t   *indexed;
    uint16_t       *lut;
    dgx_screen_t   *strip;
    size_t          buffer_used;
    uint32_t        now_ms;
    uint32_t        wake_ms;
    bool            wake_named;
    dgx_ui_canvases_t *canvases;
    dgx_ui_rect_t   cover_was;
} dgx_ui_paint_t;
```

How a tree is drawn on a screen. The program fills the first members and keeps the struct from pass to pass.

| Member | Meaning |
| ------ | ------- |
| `scr` | the screen of DGX |
| `colors`, `color_count` | the table of colours: an index of a look to a colour of the screen, as `dgx_screen_rgb()` packs it |
| `background` | the index of the colour under the tree. With a drawn background, the colour a look is told where it cannot lay the picture: give the picture's main colour |
| `background_draw`, `background_ctx`, `drawn` | the program's picture under the tree; set by `dgx_ui_background_set()`, not by hand |
| `buffer`, `buffer_size` | memory of the program's that the painter draws in: for a look that needs it, a picture of 4 or 8 bits, a strip of its rows at a time; and for the strips. Start it on a word boundary, in memory the bus sends by DMA on a chip. `NULL`: none |
| `strips` | `&dgx_ui_strips`: the painter draws through the buffer in strips. Name it for a display behind a bus of 18 or 24 bits, and for one of 16 bits with a drawn background. `NULL`: straight |
| `cover` | a rectangle not drawn into: the band of DGX-DI's keyboard while it is open; empty otherwise |
| `ox`, `oy` | the pixel of the tree's screen that lies at (0, 0) of `scr`; 0, or the place of a buffer when composing |

The members after them are the painter's. A look reads these while it draws: `paper`, the index of the colour
under the widget; `on_background`, what lies under the widget is the program's picture; `whole`, nothing of the widget is on the screen as it should be; `composing`, `dgx_ui_paint_area()`
is drawing and nothing is to be remembered; `part` and `parts`, which part of the widget's box this call is for, of
how many.

| Call | What it does |
| ---- | ------------ |
| `bool dgx_ui_paint(dgx_ui_t *ui, dgx_ui_paint_t *paint, uint32_t now_ms)` | The tree laid if it has to be, then what changed drawn, in one batch, and its popup over it. The clip of the screen is given back as it was. `true` when a widget asks for more time. |
| `void dgx_ui_paint_area(dgx_ui_t *ui, dgx_ui_paint_t *paint, int x, int y, int w, int h)` | A rectangle of the tree's screen composed from the root down: the background, then every widget that meets it, whole, under a clip. Nothing is marked as drawn. Pixels may be written more than once. The looks are given the time of the last `dgx_ui_paint()` with this paint, its `now_ms`; a program that composes with a paint that `dgx_ui_paint()` never draws sets `now_ms` first. |
| `void dgx_ui_paint_area_to(dgx_ui_t *ui, dgx_ui_paint_t *paint, dgx_screen_t *scr, const uint32_t *colors, int x, int y, int w, int h)` | The same rectangle composed onto another screen, its corner at that screen's (0, 0): a strip of a mirror of the display. `colors` is the table packed for that screen, `NULL` the paint's own. The paint's screen, colours and place are put back after; its canvases stay its own. An inline of the header. |
| `void dgx_ui_paint_free(dgx_ui_paint_t *paint)` | What the painter made for itself given back: the screen objects of the looks of RGB565 and of indexed pictures and of the strips, and the table of the indexed pictures. The canvases of the widgets too. The struct may be used again. |
| `uint32_t dgx_ui_color(const dgx_ui_paint_t *paint, int index)` | A colour of the table; 0 past its end. |
| `void dgx_ui_fill_around(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, dgx_ui_rect_t area, uint32_t color)` | For a look of the program's own that holds children: an area of the widget `i`, in pixels of the tree's screen, filled with a colour everywhere but under its shown children that a look draws. In a column or a row; a widget of neither has the area filled whole. Called from a look's `draw`. |

### A drawn background

```c
typedef void (*dgx_ui_background_draw_t)(struct dgx_ui_paint_t *paint, int x, int y, int w, int h, void *ctx);
```

The program's picture under the tree: a function that puts the rectangle `x, y, w, h` of the tree's screen on
`paint->scr` at `x - paint->ox`, `y - paint->oy`. The painter has set the clip of the screen to the rectangle. The
screen is the display, or a strip of the painter's over the buffer, of the display's depth. `ctx` is what the program
gave with the function.

| Call | What it does |
| ---- | ------------ |
| `void dgx_ui_background_set(dgx_ui_paint_t *paint, dgx_ui_background_draw_t draw, void *ctx)` | Gives the paint the function and what it is called with; `NULL` and `NULL` take the picture away. It sets `background_draw`, `background_ctx` and `drawn`. An inline of the header: only a program that calls it carries the laying of a picture, so a function written into `background_draw` by hand is not called. After the picture was taken away, `dgx_ui_redraw(ui, 0)` draws the tree on the colour again. |
| `void dgx_ui_background_changed(dgx_ui_t *ui, dgx_ui_paint_t *paint, int x, int y, int w, int h)` | The picture changed in a rectangle of the tree's screen: it is laid there at once where it shows, and the widgets on it are drawn again over it. Nothing is drawn within a widget whose look fills, under a popup, or under the keyboard's band. Nothing is kept for the next pass. Called by the task that owns the screen; inside a batch of DGX several calls go out together. Without a drawn background it does nothing. |
| `void dgx_ui_lay_paper(dgx_ui_paint_t *paint, uint8_t paper, int x, int y, int w, int h)` | For a look: its paper laid on a rectangle of the tree's screen. `paper` is an index of the table, or `DGX_UI_PAPER`, what lies under the widget: the colour `paint->paper`, or, when `paint->on_background`, the program's picture. |

`dgx_ui_drawn_background_t` is opaque, and `extern const dgx_ui_drawn_background_t dgx_ui_drawn_background;` is the
library's laying of a picture; `dgx_ui_background_set()` names it, the program does not.

- The picture shows wherever the paper of a look is what lies under the widget and no widget above it fills: between
  the letters of a label, around the face of a button, in a box, outside the root. A frame fills, so it and all in it
  show its fill.
- On a display behind a bus name the strips as well, of 16 bits too: a widget over the picture is then put together
  in the buffer and sent once. Drawn straight it is written twice, and the soft edges of its letters mix with the
  colour `background`, not with the picture.
- The recipe is [A picture under the widgets](cookbook.md#a-picture-under-the-widgets).

### Time, as "when"

`dgx_ui_paint()` returns `true` while a widget asks for time. When the time is wanted is told beside it.

| Call | What it does |
| ---- | ------------ |
| `void dgx_ui_wake_in(dgx_ui_paint_t *paint, uint32_t ms)` | For a look that returns `true`: it asks to be called again no sooner than in `ms`. A look that asks for time and names nothing is called at the next pass. A look names its moment again at every pass it is drawn. |
| `uint32_t dgx_ui_wake(const dgx_ui_paint_t *paint, const dgx_ui_t *ui, uint32_t now_ms)` | For the loop: how long it may wait before the next pass, the least of the moments the looks named at the last `dgx_ui_paint()` and of the next repeat of a key held. 0: the next pass, and the loop waits a tick, not nothing. `DGX_UI_WAKE_NONE`: nobody asks, the loop waits for a message. |

`wake_ms` of the paint is the least of the moments named at the last pass, from its `now_ms`; `wake_named` is the
painter's own. Both calls are inlines of the header. The recipe is
[A loop that sleeps till the next moment](cookbook.md#a-loop-that-sleeps-till-the-next-moment).

### A canvas

A widget with the flag `DGX_UI_CANVAS` has a canvas: a virtual screen of DGX of 8 bits, as large as its box, that the
paint keeps from pass to pass, for a look that draws its picture on and on. `dgx_ui_canvases_t` is opaque: the
canvases a paint made.

| Call | What it does |
| ---- | ------------ |
| `dgx_screen_t *dgx_ui_canvas(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, bool *made)` | For a look: the canvas of widget `i`, made at its first need after the layout, all zeros. `made` is set: `true` when the canvas is new, or new in another size, and holds nothing. `NULL`: no canvas to be had, or the widget has no `DGX_UI_CANVAS` or no size. |
| `void dgx_ui_canvas_put(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, int ink)` | For a look: the canvas put on the screen at the top left of the widget's box, the rest of the box its paper. `ink` -1: the values of the canvas are indices of the painter's table, 0 the paper. `ink` an index of the table: the values are amounts of that ink over the paper, 0 none and 255 all. With no canvas, or on a screen of other than 16 bits, the box is paper. |

- A look draws in the canvas what the time and the state say, so that called twice in a pass it draws the same; it
  is called for every part of its box, and each call puts the canvas. Drawn whole with a kept canvas, it puts the
  canvas and draws nothing again.
- When the box changes its size a new canvas is made, then the old one freed. If no new one can be had the old one
  is kept for good, the widget marked `DGX_UI_MARK_HARD_SIZE`, and it is put cut to the box.
- With no canvas at all the widget is told `DGX_UI_NO_MEMORY` at the end of the next `dgx_ui_paint()`, once for
  that size, and the canvas is tried again whenever the look asks.
- The canvases are the paint's: a tree drawn by another paint has others. A canvas is freed when its widget leaves
  the tree, when its tree is not drawn by the paint's `dgx_ui_paint()` (a popup closed), and by
  `dgx_ui_paint_free()`. So one paint draws one tree and the popups over it.
- On a drawn background a 0 of the canvas leaves the picture, and amounts of ink mix with it where the screen can
  be read, in a strip.
- It needs `CONFIG_DGX_ENABLE_VSCREEN`. The recipe is
  [A picture that stays between passes](cookbook.md#a-picture-that-stays-between-passes).

### Drawing through the buffer

`extern const dgx_ui_strips_t dgx_ui_strips;` is the painter's way of drawing through the buffer in strips; the
program names it in `strips` of the paint. `dgx_ui_strips_t` is opaque.

A display behind a bus of 18 or 24 bits fills the field of a text before it writes the letters, so a text drawn
straight blinks there at every change. With the strips named and a buffer given, every widget without children is
drawn whole in a virtual screen of the display's depth laid over the buffer, a strip of as many rows as the buffer
takes, and each strip is sent once: every pixel once.

- A widget that changed is sent whole, all of its box.
- The buffer holds a row of the widget at least, its width times 3 bytes; a widget too wide for it is drawn straight,
  told once in the log. A widget with children is drawn straight too: it fills around them.
- A 16-bit display draws straight, but for a widget on a drawn background, which goes through the strips at any
  depth. A virtual screen draws straight whatever is named.
- It needs `CONFIG_DGX_ENABLE_VSCREEN`: named without it, the program does not link. A program that does not name it
  carries none of it.

### A look

The struct of a look, `dgx_ui_look_t`, is declared in `dgx_ui.h`, so that the core sees the behaviour a look may
name: [A look and a behaviour](#a-look-and-a-behaviour). The rules its `draw` keeps are in
[A look of your own](cookbook.md#a-look-of-your-own).

`DGX_UI_PAPER`, 0xff, as the paper of a look's parameters means the colour under the widget.

### The looks of the library

Their colours are indices of the table: by default 0 the paper, 1 the ink, 2 a fill, 3 a border.

| Look | For | Parameters |
| ---- | --- | ---------- |
| `dgx_ui_look_box` | a box: its paper, when drawn whole. The look of `DGX_UI_BOX` when none is named | none |
| `dgx_ui_look_frame` | a frame: a border and a fill. The look of `DGX_UI_FRAME` when none is named | `dgx_ui_frame_look_t` |
| `dgx_ui_look_text` | a text of one line, cut with an ellipsis when too long. The look of `DGX_UI_LABEL` when none is named | `dgx_ui_text_look_t` |
| `dgx_ui_look_lines` | a text of several lines: laid word by word, cut or scrolled, its letters appearing | `dgx_ui_text_look_t` |
| `dgx_ui_look_button` | a button or a dropdown: a face over a shadow, its text in the middle | `dgx_ui_button_look_t` |
| `dgx_ui_look_switch` | a switch: a track with a knob at one end | `dgx_ui_switch_look_t` |
| `dgx_ui_look_slider` | a slider: a track filled up to a knob | `dgx_ui_slider_look_t` |
| `dgx_ui_look_list` | a list: its rows the lines of its text, the row picked on a bar | `dgx_ui_list_look_t` |
| `dgx_ui_look_dropdown` | a dropdown: a flat face with the option chosen and an arrow pointing down | `dgx_ui_dropdown_look_t` |
| `dgx_ui_look_input` | a field: its text in one line, at the top or with `DGX_UI_TEXT_MIDDLE` in the middle of its height, and, focused, a caret after it | `dgx_ui_input_look_t` |
| `dgx_ui_look_picture` | a picture or an icon of 1 bit, in the middle of the box | `dgx_ui_picture_look_t` |
| `dgx_ui_look_rgb565` | a picture or an icon of RGB565. It needs `CONFIG_DGX_ENABLE_VSCREEN` and a 16-bit screen | `dgx_ui_picture_look_t` |
| `dgx_ui_look_indexed` | a picture or an icon of 4 or 8 bits, its values through its map indices of the table; one of 1 bit too. It needs `CONFIG_DGX_ENABLE_VSCREEN`, the paint's buffer of a row of the picture at least, and a 16-bit screen; without them the widget is paper | `dgx_ui_picture_look_t` |

A look that no widget names is not linked into the program.

### The drawing functions

The drawing of each look of the library has a name of its own, so that a look in the program's own colours is a
constant: `static const dgx_ui_look_t mine = {.draw = dgx_ui_draw_button, .params = &my_colours, .fill = -1};`. All
have the signature of `dgx_ui_look_t::draw`, and each takes the parameters of its look's type.

| Function | The look it draws | Parameters |
| -------- | ----------------- | ---------- |
| `dgx_ui_draw_box` | `dgx_ui_look_box` | none; its paper is what lies under it |
| `dgx_ui_draw_frame` | `dgx_ui_look_frame` | `dgx_ui_frame_look_t`; its fill is the look's `fill` |
| `dgx_ui_draw_text` | `dgx_ui_look_text` | `dgx_ui_text_look_t` |
| `dgx_ui_draw_lines` | `dgx_ui_look_lines` | `dgx_ui_text_look_t` |
| `dgx_ui_draw_button` | `dgx_ui_look_button` | `dgx_ui_button_look_t` |
| `dgx_ui_draw_input` | `dgx_ui_look_input` | `dgx_ui_input_look_t` |
| `dgx_ui_draw_switch` | `dgx_ui_look_switch` | `dgx_ui_switch_look_t` |
| `dgx_ui_draw_slider` | `dgx_ui_look_slider` | `dgx_ui_slider_look_t` |
| `dgx_ui_draw_list` | `dgx_ui_look_list` | `dgx_ui_list_look_t` |
| `dgx_ui_draw_dropdown` | `dgx_ui_look_dropdown` | `dgx_ui_dropdown_look_t` |
| `dgx_ui_draw_picture` | `dgx_ui_look_picture` | `dgx_ui_picture_look_t` |
| `dgx_ui_draw_rgb565` | `dgx_ui_look_rgb565` | `dgx_ui_picture_look_t` |
| `dgx_ui_draw_indexed` | `dgx_ui_look_indexed` | `dgx_ui_picture_look_t` |

A look of the program's with `dgx_ui_draw_box` or `dgx_ui_draw_frame` fills around its children as the library's box
and frame do.

```c
typedef struct dgx_ui_frame_look_t {
    uint8_t border; /* the colour of the border; the fill is the look's fill */
    uint8_t width;  /* of the border, in points */
} dgx_ui_frame_look_t;

typedef struct dgx_ui_text_look_t {
    uint8_t ink;   /* the letters */
    uint8_t paper; /* under them; DGX_UI_PAPER: what lies under the widget */
} dgx_ui_text_look_t;

typedef struct dgx_ui_button_look_t {
    uint8_t face, pressed, focused, disabled; /* the face in each state */
    uint8_t text, text_disabled;              /* the text */
    uint8_t shadow;                           /* right of the face and below it */
    uint8_t depth;                            /* of the shadow, in points; 0: a flat button */
} dgx_ui_button_look_t;

typedef struct dgx_ui_switch_look_t {
    uint8_t off, on, knob, focused; /* the track off and on, the knob, the knob when focused */
    uint8_t disabled;               /* the knob when disabled */
} dgx_ui_switch_look_t;

typedef struct dgx_ui_slider_look_t {
    uint8_t track, fill, knob, focused; /* the track, its filled part, the knob, the knob when focused */
    uint8_t disabled;                   /* the knob when disabled */
    uint8_t track_h;                    /* the height of the track, in points */
} dgx_ui_slider_look_t;

typedef struct dgx_ui_picture_look_t {
    uint8_t ink;   /* the pixels of a picture of 1 bit */
    uint8_t paper; /* the rest; DGX_UI_PAPER: what lies under the widget */
} dgx_ui_picture_look_t;

typedef struct dgx_ui_input_look_t {
    uint8_t ink, caret; /* the text, and the mark where the next letter goes */
    uint8_t paper;      /* under them; DGX_UI_PAPER: what lies under the widget */
    bool    secret;     /* a star for each letter */
} dgx_ui_input_look_t;

typedef struct dgx_ui_list_look_t {
    uint8_t ink, paper;     /* a row's text and what is under it; DGX_UI_PAPER: what lies under the widget */
    uint8_t pick, pick_ink; /* the bar of the row picked, and its text */
    uint8_t focused;        /* the bar while the list has the focus */
    uint8_t disabled;       /* the text of every row while the list is disabled */
    uint8_t bar;            /* a list that scrolls: the mark on its right edge of where its window is */
} dgx_ui_list_look_t;

typedef struct dgx_ui_dropdown_look_t {
    uint8_t face, pressed, focused, disabled; /* the face in each state */
    uint8_t text, text_disabled;              /* the option chosen */
    uint8_t arrow;                            /* the arrow; disabled, it is text_disabled */
} dgx_ui_dropdown_look_t;
```

The rows of a list and the options of a dropdown are the lines of the widget's text, parted by `\n`:
`"Auto\nHeat\nCool"`. The first line is the value `min`, the next `min + 1`. A dropdown shows the line of its value;
a value with no line shows an empty face.

A list whose rows all go into it, at a line of its text or its `step` each, whichever is larger, shows them all, and
they share its height. A list of more rows shows a window of them: as many as go into its height, the first one
kept in `top` of its text box, and a bar on its right edge, two points wide, marks where the window is. The window
follows the pick. With all its rows shown a finger picks the row under it; on a list that scrolls a finger put
down picks nothing, moved up or down it moves the pick a row for each row's height, up going on down the list, and
let go without a move it clicks the row under it.

By default the bar of a list is colour 3 and the text on it colour 0; while the list has the focus the bar is colour
1, so the pick shows in a frame too, whose fill is colour 2. The rows of a disabled list are colour 2, and the mark
of a window, `bar`, colour 3.

A field whose parameters say `secret` shows a star for each letter, a password or a PIN; the text box keeps the
letters. The font of the field must have the star, `*`: without it the field shows nothing. A font made at build time
gets it when `*` is listed among the characters of the need.

## dgx_ui_dropdown.h

The list of a dropdown, opened as a popup. It is a source file of its own: a program that calls nothing of it
carries none of it.

```c
typedef struct dgx_ui_dropdown_t {
    dgx_ui_t        tree;     /* the popup: a frame holding the list */
    dgx_ui_node_t   nodes[2];
    dgx_ui_widget_t frame, list;
    dgx_ui_t       *ui;       /* the tree of the dropdown whose list it shows */
    int16_t         i;        /* that dropdown */
} dgx_ui_dropdown_t;
```

The popup of a list, in the program's RAM: 264 bytes on a 32-bit chip, zeroed before its first use, as a `static`
is. One list is open at a time, so one serves every dropdown of the program.

| Call | What it does |
| ---- | ------------ |
| `bool dgx_ui_dropdown_open(dgx_ui_t *ui, int i, dgx_ui_dropdown_t *dd, const dgx_ui_look_t *frame_look, const dgx_ui_look_t *list_look)` | Opens the list of dropdown `i` as a popup over its tree, which may be a popup itself. The looks are those of the popup's frame, its border one point, and of its list; `NULL` is the library's. `false`, nothing opened: `i` is no dropdown, `dd` shows a list already, the tree does not know its screen yet (the painter tells it, or `dgx_ui_screen()`), or the popup is refused. |

The program calls it when the dropdown's `on()` is told `DGX_UI_ACTIVATED`.

- The list stands under the dropdown, as wide as it; above it when the screen has no room below; else moved up so
  that it fits the screen, lying over the dropdown.
- Its rows are as high as the dropdown, never lower than a line of the text. When neither the room under the
  dropdown nor the room above it takes all the options the list scrolls: it stands in the larger of the two, as many
  rows as go into it, its window at the option chosen, and does not lie over the dropdown.
- The popup is modal and closed by a finger outside it. The keys stay in the list: next and previous move the pick,
  the key that activates chooses. "Back" is the program's, which closes the list by `dgx_ui_popup_close()`.
- A row chosen sets the dropdown's value, tells its `on()` `DGX_UI_CHANGED` and closes the list. The option it had,
  chosen again, closes the list and tells nothing. Closed any other way, the value stays and nothing is told.
- The list shows the dropdown's own text. While it is open the program does not change the options and does not
  remove the dropdown.

## dgx_ui_di.h

For a program that has [DGX-DI](https://github.com/jef-sure/esp32-component-dgx-di): the header is empty without it.
All of it is `static inline`; dgx-ui does not depend on DGX-DI.

```c
typedef struct dgx_ui_di_gestures_t {
    uint8_t edge;       /* DGX_DI_EDGE_*: swiped from that edge */
    uint8_t swipe;      /* DGX_DI_SWIPE_*: swiped that way */
    bool    long_press; /* held long */
    bool    taken;      /* a widget took the finger: it went down on one */
} dgx_ui_di_gestures_t;
```

The gestures of a touch that are the program's.

| Call | What it does |
| ---- | ------------ |
| `void dgx_ui_di_pointer(dgx_ui_t *ui, const dgx_di_pointer_t *pointer, dgx_di_touch_t *touch, uint32_t now_ms, dgx_ui_di_gestures_t *g)` | The first finger of a pointer already read, and left by the keyboard and the pad of DGX-DI, taken into the touch and given to the tree. |
| `void dgx_ui_di_touch(dgx_ui_t *ui, const dgx_di_touch_t *touch, uint32_t now_ms, dgx_ui_di_gestures_t *g)` | A touch the program took itself by `dgx_di_touch_take()`, given to the tree. |

While a widget holds the finger, `swipe` and `long_press` are not told. `edge` is told unless the widget that holds
the finger is dragged.
