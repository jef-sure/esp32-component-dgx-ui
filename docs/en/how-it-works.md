# How it works

*[По-русски](../ru/how-it-works.md)*

This page explains how dgx-ui works: what parts it is made of, and what rules each part keeps. Read it when the
first program runs and you want to know why it is written the way it is. The [Cookbook](cookbook.md) shows the same
things as code, and the [API reference](api.md) lists every call.

## The parts

| Header | What it is | Calls DGX |
| ------ | ---------- | --------- |
| `dgx_ui.h` | the core: the tree, the layout, the state of widgets, the pointer, the keys, the focus, a popup | no |
| `dgx_ui_text.h` | the text box: a string in the program's buffer, a style, the fonts | no, it reads the sizes of a font |
| `dgx_ui_msg.h` | messages: offering one to the tree, decoders, code points typed | no |
| `dgx_ui_post.h` | sending a message through a queue of FreeRTOS, from an interrupt too | no |
| `dgx_ui_ticker.h` | a message every so many milliseconds | no |
| `dgx_ui_paint.h` | the painter and the looks: this is all that draws | yes |
| `dgx_ui_di.h` | the touch of DGX-DI as the pointer of a tree; only when the program has DGX-DI | no |

The core, the text box and the messages draw nothing, so they are tested on a computer without a display.

dgx-ui has no task, no queue and no lock of its own. The task, its queue, the first array of nodes and the buffers of
the texts are the program's. The library allocates only blocks of nodes when the program's array is full, and one
small screen object when a program shows pictures of RGB565.

## One task owns the tree

One task calls everything: it owns the tree as it owns the screen of DGX. No call of dgx-ui may come from another
task, except the sending of a message: `dgx_ui_post()` and its kin.

So a sensor, the network, a key and a timer do not touch widgets. Each sends a message, and the owner's loop offers
it to the widgets:

```c
void owner_loop(dgx_ui_t *ui, dgx_ui_paint_t *paint, QueueHandle_t queue)
{
    for (bool busy = false;;) {
        dgx_ui_msg_t msg;
        TickType_t   wait = busy ? 1 : portMAX_DELAY;
        while (xQueueReceive(queue, &msg, wait) == pdTRUE) {
            wait = 0;
            dgx_ui_take(ui, &msg);
            dgx_ui_msg_done(&msg);
        }
        const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        busy               = dgx_ui_time(ui, now);
        busy |= dgx_ui_paint(ui, paint, now);
    }
}
```

## A widget is two parts

What the program says of a widget is a `dgx_ui_widget_t`: its kind, its sizes, its range, its text box and picture,
its look, its two functions. The library never writes it. It is 52 bytes on a 32-bit chip and lies in flash when it is
`static const`.

What changes while the program runs is a `dgx_ui_node_t`, 28 bytes of RAM: a pointer to the widget, the links to its
parent, first child and next sibling, the rectangle it was laid in, and its state.

`dgx_ui_add()` keeps the pointer to the widget and copies nothing. So the widget must live as long as it is in the
tree: a `static const` object, or memory the program owns. A compound literal made inside a function dies with the
function, and the tree would point into its dead stack.

A widget is known by its number: `dgx_ui_add()` returns it, and every call takes `(ui, i)`. The slot of a widget
removed is given to the next one added, so a number kept by the program may come to name another widget. A widget
that comes and goes is better found by its tag, `dgx_ui_find()`.

The first nodes are in an array the program gives to `dgx_ui_init()`. When it is full, the library allocates a block of
64 nodes, 1792 bytes, and another when that is full. Nodes do not move.

## The layout

Sizes and places are in points. A point is `unit` pixels, given to `dgx_ui_init()`: the same tree fits a display of
another density with another unit.

A box (`DGX_UI_BOX`, and a frame, which is a box with a fill and a border) lays its children:

- with `DGX_UI_COLUMN` one under another, with `DGX_UI_ROW` side by side, `gap` points apart and `pad` points from
  its edges;
