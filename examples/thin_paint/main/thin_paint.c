/*
 * thin_paint: a frame with a CO2 value a sensor task sends every 200 ms, and a clock a ticker wakes every second,
 * and the build number. The task that owns the display sleeps on its queue; the painter draws only the fields that
 * change. On the computer, in a window of DGX the size of a CYD lying down; on a CYD, its ILI9341 standing
 * (240x320), as DGX's graphicstest sets it.
 */
#include <stdio.h>

#include "dgx_ui_paint.h"
#include "dgx_ui_post.h"
#include "dgx_ui_text.h"
#include "dgx_ui_ticker.h"
#include "esp_timer.h"
#include "fonts/DejaVuSansBook32.h"
#include "fonts/TerminusTTFMedium12.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BUILD 3

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
    dgx_bus_protocols_t *bus = dgx_spi_init(SPI2_HOST, SPI_DMA_CH_AUTO, GPIO_NUM_13, GPIO_NUM_12, GPIO_NUM_14, GPIO_NUM_15, GPIO_NUM_2, 40 * 1000 * 1000, 0);
    dgx_screen_t        *scr = bus ? dgx_ili9341_init(bus, GPIO_NUM_NC, GPIO_NUM_21, 16, DgxScreenRGB) : NULL;
    if (scr) dgx_screen_orientation(scr, DgxScreenLeftRight, DgxScreenTopBottom, false); /* upright, 240x320 */
    return scr;
}
#endif

/* the messages: the program's numbers, and the data of each */
enum { MSG_CO2 = 1, MSG_SECOND = 2 };
typedef struct {
    int16_t ppm;
} co2_t;
DGX_UI_MESSAGE(co2, MSG_CO2, co2_t)

/* the texts: their buffers and boxes in RAM, their styles and widgets constant */
static char                  co2_buf[24], clock_buf[16], build_buf[24];
static dgx_ui_textbox_t      co2_box, clock_box, build_box;
static dgx_ui_font_variant_t vbig[2], vsmall[1]; /* the fonts are got by functions: filled at the start */
static dgx_ui_text_style_t   st_big, st_left, st_right;

/* each widget catches its own value from the messages */
static bool co2_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    co2_t c;
    if (msg_decode_co2(msg, &c) == ESP_OK) dgx_ui_text_printf(ui, i, "CO2 %d ppm", c.ppm);
    return false;
}

static bool clock_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    if (msg->what != MSG_SECOND) return false;
    const uint32_t s = (uint32_t)(esp_timer_get_time() / 1000000);
    dgx_ui_text_printf(ui, i, "%02u:%02u:%02u", (unsigned)(s / 3600 % 24), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
    return true; /* nobody else wants the second */
}

static const dgx_ui_widget_t root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 4, .gap = 4};
static const dgx_ui_widget_t frame_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 3, .want_h = 40};
static const dgx_ui_widget_t co2_w   = {.kind = DGX_UI_LABEL, .text = &co2_box, .want_h = 34, .take = co2_take};
static const dgx_ui_widget_t row_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 10};
static const dgx_ui_widget_t clock_w = {.kind = DGX_UI_LABEL, .text = &clock_box, .want_w = 50, .take = clock_take};
static const dgx_ui_widget_t build_w = {.kind = DGX_UI_LABEL, .text = &build_box, .want_w = 60};

static dgx_ui_node_t   nodes[8];
static dgx_ui_t        ui;
static QueueHandle_t   queue;
static dgx_ui_ticker_t second;

/* a sensor of its own task: a value every 200 ms, sent and forgotten */
static void sensor_task(void *arg)
{
    for (int k = 0;; ++k) {
        const co2_t c = {.ppm = (int16_t)(400 + (k * 37) % 900)};
        msg_post_co2(queue, &c);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void app_main(void)
{
    dgx_screen_t *scr = display_init();
    if (!scr) return;
    const uint32_t colors[] = {
        dgx_screen_rgb(scr, 16, 20, 28),    /* 0 the paper */
        dgx_screen_rgb(scr, 230, 230, 220), /* 1 the ink */
        dgx_screen_rgb(scr, 32, 48, 72),    /* 2 the frame's fill */
        dgx_screen_rgb(scr, 120, 160, 220), /* 3 its border */
    };
    vbig[0] = (dgx_ui_font_variant_t){.font = DejaVuSansBook32()}, vbig[1] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    vsmall[0] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    st_big    = (dgx_ui_text_style_t){.variants = vbig, .count = 2, .align = DGX_FONT_ALIGN_CENTER, .flags = DGX_UI_TEXT_MIDDLE};
    st_left   = (dgx_ui_text_style_t){.variants = vsmall, .count = 1, .align = DGX_FONT_ALIGN_LEFT};
    st_right  = (dgx_ui_text_style_t){.variants = vsmall, .count = 1, .align = DGX_FONT_ALIGN_RIGHT};
    dgx_ui_textbox_init(&co2_box, co2_buf, sizeof co2_buf, 1, &st_big);
    dgx_ui_textbox_init(&clock_box, clock_buf, sizeof clock_buf, 1, &st_left);
    dgx_ui_textbox_init(&build_box, build_buf, sizeof build_buf, 1, &st_right);

    dgx_ui_init(&ui, nodes, 8, 2);
    dgx_ui_add(&ui, -1, &root_w);
    const int frame = dgx_ui_add(&ui, 0, &frame_w);
    dgx_ui_add(&ui, frame, &co2_w);
    const int row = dgx_ui_add(&ui, 0, &row_w);
    dgx_ui_add(&ui, row, &clock_w);
    const int build = dgx_ui_add(&ui, row, &build_w);
    dgx_ui_text_printf(&ui, build, "dgx-ui build %d", BUILD);

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
                /* no widget took it: the program's */
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
