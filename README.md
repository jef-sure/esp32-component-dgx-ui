# dgx-ui

*[По-русски: README-ru.md](README-ru.md)*

dgx-ui is a light layer of widgets on [DGX](https://github.com/jef-sure/esp32-component-dgx), the display library for
ESP32. Widgets stand in rows and columns, their values come as messages from other tasks, and the display is written
only where something changed. It needs no memory for pixels: a widget is 28 bytes of RAM and 56 bytes of flash, and it
draws straight on the display.

![A value in a frame, a clock and a note under it](docs/images/first.png)
![A settings screen: a slider, two switches, an icon, two buttons](docs/images/settings.png)

The pictures of these pages are drawn by dgx-ui itself, on a virtual screen of DGX on a computer.

```c
enum { MSG_CO2 = 1 };
typedef struct { int16_t ppm; } co2_t;
DGX_UI_MESSAGE(co2, MSG_CO2, co2_t) /* gives msg_post_co2() and msg_decode_co2() */

/* the label catches its own value from the messages */
static bool co2_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    co2_t c;
    if (msg_decode_co2(msg, &c) == ESP_OK) dgx_ui_text_printf(ui, i, "CO2 %d ppm", c.ppm);
    return false;
}

static dgx_ui_textbox_t      co2_box; /* the text: its buffer and its style are given at the start */
static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 4};
static const dgx_ui_widget_t co2_w  = {.kind = DGX_UI_LABEL, .text = &co2_box, .take = co2_take};

/* any task: a value sent and forgotten */
void sensor_task(void *queue)
{
    for (;;) {
        const co2_t c = {.ppm = read_co2()};
        msg_post_co2(queue, &c);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* the task that owns the display: asleep while nothing changes */
void ui_loop(dgx_ui_t *ui, dgx_ui_paint_t *paint, QueueHandle_t queue)
{
    for (TickType_t wait = 0;;) {
        dgx_ui_msg_t msg;
        while (xQueueReceive(queue, &msg, wait) == pdTRUE) {
            wait = 0;
            dgx_ui_take(ui, &msg); /* every widget is offered the message */
            dgx_ui_msg_done(&msg);
        }
        const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        dgx_ui_time(ui, now);
        dgx_ui_paint(ui, paint, now); /* draws only what changed */
        /* till a message comes, or till the moment a widget asks for: a tick at least */
        const uint32_t ms = dgx_ui_wake(paint, ui, now);
        wait              = ms == DGX_UI_WAKE_NONE ? portMAX_DELAY : pdMS_TO_TICKS(ms) > 0 ? pdMS_TO_TICKS(ms) : 1;
    }
}
```

## Status

Version 0.0.35: a tag of the repository, not in the ESP Component Registry yet. It is being built, and its API may
still change. What it has is listed under [What is in it](#what-is-in-it), what it lacks under
[Not there yet](#not-there-yet).

## Quick start

In `main/idf_component.yml` of the project:

```yaml
dependencies:
  dgx-ui:
    git: https://github.com/jef-sure/esp32-component-dgx-ui.git
    version: "0.0.35"
```

It needs ESP-IDF 5.2 or later and DGX 0.11.6 or a later 0.11; DGX comes by itself, from the registry. Start with
[Getting started](docs/en/getting-started.md): a program that shows a value sent by another task, and a clock.

## Documentation

| Page | What it holds |
| ---- | ------------- |
| [Getting started](docs/en/getting-started.md) | adding the component, the first program piece by piece, building for a chip and for the computer |
| [How it works](docs/en/how-it-works.md) | the tree, the layout, state and events, messages, time, the painter and the looks, memory |
| [Cookbook](docs/en/cookbook.md) | recipes: a value from a task, a clock, buttons, a slider, a list, a dropdown, an icon, texts of several lines, a handwritten text, a popup, keys, touch, a field and a keyboard, a look of your own, a control of your own, the tree as a layer in another compositor's strips |
| [API reference](docs/en/api.md) | every type, function and constant, by header |

Examples, each for a CYD board and for a window on the computer: [thin_paint](examples/thin_paint/main/thin_paint.c),
a value sent by a task and a clock; [controls](examples/controls/main/controls.c), a button with a popup, a switch, a
dropdown, a slider and two fields, a code and a PIN shown as stars, each edited in a strip over a keypad of DGX-DI;
worked by a finger and by the keys, on a board taken from
[boards](https://github.com/jef-sure/esp32-component-boards). The list of changes is in [CHANGES.md](CHANGES.md).

## What is in it

- **A tree of widgets** in an array the program gives, and in blocks of 64 past it. What the program says of a
  widget is constant and may lie in flash; the tree keeps 28 bytes of RAM a widget.
- **Rows and columns**: a box stacks its children as a column or a row, with a padding and a gap, in points that are
  so many pixels; a child may take all that is left. The layout is made again only when the screen, the widgets or
  their sizes change, and then only what moved is drawn.
- **Messages**: a number of the program's and up to 12 bytes by value, or data by reference. Other tasks send them
  through a queue of FreeRTOS, an interrupt too; every widget is offered a message and catches its own values.
  A ticker sends one every so many milliseconds and keeps at most one of them in the queue.
- **A painter that writes only what changed**, in one batch of DGX, each widget under a clip of its box. A value
  drawn again writes every pixel of its field once, so it does not blink on a display behind a bus; on one of 18 or
  24 bits it is drawn through a buffer of the program's for that.
- **Texts**: one line, or several laid word by word; the font picked from a list so that the text goes in; a text
  too long cut with an ellipsis or scrolled; letters that appear one by one. A text in a handwritten font of DGX,
  of one line or several, written by a pen stroke by stroke.
- **Controls**: a button with a shadow and a pressed face, a switch, a checkbox, radio buttons, a slider, a list, a dropdown
  with its list, a
  field typed into, pictures and icons of 1, 4 and 8 bits and of RGB565. A control names its look, so a program
  carries only the looks it uses.
- **Controls of the program's own**: a widget drawn by the program's look becomes a control when the look names a
  behaviour, what the finger and the keys do with it; the core does the hit, the press, the focus and the click.
- **A picture for the background**: a function of the program's lays it, and labels, buttons and pictures show it
  around their letters and faces; where it changes, the program says so and only that is drawn again.
- **A canvas of a widget's own**: a picture that stays from pass to pass, for a look that only adds to it, a graph
  or digits that morph.
- **Popups** over the tree, one over another: modal, or closed by a finger outside. Pages, each the whole screen,
  one over another, closed by the key back; on a board with the memory, coming by a slide or a crossfade.
- **Input**: a pointer, the keys of a dialog, the buttons of a game pad with a repeat of a held one, code points
  typed; the touch of [DGX-DI](https://github.com/jef-sure/esp32-component-dgx-di) through a header, if the program
  has it.
- **Time only when asked**: a widget that animates asks for time and says when; an idle interface is a sleeping
  task, and one that scrolls a text sleeps between its steps.
- **A layer in another compositor**: for a program that sends the display its own picture, a game, a map, a
  video, the tree tells the rectangles that changed and draws nothing, and is composed into the program's own strips.
- **Tests on the computer**: the logic draws nothing and calls no function of DGX, so it is tested without a display.

## Not there yet

- a builder of interfaces on the computer.

An interface of tables, graphs and long texts is a task for
[DGX-LVGL](https://github.com/jef-sure/esp32-component-dgx-lvgl).

## How light

Measured on an ESP32 with ESP-IDF 5.5.4, the smallest program, one label fed by a message:

| | |
| --- | --- |
| code of dgx-ui in the program | about 8 KB |
| RAM of dgx-ui itself | none; 4 bytes once the program's loop calls `dgx_ui_time()` |
| a widget | 28 bytes of RAM and 56 bytes of flash |
| a message | 20 bytes |
| the keys and the focus, when the program gives the tree keys or buttons | about 1.5 KB more |
| the look of several lines, when a widget names it | about 2.5 KB more |
| handwriting, when a variant of a font names it | about 1.6 KB more, and 28 KB of DGX with its font |
| handwriting in several lines, when a widget names its look | about 3.9 KB more |
| drawing through the buffer for a display of 18 or 24 bits, when the program names it | about 0.6 KB more, and 5 KB of DGX |
| a picture for the background, when the program sets one | about 0.5 KB more |
| a canvas, when a widget has one | about 1.5 KB more, and a byte of RAM a pixel of its box |
| `dgx_ui_changed()`, when the program calls it | about 0.6 KB more |
| transitions between pages, when the program names them | about 2 KB more, and two pictures of the screen in RAM |
| the statistics of painting, when the program names them | about 1.8 KB more |
| a checkbox or radio buttons, when a widget names the look | about 1.5 KB more; both, 1.7 KB |

## License

MIT, see [LICENSE](LICENSE).