- with neither, each child where it asks: `at_x`, `at_y` points from the box's inner edge, or from the far edge when
  negative.

The size of a child is `want_w`, `want_h`. Where one is 0:

| Widget | Width | Height |
| ------ | ----- | ------ |
| a picture, an icon | the picture's | the picture's |
| a widget with a text box | none of its own | its lines times the line of its first font |
| a box, a frame, a control without a text | none of its own | none of its own |

A size the widget has none of is taken across the stacking: the width of the column, or the height of the row.
Along the stacking it stays 0, so a box in a column needs `want_h`, and a label in a row needs `want_w`, or the
flag `DGX_UI_REST`. The flag
`DGX_UI_STRETCH` makes a child as wide as its column or as high as its row whatever its own size.

A child with the flag `DGX_UI_REST` takes, along the stacking, all the box has left after the other children, the
gaps and the pads; several such children share it equally. A row of a name and a value, or a column of a title, a
content and buttons at the bottom, then needs no exact sizes and fits a screen of any width.

A hidden widget takes no place. A widget is drawn and hit only within its parent's box: what of it lies outside,
because it is laid larger than the box or put out of it by `at_x` and `at_y`, is cut off.

The tree is laid again:

1. before it is first drawn;
2. when the screen changed its size or its orientation: the painter compares them at every pass;
3. when a widget was added, removed, shown or hidden;
4. when the program asks: `dgx_ui_relayout()`, after it changed a widget it keeps in its own RAM.

A change only marks the tree. The layout runs once, before the next drawing or the next hit of the pointer. A widget
that moved, came or changed its size is drawn whole. Its parent fills only what its children do not cover, and the
children that stayed where they were are not drawn again. After a turn of the screen everything is drawn.

## State and events

The state of a widget is four bits and a value: hidden, disabled, focused, pressed, and the value of a control. A node
keeps it twice: as it is, `state_value`, and as it was last drawn, `state_drawn`.

Anybody may change the state through the calls: the pointer, a key, the program, a message.
`dgx_ui_set_value()`, `dgx_ui_set_state()`, `dgx_ui_show()` and `dgx_ui_text_printf()` compare the new with the old and
change nothing when they are equal.

A widget's `on()` is told only what the user did: `DGX_UI_ACTIVATED`, `DGX_UI_CHANGED`, `DGX_UI_FOCUS`, `DGX_UI_BACK`,
`DGX_UI_EDIT`. No call of the program tells it anything. Take a thermostat: its set point comes by a message from the
network, and `on(DGX_UI_CHANGED)` sends the user's new set point to the network. If the message's value told `on()`
too, the program would send it back to where it came from.

For the same reason a button the program set to pressed is drawn pressed and activates nothing: a hardware "OK"
held down may show on the screen's "OK" without pressing it.

## Messages

A message is a number of the program's and data: up to 12 bytes by value, or a pointer and a size by reference. It is
20 bytes, the item of the program's queue. dgx-ui reserves no number and gives none a meaning.

`dgx_ui_take()` offers a message to every widget in the order of the tree, parents before children, the hidden ones
too, so that they are fresh when shown; then to the widgets of an open popup. A widget's `take()` returns `true` to
claim the message, and the walk stops there. `dgx_ui_take()` returns whether a widget claimed it; an unclaimed
message is the program's.

Return `true` only for a message meant for one widget. Take a CO2 value shown twice, as a large number and as a small
graph: if the number's `take()` returned `true`, the graph would never get the value. A value several widgets may
show is read and left unclaimed: `false`.

Data by reference belongs to the message from the sending until `dgx_ui_msg_done()`, which calls the sender's `done()`
once, in the owner's task. The loop calls `dgx_ui_msg_done()` once for every message. A widget that needs the data
after the walk calls `dgx_ui_msg_keep()` in its `take()`: it gets a copy of the message, the loop's
`dgx_ui_msg_done()` then does nothing, and the widget calls it on its copy when it is done.

