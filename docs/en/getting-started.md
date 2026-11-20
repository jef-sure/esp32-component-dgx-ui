# Getting started

*[По-русски](../ru/getting-started.md)*

dgx-ui is a light layer of widgets on [DGX](https://github.com/jef-sure/esp32-component-dgx), the display library
for ESP32. In this guide we add it to an ESP-IDF project and write its first program, in three steps:

1. add the component to the project;
2. write a program of one file: a CO2 value that another task sends, shown in a frame, and a clock under it;
3. build it and run it, on a board or in a window on the computer.

At the end the display shows this. The value changes five times a second and the clock once a second, and between
the changes the task that owns the display sleeps:

![A value in a frame, a clock and a note under it](../images/first.png)

You need to know how an ESP-IDF project is built and flashed. Nothing of dgx-ui or of DGX is assumed: every part of
the program is explained where it appears.

## What you need

- ESP-IDF 5.2 or later.
- A display that [DGX](https://github.com/jef-sure/esp32-component-dgx) drives, or no display at all: the same program
  runs in a window on the computer.
- DGX 0.11.4 or a later 0.11. It comes by itself with dgx-ui.

## 1. Add the component

Version 0.0.31 is a tag of the repository and is not in the ESP Component Registry yet. In `main/idf_component.yml` of
the project:

```yaml
dependencies:
  dgx-ui:
    git: https://github.com/jef-sure/esp32-component-dgx-ui.git
    version: "0.0.31"
```

The component manager takes dgx-ui from GitHub at the next build, and DGX from the registry, as dgx-ui asks for it.

DGX builds only the buses and the display drivers the project turns on. For an ILI9341 on SPI, in
`sdkconfig.defaults`:

```
CONFIG_DGX_ENABLE_SPI=y
CONFIG_DGX_ENABLE_SPI_ILI_9341=y
```

The other displays and buses are in
[Getting started of DGX](https://github.com/jef-sure/esp32-component-dgx#getting-started).

## 2. Write the first program

The program below is `main/main.c`, piece by piece; the pieces in their order are the whole file. The example
[thin_paint](../../examples/thin_paint/main/thin_paint.c) of the repository is a program of the same shape, with its
display upright, 240 by 320.

```c
#include "dgx_ui_paint.h"
#include "dgx_ui_post.h"
#include "dgx_ui_text.h"
#include "dgx_ui_ticker.h"
#include "esp_timer.h"
#include "fonts/DejaVuSansBook32.h"
#include "fonts/TerminusTTFMedium12.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
```

### The display

dgx-ui draws on a screen of DGX and does not care which. Here it is the ILI9341 of a CYD board (ESP32-2432S028R), or,
when the program is built for the computer, a window of the same size:

```c
#ifdef DGX_HOST_WINDOW
#include "dgx_host_screen.h"
static dgx_screen_t *display_init(void)
{
    return dgx_host_screen_init(320, 240, 16, NULL);
}
#else
#include "bus/dgx_spi_esp32.h"
#include "drivers/ili9341.h"
/* CYD: MOSI 13, MISO 12, CLK 14, CS 15, DC 2, the backlight 21, SPI at 40 MHz */
static dgx_screen_t *display_init(void)
{
    dgx_bus_protocols_t *bus = dgx_spi_init(SPI2_HOST, SPI_DMA_CH_AUTO, GPIO_NUM_13, GPIO_NUM_12, GPIO_NUM_14, GPIO_NUM_15,
                                            GPIO_NUM_2, 40 * 1000 * 1000, 0);
    dgx_screen_t        *scr = bus ? dgx_ili9341_init(bus, GPIO_NUM_NC, GPIO_NUM_21, 16, DgxScreenRGB) : NULL;
    if (scr) dgx_screen_orientation(scr, DgxScreenRightLeft, DgxScreenTopBottom, true); /* landscape, 320x240 */
    return scr;
}
#endif
```

### The messages

A message is a number the program picks and up to 12 bytes of data. dgx-ui reserves no numbers and never looks inside
a message. `DGX_UI_MESSAGE` ties a type to a number and gives two functions: `msg_post_co2()` to send it and
`msg_decode_co2()` to read it. The second message, a second gone by, carries no data.

```c
enum { MSG_CO2 = 1, MSG_SECOND = 2 };
typedef struct {
    int16_t ppm;
} co2_t;
DGX_UI_MESSAGE(co2, MSG_CO2, co2_t)
```

### The texts

A label shows the text of a text box. The box is in RAM: it holds the program's buffer for the string and a style.
A style lists the fonts the text may be drawn in, the best first: the painter takes the first one the text goes into
whole.

```c
static char                  co2_buf[24], clock_buf[16], note_buf[24];
static dgx_ui_textbox_t      co2_box, clock_box, note_box;
static dgx_ui_font_variant_t big[2], small[1]; /* the fonts are got by functions: filled at the start */
static dgx_ui_text_style_t   st_big, st_left, st_right;
```

### What each widget does with a message

Every widget is offered every message, in the order of the tree. Its `take()` reads what it wants and returns `false`,
or returns `true` to claim the message, and then no widget after it sees it.

`dgx_ui_text_printf()` compares the new text with the old one. A value that came again changes nothing, and nothing
is drawn.

```c
static bool co2_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    co2_t c;
    if (msg_decode_co2(msg, &c) == ESP_OK) dgx_ui_text_printf(ui, i, "CO2 %d ppm", c.ppm);
    return false; /* read, not claimed: another widget may want it too */
}

static bool clock_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    if (msg->what != MSG_SECOND) return false;
    const uint32_t s = (uint32_t)(esp_timer_get_time() / 1000000); /* since the start: a real clock gives the time of day */
    dgx_ui_text_printf(ui, i, "%02u:%02u:%02u", (unsigned)(s / 3600 % 24), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
    return true; /* nobody else wants the second */
}
```

### The widgets

A widget is a constant: what kind it is, how large, what it shows, what it does. The library never writes it, so it
lies in flash. Sizes are in points; a point is so many pixels, given to `dgx_ui_init()`.

- The root is a box that stacks its children as a column, 4 points from its edges and 4 points apart.
- The frame is 40 points high and as wide as the column. It holds the label of the value.
- The row under it holds two labels: the clock, 50 points wide, and a note that takes the rest of the row.

```c
static const dgx_ui_widget_t root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 4, .gap = 4};
static const dgx_ui_widget_t frame_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 3, .want_h = 40};
static const dgx_ui_widget_t co2_w   = {.kind = DGX_UI_LABEL, .text = &co2_box, .want_h = 34, .take = co2_take};
static const dgx_ui_widget_t row_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 10};
static const dgx_ui_widget_t clock_w = {.kind = DGX_UI_LABEL, .text = &clock_box, .want_w = 50, .take = clock_take};
static const dgx_ui_widget_t note_w  = {.kind = DGX_UI_LABEL, .flags = DGX_UI_REST, .text = &note_box};
```

Three rules of the sizes to remember:

- A box has no size of its own. In a column it is as wide as the column, and its height is `want_h`.
- A label with no `want_h` is as high as its lines of text; the large number above has one. It has no width of its own: in a column it takes the column's, in a row it
  needs `want_w`.
- A child with the flag `DGX_UI_REST` takes what its row or column has left after the others. The note above is
  such a child, so the row fits a screen of any width.

### The tree, the queue and the sensor

What changes while the program runs is kept by the tree: a node of 28 bytes for each widget, in an array the program
gives. One task owns the tree and the display; every other task only sends messages.

```c
static dgx_ui_node_t   nodes[8];
static dgx_ui_t        ui;
static QueueHandle_t   queue;
static dgx_ui_ticker_t second;

/* a sensor in a task of its own: a value every 200 ms, sent and forgotten */
static void sensor_task(void *arg)
{
    for (int k = 0;; ++k) {
        const co2_t c = {.ppm = (int16_t)(400 + (k * 37) % 900)};
        msg_post_co2(queue, &c);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
```

### The start and the loop

```c
void app_main(void)
{
    dgx_screen_t *scr = display_init();
    if (!scr) return;

    /* the colours of the looks are indices of this table: 0 the paper, 1 the ink, 2 a fill, 3 a border */
    const uint32_t colors[] = {
        dgx_screen_rgb(scr, 16, 20, 28),
        dgx_screen_rgb(scr, 230, 230, 220),
        dgx_screen_rgb(scr, 32, 48, 72),
        dgx_screen_rgb(scr, 120, 160, 220),
    };

    /* the value in the large font, or in the small one when it does not go in */
    big[0]   = (dgx_ui_font_variant_t){.font = DejaVuSansBook32()};
    big[1]   = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    small[0] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    st_big   = (dgx_ui_text_style_t){.variants = big, .count = 2, .align = DGX_FONT_ALIGN_CENTER, .flags = DGX_UI_TEXT_MIDDLE};
    st_left  = (dgx_ui_text_style_t){.variants = small, .count = 1, .align = DGX_FONT_ALIGN_LEFT};
    st_right = (dgx_ui_text_style_t){.variants = small, .count = 1, .align = DGX_FONT_ALIGN_RIGHT};
    dgx_ui_textbox_init(&co2_box, co2_buf, sizeof co2_buf, 1, &st_big);
    dgx_ui_textbox_init(&clock_box, clock_buf, sizeof clock_buf, 1, &st_left);
    dgx_ui_textbox_init(&note_box, note_buf, sizeof note_buf, 1, &st_right);

    /* the tree: a point is 2 pixels; the first widget added is the root */
    dgx_ui_init(&ui, nodes, 8, 2);
    dgx_ui_add(&ui, -1, &root_w);
    const int frame = dgx_ui_add(&ui, 0, &frame_w);
    dgx_ui_add(&ui, frame, &co2_w);
    const int row = dgx_ui_add(&ui, 0, &row_w);
    dgx_ui_add(&ui, row, &clock_w);
    const int note = dgx_ui_add(&ui, row, &note_w);
    dgx_ui_text_set(&ui, note, "living room");

    /* the queue of the task that owns the display, and who sends into it */
    queue = xQueueCreate(8, sizeof(dgx_ui_msg_t));
    xTaskCreate(sensor_task, "sensor", 4096, NULL, 4, NULL);
    dgx_ui_ticker_start(&second, queue, MSG_SECOND, 1000);

    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = 4, .background = 0};
    TickType_t     wait  = 0;
    for (;;) {
        dgx_ui_msg_t msg;
        while (xQueueReceive(queue, &msg, wait) == pdTRUE) {
            wait = 0;
            if (!dgx_ui_take(&ui, &msg)) {
                /* no widget claimed it: the program's own messages are handled here */
            }
            dgx_ui_msg_done(&msg); /* once, whatever handled it: the ticker may send the next */
        }
        const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        dgx_ui_time(&ui, now);
        dgx_ui_paint(&ui, &paint, now);
        /* asleep until a message comes, or until the moment a widget or a held key asks for: a tick at least */
        const uint32_t ms = dgx_ui_wake(&paint, &ui, now);
        wait              = ms == DGX_UI_WAKE_NONE ? portMAX_DELAY : pdMS_TO_TICKS(ms) > 0 ? pdMS_TO_TICKS(ms) : 1;
    }
}
```

What the loop does:

1. It sleeps on the queue. A message wakes it; it takes every message that has come, and offers each to the widgets.
2. `dgx_ui_msg_done()` is called once for every message, whoever handled it. The ticker sends its next tick only
   after the one before was done with, so its ticks never pile up in the queue.
3. `dgx_ui_paint()` lays the tree out if it has to, and draws what changed since the last pass. Here that is the
   field of the value five times a second and the field of the clock once a second; the frame, the note and the
   paper are drawn once.
4. `dgx_ui_wake()` tells how long nothing is due. A widget that animates names the moment it wants to be drawn
   again, and a held key the moment of its repeat: the loop waits for a message at most so long. When nobody asks,
   `DGX_UI_WAKE_NONE`, it waits for a message for ever. In this program nothing asks for time, and the task sleeps.

## 3. Build and run

For the board:

```sh
idf.py set-target esp32
idf.py build flash monitor
```

On the computer, in a window of DGX, with the `linux` target of ESP-IDF. The project's `CMakeLists.txt` and
`sdkconfig.defaults` need the few lines told in
[The linux target of ESP-IDF](https://github.com/jef-sure/esp32-component-dgx/blob/main/docs/host-window-en.md#the-linux-target-of-esp-idf-host-esp-idf)
of DGX; the example [thin_paint](../../examples/thin_paint/CMakeLists.txt) has them. The program built is named
after the project, by `project()` of its `CMakeLists.txt`: `thin_paint.elf` for the example.

```sh
idf.py --preview set-target linux
idf.py build
./build/<project>.elf
```

What you should see, on the board and in the window alike: a frame with the value in large letters, `CO2 400 ppm` at
first and another number every 200 ms; under the frame, on the left, the time since the start, `00:00:01`,
`00:00:02`, and on the right the note `living room`. Only the numbers change: the frame, the note and the paper are
drawn once.

## 4. Change it

Three small changes show how the parts depend on one another. Make one, build and run again:

- In `sensor_task()`, change `pdMS_TO_TICKS(200)` to `pdMS_TO_TICKS(1000)`. The value now changes once a second.
  Nothing else in the program knows how often it comes: the label is told only when a message arrives.
- In `root_w`, change `.gap = 4` to `.gap = 12`. The row with the clock moves 8 points, 16 pixels, down from the
  frame: the column lays its children again, and no coordinate is written anywhere in the program.
- In `app_main()`, change `"living room"` to `"kitchen"`. The note is set once and never drawn again.

## Where to go next

- [How it works](how-it-works.md): what the tree, the painter and a look are, and why a value drawn again does not
  blink.
- [Cookbook](cookbook.md): buttons, sliders, icons, long texts, a popup, keys and touch.
- [API reference](api.md).
