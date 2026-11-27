/*
 * hello: the smallest program of dgx-ui. A text that never changes, and in a frame a count another task sends once a
 * second. The task that owns the display sleeps on its queue; a new count is drawn in its own field, and nothing else
 * is. On a CYD (ESP32-2432S028R), its ILI9341 standing, 240x320; on the computer, in a window of DGX of the same size.
 * The fonts are two of those DGX ships.
 */
#include "dgx_colors.h" /* DGX_BLACK() and the other named colours */
#include "dgx_ui_paint.h"
#include "dgx_ui_post.h"
#include "dgx_ui_text.h"
#include "esp_timer.h"
#include "fonts/DejaVuSansBook32.h"
#include "fonts/LiberationSansRegular12.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* the display: dgx-ui draws on a screen of DGX and does not care which. Another display is another bus, driver and
   pins here, and their options in sdkconfig.defaults */
#ifdef DGX_HOST_WINDOW
#include "dgx_host_screen.h"
static dgx_screen_t *display_init(void)
{
    return dgx_host_screen_init(240, 320, 16, NULL);
}
#else
#include "bus/dgx_spi_esp32.h"
#include "drivers/ili9341.h"
/* CYD: MOSI 13, MISO 12, CLK 14, CS 15, DC 2, the backlight 21, SPI at 40 MHz */
static dgx_screen_t *display_init(void)
{
    dgx_bus_protocols_t *bus = dgx_spi_init(SPI2_HOST, SPI_DMA_CH_AUTO, GPIO_NUM_13, GPIO_NUM_12, GPIO_NUM_14, GPIO_NUM_15, GPIO_NUM_2, 40 * 1000 * 1000, 0);
    dgx_screen_t        *scr = bus ? dgx_ili9341_init(bus, GPIO_NUM_NC, GPIO_NUM_21, 16, DgxScreenRGB) : NULL;
    if (scr) dgx_screen_orientation(scr, DgxScreenLeftRight, DgxScreenTopBottom, false); /* upright, 240x320 */
    return scr;
}
#endif

/* the message: the program's number for it, and its data, up to 12 bytes */
enum { MSG_COUNT = 1 };
typedef struct {
    uint32_t n;
} count_t;
DGX_UI_MESSAGE(count, MSG_COUNT, count_t) /* gives msg_post_count() and msg_decode_count() */

/* the texts' styles, constant, and the box over the buffer the count is printed into; the title's box is its widget's
   own (DGX_UI_LABEL below). The count takes the large font, or the small one when it does not go into its field */
static const dgx_ui_font_variant_t vsmall[1] = {{.font = &LiberationSansRegular12}};
static const dgx_ui_font_variant_t vlarge[2] = {{.font = &DejaVuSansBook32}, {.font = &LiberationSansRegular12}};
static const dgx_ui_text_style_t   st_title  = {.fonts = vsmall, .count = 1, .align = DGX_FONT_ALIGN_LEFT};
static const dgx_ui_text_style_t   st_count  = {.fonts = vlarge, .count = 2, .align = DGX_FONT_ALIGN_CENTER, .flags = DGX_UI_STYLE_MIDDLE};
static char                        count_buf[12];
static dgx_ui_textbox_t            count_box;

/* the label catches its own value from the messages; the same count again would change nothing and draw nothing */
static bool count_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    count_t c;
    if (msg_decode_count(msg, &c) == ESP_OK) dgx_ui_text_printf(ui, i, "%u", (unsigned)c.n);
    return false; /* not claimed: a second widget could read the same message */
}

/* the widgets, constants in flash: a column of the title and a frame, and the count in the frame. Sizes are in points;
   how many pixels a point is, dgx_ui_init() says. The title, a label of a constant caption in one line, is as high as
   its font's letters with what hangs under the line, the hook of a g: 12 and 4 */
static const dgx_ui_widget_t root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 8, .gap = 8};
static const dgx_ui_widget_t title_w = DGX_UI_LABEL("Hello, dgx-ui", &st_title, .want_h = 16);
static const dgx_ui_widget_t frame_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 4, .want_h = 56};
static const dgx_ui_widget_t count_w = {.kind = DGX_UI_LABEL, .text = &count_box, .want_h = 48, .take = count_take};

/* what changes while the program runs: the tree, a node a widget, and the queue of the task that owns it */
static dgx_ui_node_t nodes[4];
static dgx_ui_t      ui;
static QueueHandle_t queue;

/* any other task: a value sent and forgotten, 0 at once and the next one every second */
static void count_task(void *arg)
{
    for (uint32_t n = 0;; ++n) {
        const count_t c = {.n = n};
        msg_post_count(queue, &c);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    dgx_screen_t *scr = display_init();
    if (!scr) return;
    /* the colours of the looks are indices of this table, packed for the screen */
#define SCREEN_RGB(r, g, b) dgx_screen_rgb(scr, (r), (g), (b))
    const uint32_t colors[] = {
        DGX_BLACK(SCREEN_RGB),          /* 0 the paper */
        DGX_GAINSBORO(SCREEN_RGB),      /* 1 the ink */
        DGX_MIDNIGHTBLUE(SCREEN_RGB),   /* 2 the frame's fill */
        DGX_CORNFLOWERBLUE(SCREEN_RGB), /* 3 its border */
    };
#undef SCREEN_RGB
    dgx_ui_textbox_init(&count_box, count_buf, sizeof count_buf, 1, &st_count);

    /* the tree: a point is a pixel here; the first widget added is the root */
    dgx_ui_init(&ui, nodes, 4, 1);
    const int root = dgx_ui_add(&ui, -1, &root_w);
    dgx_ui_add(&ui, root, &title_w);
    const int frame = dgx_ui_add(&ui, root, &frame_w);
    dgx_ui_add(&ui, frame, &count_w);

    queue = xQueueCreate(8, sizeof(dgx_ui_msg_t));
    xTaskCreate(count_task, "count", 4096, NULL, 4, NULL);

    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = 4, .background = 0};
    TickType_t     wait  = 0;
    for (;;) {
        dgx_ui_msg_t msg;
        while (xQueueReceive(queue, &msg, wait) == pdTRUE) {
            wait = 0;
            if (!dgx_ui_take(&ui, &msg)) {
                /* no widget claimed it: the program's own messages are handled here */
            }
            dgx_ui_msg_done(&msg); /* once, whatever handled it */
        }
        const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        dgx_ui_time(&ui, now);
        dgx_ui_paint(&ui, &paint, now); /* lays the tree out when it has to, and draws what changed */
        /* asleep until a message comes, or until the moment a widget asks for: a tick at least */
        const uint32_t ms = dgx_ui_wake(&paint, &ui, now);
        wait              = ms == DGX_UI_WAKE_NONE ? portMAX_DELAY : pdMS_TO_TICKS(ms) > 0 ? pdMS_TO_TICKS(ms) : 1;
    }
}