Sending never waits. When the queue is full the message is lost and the call returns `false`; for data by reference
`done()` is not called then, and the data is still the sender's.

A ticker is a timer that sends a message every so many milliseconds. It sends the next only after the one before was
done with, so however long the owner is busy, at most one tick lies in the queue.

## Time

Nothing runs by itself. A look that animates returns `true` from its drawing, `dgx_ui_paint()` returns `true`
then, and the loop comes back after one tick of FreeRTOS. A look works out where its animation stands from the
time it is given, not from the number of calls, so a pass missed does not stretch it.

`dgx_ui_time()` is the time step of the logic: a held key repeats there. It returns `true` while a key is held.

Slow things that change with time, a clock or the frames of an icon, are better driven by a ticker's message than by
asking for time: then the task sleeps between them.

## The painter

`dgx_ui_paint()` walks the tree, parents before children, in one batch of DGX. It goes down only into the parts of
the tree where something changed, so a pass over a large tree with one new value costs little. A widget is drawn when:

- its state differs from the state as drawn;
- its content changed: a text, a picture's value;
- it is to be drawn whole: the first time, after it moved, when its parent is drawn whole, when the program asked by
  `dgx_ui_redraw()`;
- its look asked for time.

For each such widget the clip of the screen is set to its box, and its look draws. When the pass ends, the screen has
the clip it had before.

**The paper.** A widget is told the colour that lies under it: the fill of the nearest widget above it that has one
(a frame), or the background. A label drawn on a frame fills its field with the frame's fill by itself. The
background is a colour, or a picture the program draws by a function of its own: then a label over it shows the
picture between its letters, and a frame still shows its fill. See
[A picture under the widgets](cookbook.md#a-picture-under-the-widgets).

**Every pixel once.** A look writes each pixel of what it draws once. A text is set in a field of DGX: the letters
and the paper around them go out in one pass, so a shorter text leaves nothing of a longer one and nothing blinks. A
picture of 1 bit is drawn opaque. A box or a frame that stacks its children fills only what they do not cover: its
pads, its gaps and what is left after the last child. So a widget hidden, shown or added does not blink either. This
holds for a 16-bit display behind a bus. A box whose children sit where they ask may have them overlap: it is filled
whole, and its children draw over it.

**Displays of 18 and 24 bits.** Behind a bus such a display fills the field of a text before it writes the letters, so
a text drawn straight blinks. There the program gives the painter a buffer and names `dgx_ui_strips`: every widget
without children is drawn in the buffer and sent strip by strip, every pixel once. See
[A display of 18 or 24 bits](cookbook.md#a-display-of-18-or-24-bits).

**Covers.** Rectangles may lie over a tree: the popups open over it, and the band of DGX-DI's keyboard, which the
program gives as `cover`. A widget partly under them is drawn in the parts that show, the clip set to each part in turn. One
wholly under them is not drawn and keeps its marks, so a text changed under a popup is drawn right when the popup
closes. Where a cover was and is no more, the painter composes the tree again.

**Composing.** `dgx_ui_paint_area()` draws a rectangle of the screen from the root down: the background, then every
widget that meets it, whole, under a clip. With `ox`, `oy` the screen may be a small buffer that holds a strip of the
rectangle.

## Looks

A look is how a kind of widget is drawn: a function, its parameters, and the colour it leaves under its children.
Colours are indices of a table the program gives the painter, packed for the screen by `dgx_screen_rgb()`. Another
table is the whole interface in other colours: a night mode is one assignment and a redraw of the root.

A box, a frame and a label have a look when theirs is `NULL`. A control names its look: `.look = &dgx_ui_look_button`.
A control with no look is not drawn, and the painter says so once in the log.
A look that no widget names is not in the program, so a clock of labels does not carry the button, the slider and the
text of several lines.

A look in the program's own colours is a constant of the library's drawing function and parameters of its own:
`{.draw = dgx_ui_draw_button, .params = &my_colours, .fill = -1}`.

The looks of the library draw the whole box of the widget in its state as it is, and keep no memory of their own.
A look that does remember what it drew stores it only in the last part of a pass: see
[A look of your own](cookbook.md#a-look-of-your-own).

## Texts

A text box holds the program's buffer for the string, the number of lines it shows and a style. A style lists the
variants: the fonts the text may be drawn in, the best first. The painter takes the first variant the text goes into
whole; when none does, the one that shows the most letters.

A label draws one line: the text in a field as wide as the widget, a text too long cut with an ellipsis. The look
`dgx_ui_look_lines` lays the text into the lines of the box word by word; a text too long is cut in its last line, or,
when its style says it is important, scrolls a line up every `scroll_ms`. Letters may appear one by one, each where it
will stand.

No table of lines is kept: the text is laid again when it is drawn.

## Input

**The pointer.** The program gives `dgx_ui_pointer()` where the finger is and whether it went down or up. The deepest
widget under the finger that answers a pointer is pressed at once and takes the focus. A slider follows the finger,
and so does the pick of a list; a button, a switch, a row of a list and a dropdown are clicked when the finger is let
go within `DGX_UI_CLICK_MS` and was not swiped. A disabled or hidden widget is not hit.

**Keys.** `dgx_ui_key()` takes the keys of a dialog: next and previous move the focus (in a list, the rows first),
activate presses the focused one, more and less move a slider or a switch. Back is never taken: it is the program's.
`dgx_ui_buttons()` takes the buttons held now, as bits of a game pad, and makes keys of them; a held one repeats after
500 ms, every 100 ms.

**Typing.** `dgx_ui_typed()` gives code points to the focused field: characters, erase, done and cancel, as the
keyboard of DGX-DI types them.

**Touch.** `dgx_ui_di.h` gives the first finger of DGX-DI's pointer to the tree and sorts the gestures. While a widget
holds the finger, a plain swipe and a long press are its own. A swipe from an edge is the program's, unless the widget
is dragged: a slider at the left edge is dragged to the right without opening a side panel.

## A popup

A popup is the only thing that lies over other widgets. It is a tree of its own, laid in its rectangle and drawn over
the tree, which is drawn around it. The pointer and the keys go to it first; a finger that goes down inside it is the
popup's or nobody's. A modal popup takes everything; one with `DGX_UI_POPUP_CLOSE_OUTSIDE` is closed by a finger
outside it. Messages go to every tree.

A popup is a tree, so it may have a popup of its own: a list of choices opened from a dialog, a question asked over
it. Up to `DGX_UI_POPUP_DEPTH`, 8, popups stand one over another. An upper one may hang out of the one under it; each
tree is drawn around every popup over it. The finger and the keys go to the top one first. Closing a popup closes
every popup over it. The list of a dropdown is such a popup, opened by `dgx_ui_dropdown_open()`.

## Memory and weight

| What | Size on a 32-bit chip |
| ---- | --------------------- |
| a widget, constant | 52 bytes of flash |
| its node | 28 bytes of RAM |
| a block of 64 nodes, when the program's array is full | 1792 bytes |
| a message | 20 bytes |
| a text box | 28 bytes and the program's buffer |
| a tree, `dgx_ui_t` | 96 bytes |
| the code of the smallest program, one label fed by a message | about 8 KB |
| the look of several lines, when named | about 2.5 KB |

No memory is taken for pixels: everything is drawn straight on the display. Two things go through a buffer the
program gives the painter: a picture of 4 or 8 bits, a row of the picture at least; and, when the program names the
strips, the widgets of a display of 18 or 24 bits, a row of the widest widget at least.

## Tests on the computer

The tests of the repository run on a computer with the sanitizers of the compiler: `make -C test/host`. The painter is
tested on a model of a 16-bit display behind a bus that counts how many times each pixel is written, so "every pixel
once" is a test and not a promise.

A program can test its own interface the same way: build its tree, send it messages and pointer events, and look at
the states, with no display at all.
