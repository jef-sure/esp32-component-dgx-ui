/* The recipes of the cookbook run: its C blocks, as the checker joined them, with the program's own functions stubbed. */
#include "docs_en_cookbook.md.c" /* written by check_docs.py into its build directory: give it with -I */
#include "sound_day.h" /* the recipe's page made on its appearance, by check_docs.py */
#include <stdio.h>
#include "dgx_vscreen.h"
#define PANEL_BITS 24 /* the model of a display behind a bus, of test/host: it counts the writes of each pixel */
#include "panel.h"

static int saves, backs, menus, backlight = -1, wifi = -1;
void settings_save(void) { ++saves; }
static int air_reads; /* the sensor asked, by the recipe of the page that works only while it is seen */
void air_read(void) { ++air_reads; }
void backlight_set(int percent) { backlight = percent; }
void wifi_enable(bool on) { wifi = on; }
void menu_back(void) { ++backs; }
void menu_open(void) { ++menus; }
static int mode = -1, modes;
void mode_set(int m) { mode = m, ++modes; }
static int fan_speed = -1, fans;
static uint16_t mirror_all[320 * 240]; /* what mirror_send() was given, strip after strip */
static int      mirror_got;
static uint32_t table[COLORS];

/* the sliding pages: the program's big memory, there or not */
static bool  big_memory_there;
static void *big_blocks[4];
static int   big_count;
void        *big_memory(size_t bytes) { return big_memory_there && big_count < 4 ? (big_blocks[big_count++] = calloc(1, bytes)) : NULL; }

/* the layer recipe: the compositor's scene, a pattern that depends on scene_version, and what it sent */
static int      scene_version, strips_sent;
static uint16_t layer_shown[320 * 240];
void            scene_piece(dgx_screen_t *to, int to_x, int to_y, int x, int y, int w, int h)
{
    for (int row = 0; row < h; ++row)
        for (int column = 0; column < w; ++column)
            dgx_set_pixel(to, to_x + column, to_y + row, table[((x + column) / 16 + (y + row) / 16 + scene_version) % 2 ? FILL : DIM]);
}
void strip_send(const uint16_t *pixels, int top, int rows)
{
    ++strips_sent;
    memcpy(layer_shown + 320 * top, pixels, sizeof(uint16_t) * 320 * (size_t)rows);
}
/* the pad of two axes: what the program is told */
static int point_calls, point_x, point_y;
void       point_set(int x, int y) { ++point_calls, point_x = x, point_y = y; }
void mirror_send(const uint16_t *pixels, int count)
{
    if (mirror_got + count <= 320 * 240) memcpy(mirror_all + mirror_got, pixels, (size_t)count * 2), mirror_got += count;
}
void fan_set(int s) { fan_speed = s, ++fans; }
/* the radio group's recipe: what the program was told */
static int air_mode_told = -1, air_calls, beep_told = -1, beep_calls;
void       air_mode_set(int mode) { air_mode_told = mode, ++air_calls; }
void       beep_enable(bool on) { beep_told = on, ++beep_calls; }
void wifi_connect(const char *password) { (void)password; }
const uint16_t logo_pixels[64 * 32];
const uint16_t map_pixels[320 * 240]  = {[0 ... 320 * 240 - 1] = 0x1084}; /* two maps of one colour each */
const uint16_t rain_pixels[320 * 240] = {[0 ... 320 * 240 - 1] = 0x1f00};
int64_t    esp_timer_get_time(void) { return 0; }
/* NVS for the recipe of settings that stay: one blob in memory, and how many times it was committed */
static uint8_t nvs_blob[64];
static size_t  nvs_blob_size;
static int     nvs_commits;
esp_err_t nvs_open(const char *namespace_name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle)
{
    *out_handle = 1;
    return open_mode == NVS_READONLY && !nvs_blob_size ? ESP_FAIL : ESP_OK; /* read only, and nothing kept yet: no such namespace */
}
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length)
{
    if (!nvs_blob_size) return ESP_FAIL;
    if (*length < nvs_blob_size) return ESP_FAIL; /* no room for it */
    memcpy(out_value, nvs_blob, nvs_blob_size), *length = nvs_blob_size;
    return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length)
{
    if (length > sizeof nvs_blob) return ESP_FAIL;
    memcpy(nvs_blob, value, length), nvs_blob_size = length;
    return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t handle) { return ++nvs_commits, ESP_OK; }
void      nvs_close(nvs_handle_t handle) {}
/* a clock for the measured pass: 10 microseconds a read */
static int64_t runner_us;
static int64_t runner_clock(void) { return runner_us += 10; }
static int64_t runner_clock_still(void) { return runner_us; } /* moved by the runner's loop alone */
void       vTaskDelay(TickType_t t) { (void)t; }
BaseType_t xQueueSend(QueueHandle_t q, const void *i, TickType_t w) { return pdTRUE; }
bool dgx_ui_post(QueueHandle_t q, int what, const void *data, size_t size) { return true; }
bool dgx_ui_post_ref(QueueHandle_t q, int what, void *data, size_t size, void (*done)(void *, size_t)) { return true; }
/* what came from an interrupt, the touch's ticker and the chip's line, as the recipe of the touch woken by an interrupt uses them */
static int  isr_posted = -1, ticker_running, ticker_what, ticker_ms, line_pin = -1, line_level, line_watched;
static void (*line_handler)(void *);
bool dgx_ui_post_from_isr(QueueHandle_t q, int what, const void *data, size_t size, BaseType_t *woken) { return isr_posted = what, true; }
bool dgx_ui_ticker_start(dgx_ui_ticker_t *t, QueueHandle_t q, int what, uint32_t ms) { return ticker_running = 1, ticker_what = what, ticker_ms = (int)ms, true; }
void dgx_ui_ticker_stop(dgx_ui_ticker_t *t) { ticker_running = 0; }
esp_err_t gpio_config(const gpio_config_t *config)
{
    for (int pin = 0; pin < 64; ++pin)
        if (config->pin_bit_mask >> pin & 1) line_pin = pin;
    return line_level = config->intr_type, line_watched = 1, ESP_OK;
}
esp_err_t gpio_install_isr_service(int flags) { return ESP_OK; }
esp_err_t gpio_isr_handler_add(gpio_num_t pin, void (*handler)(void *), void *arg) { return line_handler = handler, ESP_OK; }
esp_err_t gpio_intr_enable(gpio_num_t pin) { return line_watched = 1, ESP_OK; }
esp_err_t gpio_intr_disable(gpio_num_t pin) { return line_watched = 0, ESP_OK; }
int  dgx_di_pointer_read(dgx_di_pointer_t *p, uint32_t now) { return 0; }
bool dgx_di_pointer_first(const dgx_di_pointer_t *p, int *x, int *y) { *x = *y = 0; return false; }
void dgx_di_touch_take(dgx_di_touch_t *t, int x, int y, bool down, uint32_t now) {}
int  dgx_di_keyboard_take(dgx_di_keyboard_t *kb, dgx_di_pointer_t *p, uint32_t now, uint32_t *typed, int max) { return 0; }
/* the keyboard of the strip's recipe: its band the lower part of the display, from 36 pixels down, as on a CYD */
const uint8_t keypad[1];
const size_t  keypad_size = sizeof keypad;
bool dgx_di_keyboard_open(dgx_di_keyboard_t *kb, dgx_screen_t *d, int mm_w, int mm_h, const uint8_t *layout, size_t size, const dgx_font_t *font,
                          const dgx_di_rect_t *field)
{
    kb->open = true, kb->x = 0, kb->y = 36, kb->w = (int16_t)d->width, kb->h = (int16_t)(d->height - 36);
    return true;
}
void dgx_di_keyboard_close(dgx_di_keyboard_t *kb) { kb->open = false; }
/* the keyboard's band drawn on another screen at an offset, as DGX-DI 0.1.7 does it: here one colour, 0xffff */
void dgx_di_keyboard_draw_to(const dgx_di_keyboard_t *kb, dgx_screen_t *scr, int ox, int oy)
{
    if (kb->open) dgx_fill_rectangle(scr, kb->x - ox, kb->y - oy, kb->w, kb->h, 0xffff);
}

static const dgx_ui_widget_t col_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};

/* a picture of a screen as PPM */
static void shot(dgx_screen_t *scr, const char *dir, const char *name)
{
    static uint32_t px[320 * 240];
    char            path[600];
    if (!dir) return;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    dgx_vscreen_to_argb8888(scr, 0, 0, scr->width, scr->height, NULL, px, scr->width);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", scr->width, scr->height);
    for (int i = 0; i < scr->width * scr->height; ++i) fputc(px[i] >> 16 & 255, f), fputc(px[i] >> 8 & 255, f), fputc(px[i] & 255, f);
    fclose(f);
}

/* a rectangle outlined in thin dashes: three pixels on, three off */
static void dashes(dgx_screen_t *scr, int x, int y, int w, int h, uint32_t color)
{
    for (int k = 0; k < w; ++k)
        if (k % 6 < 3) dgx_set_pixel(scr, x + k, y, color), dgx_set_pixel(scr, x + k, y + h - 1, color);
    for (int k = 0; k < h; ++k)
        if (k % 6 < 3) dgx_set_pixel(scr, x, y + k, color), dgx_set_pixel(scr, x + w - 1, y + k, color);
}

/* the recipe of the ring scale on a screen of 240: its picture, and the same with the boxes of the layout in dashes */
static void ring_pictures(const char *dir)
{
    static dgx_ui_node_t nodes[8];
    dgx_ui_t             ui;
    dgx_screen_t        *scr = dgx_vscreen_init(240, 240, 16, DgxScreenRGB);
    /* the recipe's own table: its ring_add() fills the scale's four colours, the paper and the ink are filled here */
    ring_table[PAPER] = dgx_screen_rgb(scr, 12, 14, 20), ring_table[INK] = dgx_screen_rgb(scr, 235, 235, 225);
    dgx_ui_paint_t paint = {.scr = scr, .colors = ring_table, .color_count = RING_COLORS, .background = PAPER};
    dgx_ui_init(&ui, nodes, 8, 1);
    const int ring = ring_add(&ui, scr);
    dgx_ui_msg_t    msg;
    const ppm_t     v = {.ppm = 1340};
    const climate_t c = {.t10 = 215, .rh = 41};
    dgx_ui_msg_value(&msg, MSG_PPM, &v, sizeof v), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    dgx_ui_msg_value(&msg, MSG_CLIMATE, &c, sizeof c), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    dgx_ui_paint(&ui, &paint, 0);
    shot(scr, dir, "ring.ppm");
    /* a second value: only the arc between the two and the number's field */
    const ppm_t v2 = {.ppm = 1350};
    dgx_ui_msg_value(&msg, MSG_PPM, &v2, sizeof v2), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    dgx_ui_paint(&ui, &paint, 1000);
    const ppm_t v3 = {.ppm = 1340};
    dgx_ui_msg_value(&msg, MSG_PPM, &v3, sizeof v3), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    dgx_ui_paint(&ui, &paint, 2000);
    /* the layout in dashes: the square the pad leaves, then the box of every widget */
    const dgx_ui_node_t *r   = dgx_ui_node(&ui, ring);
    const int            pad = r->widget->pad * ui.unit;
    dashes(scr, r->x + pad, r->y + pad, r->w - 2 * pad, r->h - 2 * pad, dgx_screen_rgb(scr, 90, 140, 255));
    for (int i = 0; i < ui.count; ++i) {
        const dgx_ui_node_t *n = dgx_ui_node(&ui, i);
        if (n->widget && n->w > 0 && n->h > 0) dashes(scr, n->x, n->y, n->w, n->h, dgx_screen_rgb(scr, 255, 170, 60));
    }
    shot(scr, dir, "ring-layout.ppm");
    for (int i = 0; i < ui.count; ++i) {
        const dgx_ui_node_t *n = dgx_ui_node(&ui, i);
        printf("ring: widget %d at %d,%d %dx%d\n", i, n->x, n->y, n->w, n->h);
    }
}

int main(int argc, char **argv)
{
    dgx_screen_t  *scr = dgx_vscreen_init(320, 240, 16, DgxScreenRGB);
    for (int k = 0; k < COLORS; ++k) table[k] = dgx_screen_rgb(scr, 30 * k, 30 * k, 255 - 30 * k);
    dgx_ui_paint_t paint = {.scr = scr, .colors = table, .color_count = COLORS, .background = PAPER};
    styles_init(), city_style_init();

    interface_test(); /* the message and the tap of the last recipe */
    printf("test recipe: the label got its text, the tap saved %d time(s)\n", saved);

    static dgx_ui_node_t nodes[32];
    dgx_ui_t             ui;
    dgx_ui_init(&ui, nodes, 32, 2);
    const int root = dgx_ui_add(&ui, -1, &col_w);

    /* the level look: 40 of 100 */
    const int level = dgx_ui_add(&ui, root, &level_w);
    dgx_ui_set_value(&ui, level, 40);
    dgx_ui_paint(&ui, &paint, 0);
    const dgx_ui_node_t *n = dgx_ui_node(&ui, level);
    printf("level look: box %d,%d %dx%d; a pixel at 20%% is the bar %d, at 80%% the rest %d\n", n->x, n->y, n->w, n->h,
           dgx_get_pixel(scr, n->x + n->w / 5, n->y + 1) == table[ACCENT], dgx_get_pixel(scr, n->x + n->w * 4 / 5, n->y + 1) == table[FILL]);

    /* the tree as a layer in the compositor's strips: told and composed, against the same tree painted over the same scene */
    {
        enum { SENTINEL = 0x1234 };
        dgx_screen_t  *layer_display = dgx_vscreen_init(320, 240, 16, DgxScreenRGB), *painted = dgx_vscreen_init(320, 240, 16, DgxScreenRGB);
        dgx_screen_t  *shown         = dgx_vscreen_from_array(layer_shown, 320, 240, 16, DgxScreenRGB);
        dgx_fill_rectangle(layer_display, 0, 0, 320, 240, SENTINEL);
        dgx_ui_paint_t layer_paint = {.scr = layer_display, .colors = table, .color_count = COLORS, .background = PAPER};
        dgx_ui_paint_t plain_paint = {.scr = painted, .colors = table, .color_count = COLORS, .background = PAPER};
        layer_init(&layer_paint), layer_init(&plain_paint);
        static const dgx_ui_widget_t layer_root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
        static const dgx_ui_widget_t layer_space_w  = {.kind = DGX_UI_BOX, .want_h = 100};
        static int32_t               layer_slider_value;
        static const dgx_ui_widget_t layer_slider_w = {.kind = DGX_UI_SLIDER, .max = 100, .want_h = 12, .value = &layer_slider_value, .look = &dgx_ui_look_slider};
        static dgx_ui_node_t         layer_nodes[2][4];
        dgx_ui_t                     layer_ui, plain_ui;
        int                          slider = -1;
        for (int which = 0; which < 2; ++which) {
            dgx_ui_t *tree = which ? &plain_ui : &layer_ui;
            dgx_ui_init(tree, layer_nodes[which], 4, 1);
            dgx_ui_add(tree, -1, &layer_root_w);
            dgx_ui_add(tree, 0, &layer_space_w);
            slider = dgx_ui_add(tree, 0, &layer_slider_w);
        }
        int differs[4], sent[4];
        for (int step = 0; step < 4; ++step) {
            if (step == 2) dgx_ui_set_value(&layer_ui, slider, 60), dgx_ui_set_value(&plain_ui, slider, 60);
            if (step == 3) {
                /* the scene changed in rows 32 to 63: the compositor marks them itself; the painted tree is told its background changed */
                ++scene_version;
                layer_changed(NULL, dgx_ui_rect(0, 32, 320, 32));
                dgx_ui_background_changed(&plain_ui, &plain_paint, 0, 32, 320, 32);
            }
            strips_sent = 0;
            layer_show(&layer_ui, &layer_paint, 1000 + 100 * (uint32_t)step);
            dgx_ui_paint(&plain_ui, &plain_paint, 1000 + 100 * (uint32_t)step);
            sent[step] = strips_sent, differs[step] = 0;
            for (int row = 0; row < 240; ++row)
                for (int column = 0; column < 320; ++column) differs[step] += dgx_get_pixel(shown, column, row) != dgx_get_pixel(painted, column, row);
        }
        int written = 0;
        for (int row = 0; row < 240; ++row)
            for (int column = 0; column < 320; ++column) written += dgx_get_pixel(layer_display, column, row) != SENTINEL;
        const dgx_ui_node_t *slider_node = dgx_ui_node(&layer_ui, slider);
        printf("layer: the slider in rows %d..%d; strips sent: first call %d, nothing changed %d, the slider moved %d, the scene changed in rows 32..63 %d\n",
               slider_node->y, slider_node->y + slider_node->h - 1, sent[0], sent[1], sent[2], sent[3]);
        printf("layer: what was sent differs from the same tree painted over the same scene by %d, %d, %d, %d pixels; written on the paint's own screen %d\n",
               differs[0], differs[1], differs[2], differs[3], written);
        dgx_ui_free(&layer_ui), dgx_ui_free(&plain_ui);
        dgx_ui_paint_free(&layer_paint), dgx_ui_paint_free(&plain_paint);
        dgx_screen_destroy(&layer_display), dgx_screen_destroy(&painted), dgx_screen_destroy(&shown);
    }

    /* pages that slide: with the two pictures, and on a board without them */
    for (int with_memory = 1; with_memory >= 0; --with_memory) {
        dgx_screen_t                *slide_screen = dgx_vscreen_init(160, 60, 16, DgxScreenRGB), *fresh = dgx_vscreen_init(160, 60, 16, DgxScreenRGB);
        dgx_ui_paint_t               slide_paint_here = {.scr = slide_screen, .colors = table, .color_count = COLORS, .background = PAPER};
        static const dgx_ui_widget_t slide_home_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};
        static const dgx_ui_widget_t slide_button_w = {.kind = DGX_UI_BUTTON, .want_h = 16, .look = &dgx_ui_look_button, .on_event = on_slide_settings};
        static const dgx_ui_widget_t slide_page_w   = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2, .on_event = on_slide_root};
        static int32_t               slide_slider_value;
        static const dgx_ui_widget_t slide_slider_w = {.kind = DGX_UI_SLIDER, .max = 100, .want_h = 10, .value = &slide_slider_value, .look = &dgx_ui_look_slider};
        static dgx_ui_node_t         slide_home_nodes[4];
        static dgx_ui_t              slide_home;
        big_memory_there = with_memory;
        page_slide       = (dgx_ui_transition_t){.ms = 250};
        dgx_ui_init(&slide_home, slide_home_nodes, 4, 1), dgx_ui_add(&slide_home, -1, &slide_home_w);
        const int open_button = dgx_ui_add(&slide_home, 0, &slide_button_w);
        dgx_ui_init(&page_settings, page_settings_nodes, 8, 1), dgx_ui_add(&page_settings, -1, &slide_page_w);
        const int page_slider = dgx_ui_add(&page_settings, 0, &slide_slider_w);
        pages_home            = &slide_home;
        slide_init(&slide_paint_here);
        const int saves_before = saves;
        int       open_frames = 0, back_frames = 0, differs_open, differs_back;
        uint32_t  now = 0;
        slide_show(&slide_home, &slide_paint_here, NULL, now);
        dgx_ui_focus(&slide_home, open_button), dgx_ui_key(&slide_home, DGX_UI_KEY_ACTIVATE); /* the settings */
        for (now = 20; page_slide.running && now < 2000; now += 20) {
            if (now == 100) dgx_ui_set_value(&page_settings, page_slider, 70); /* changed while the frames run */
            slide_show(&slide_home, &slide_paint_here, NULL, now), ++open_frames;
        }
        slide_show(&slide_home, &slide_paint_here, NULL, now); /* the pass after: what changed meanwhile */
        const bool page_on_top = slide_home.popup == &page_settings;
        dgx_ui_paint_area_to(&slide_home, &slide_paint_here, fresh, NULL, 0, 0, 160, 60);
        differs_open = 0;
        for (int row = 0; row < 60; ++row)
            for (int column = 0; column < 160; ++column) differs_open += dgx_get_pixel(fresh, column, row) != dgx_get_pixel(slide_screen, column, row);
        dgx_ui_key(&slide_home, DGX_UI_KEY_BACK); /* back by the key: the root saves and names the slide */
        for (now += 20; page_slide.running && now < 4000; now += 20) slide_show(&slide_home, &slide_paint_here, NULL, now), ++back_frames;
        slide_show(&slide_home, &slide_paint_here, NULL, now);
        dgx_ui_paint_area_to(&slide_home, &slide_paint_here, fresh, NULL, 0, 0, 160, 60);
        differs_back = 0;
        for (int row = 0; row < 60; ++row)
            for (int column = 0; column < 160; ++column) differs_back += dgx_get_pixel(fresh, column, row) != dgx_get_pixel(slide_screen, column, row);
        printf("slide, %s: the settings on top %d after %d frames, the screen then differs from the trees composed anew by %d pixels (the slider at %d); "
               "back: %d frames, the home on top %d, differs by %d; settings_save() called %d time(s)\n",
               with_memory ? "with the two pictures" : "no memory for them", page_on_top, open_frames, differs_open,
               dgx_ui_value(&page_settings, page_slider), back_frames, !slide_home.popup, differs_back, saves - saves_before);
        if (page_slide.from) dgx_screen_destroy(&page_slide.from);
        if (page_slide.to) dgx_screen_destroy(&page_slide.to);
        while (big_count > 0) free(big_blocks[--big_count]);
        dgx_ui_paint_free(&slide_paint_here), dgx_ui_free(&slide_home), dgx_ui_free(&page_settings);
        dgx_screen_destroy(&slide_screen), dgx_screen_destroy(&fresh);
    }

    /* the touch woken by an interrupt: the line watched, a finger, the ticks while any finger is on the pointer, the sleep after */
    {
        static dgx_ui_node_t    wake_nodes[2];
        static dgx_di_pointer_t wake_pointer; /* no finger on it */
        dgx_ui_t                wake_ui;
        dgx_ui_init(&wake_ui, wake_nodes, 2, 1);
        dgx_ui_add(&wake_ui, -1, &col_w);
        const bool started = touch_wake_start((QueueHandle_t)1, 36, true);
        printf("wake: started %d on pin %d by the low level %d; watched %d, the ticker running %d\n", started, line_pin, line_level == GPIO_INTR_LOW_LEVEL,
               line_watched, ticker_running);
        line_handler(NULL); /* a finger: the chip pulls its line */
        printf("wake: the interrupt posted the wake %d and turned itself off (watched %d)\n", isr_posted == MSG_TOUCH_WAKE, line_watched);
        touch_wake_message();
        printf("wake: the loop started the ticker %d of the touch's message %d every %d ms\n", ticker_running, ticker_what == MSG_TOUCH, ticker_ms);
        dgx_di_touch_t tree_touch = {.down = true};
        touch_sleep_when_up(&wake_ui, &wake_pointer, &tree_touch);
        printf("wake: a tick with the tree's finger down: the ticker running %d, watched %d\n", ticker_running, line_watched);
        tree_touch.down = false, wake_pointer.fingers[1].down = true; /* a finger on a key of the keyboard: not the tree's */
        touch_sleep_when_up(&wake_ui, &wake_pointer, &tree_touch);
        printf("wake: a tick with a finger on the keyboard and the tree's touch up: the ticker running %d, watched %d\n", ticker_running, line_watched);
        wake_pointer.fingers[1].down = false, wake_pointer.fingers[1].sees = true; /* seen by the device, not yet taken as down */
        touch_sleep_when_up(&wake_ui, &wake_pointer, &tree_touch);
        printf("wake: a tick with a finger seen and not down yet: the ticker running %d, watched %d\n", ticker_running, line_watched);
        wake_pointer.fingers[1].sees = false;
        touch_sleep_when_up(&wake_ui, &wake_pointer, &tree_touch);
        printf("wake: a tick with no finger on the pointer: the ticker running %d, watched %d\n", ticker_running, line_watched);
        dgx_ui_free(&wake_ui);
    }

    /* the log of the last lines: 60 lines added, every seventh wider than the box; the last rows shown after each, a row back by hand, a line too long */
    {
        dgx_screen_t                *log_screen = dgx_vscreen_init(200, 100, 16, DgxScreenRGB);
        dgx_ui_paint_t               log_paint  = {.scr = log_screen, .colors = table, .color_count = COLORS, .background = PAPER};
        static const dgx_ui_widget_t log_root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
        static dgx_ui_node_t         log_nodes[3];
        dgx_ui_t                     log_ui;
        dgx_ui_init(&log_ui, log_nodes, 3, 1);
        dgx_ui_add(&log_ui, -1, &log_root_w);
        const int log = log_make(&log_ui, 0);
        dgx_ui_screen(&log_ui, 200, 100, 0);
        int wrong = 0, longest = 0;
        for (int count = 1; count <= 60; ++count) {
            char line[80];
            if (count % 7 == 0) snprintf(line, sizeof line, "%d: the pump of the second floor does not answer, tried again", count);
            else snprintf(line, sizeof line, "%d: sensor %d ok", count, count * 3 % 11);
            log_add(&log_ui, log, line);
            dgx_ui_paint(&log_ui, &log_paint, (uint32_t)count * 100);
            const char *last_line = strrchr(log_buf, '\n');
            const int   rows = log_box.total_lines, first_shown = log_box.top;
            wrong += strcmp(last_line ? last_line + 1 : log_buf, line) != 0 || first_shown != (rows > LOG_ROWS ? rows - LOG_ROWS : 0);
            if ((int)strlen(log_buf) > longest) longest = (int)strlen(log_buf);
        }
        printf("log: 60 lines added, the last rows not shown or the new line not last %d times; the text %d bytes at most of %d; it begins \"%.14s\"\n", wrong, longest,
               (int)sizeof log_buf, log_buf);
        const int at_end = log_box.top;
        dgx_ui_text_scroll(&log_ui, log, -1), dgx_ui_paint(&log_ui, &log_paint, 6100);
        const int row_back = log_box.top;
        log_add(&log_ui, log, "61: back at the end"), dgx_ui_paint(&log_ui, &log_paint, 6200);
        printf("log: the first row shown %d at the end, %d a row back by hand, %d of %d rows after the next line\n", at_end, row_back, log_box.top, log_box.total_lines);
        char long_line[700] = "";
        for (int letter = 0; letter < 170; ++letter) strcat(long_line, letter % 9 == 8 ? " " : "\xd0\xb6"); /* a letter of two bytes */
        log_add(&log_ui, log, long_line), dgx_ui_paint(&log_ui, &log_paint, 6300);
        const size_t kept = strlen(log_buf);
        printf("log: a line of %d bytes: %d kept, cut on a whole letter %d, alone in the log %d\n", (int)strlen(long_line), (int)kept,
               (unsigned char)log_buf[kept - 1] < 0x80 || ((unsigned char)log_buf[kept - 1] & 0xC0) == 0x80, strchr(log_buf, '\n') == NULL);
        dgx_ui_paint_free(&log_paint), dgx_ui_free(&log_ui);
        dgx_screen_destroy(&log_screen);
    }

    /* how long a pass takes: three passes measured by a clock of 10 us a read, the line, the line in a label, the count begun anew */
    {
        static dgx_ui_node_t         stats_nodes[4];
        static char                  stats_buf[16], stats_line_buf[400];
        static dgx_ui_textbox_t      stats_box, stats_line_box;
        static const dgx_ui_widget_t stats_label_w  = {.kind = DGX_UI_LABEL, .text = &stats_box, .want_h = 14};
        static int32_t               stats_slider_value;
        static const dgx_ui_widget_t stats_slider_w = {.kind = DGX_UI_SLIDER, .min = 0, .max = 100, .want_h = 12, .value = &stats_slider_value, .look = &dgx_ui_look_slider};
        static const dgx_ui_widget_t stats_line_w   = {.kind = DGX_UI_LABEL, .flags = DGX_UI_REST, .text = &stats_line_box, .look = &dgx_ui_look_lines};
        dgx_screen_t                *stats_screen   = dgx_vscreen_init(200, 160, 16, DgxScreenRGB);
        dgx_ui_paint_t               stats_paint    = {.scr = stats_screen, .colors = table, .color_count = COLORS, .background = PAPER};
        dgx_ui_t                     stats_ui;
        dgx_ui_textbox_init(&stats_box, stats_buf, sizeof stats_buf, 1, &st_left);
        dgx_ui_textbox_init(&stats_line_box, stats_line_buf, sizeof stats_line_buf, 10, &st_left);
        dgx_ui_init(&stats_ui, stats_nodes, 4, 1);
        dgx_ui_add(&stats_ui, -1, &col_w);
        const int label = dgx_ui_add(&stats_ui, 0, &stats_label_w), slider = dgx_ui_add(&stats_ui, 0, &stats_slider_w);
        const int line_label = dgx_ui_add(&stats_ui, 0, &stats_line_w);
        dgx_ui_text_set(&stats_ui, label, "21.5 C");
        pass_stats.clock_us = runner_clock;
        stats_each_widget(&stats_ui);
        paint_measured(&stats_ui, &stats_paint, 0);  /* the column, the label, the slider and the empty label of the line */
        paint_measured(&stats_ui, &stats_paint, 20); /* nothing */
        dgx_ui_set_value(&stats_ui, slider, 40);
        paint_measured(&stats_ui, &stats_paint, 40); /* the slider */
        char line[400];
        const int length = dgx_ui_paint_stats_text(&pass_stats, "home", line, sizeof line);
        printf("stats (%d letters): %s\n", length, line);
        stats_show(&stats_ui, line_label);
        const bool same = strcmp(stats_line_buf, line) == 0;
        paint_measured(&stats_ui, &stats_paint, 10060); /* the line drawn, counted; ten seconds gone: the line in the log */
        printf("stats: the line shown in a label %d; then passes %u, drew %u, the slowest of the last pass widget %d\n", same, (unsigned)pass_stats.passes,
               (unsigned)pass_stats.drawing_passes, pass_stats.slowest_last.widget);
        char widget_line[96];
        dgx_ui_widget_stats_text(&pass_stats, slider, widget_line, sizeof widget_line);
        printf("stats: each widget: the slider's entry: %s; a line of the log for each widget drawn:\n", widget_line);
        stats_log_widgets();
        const uint32_t slider_draws = home_widgets[slider].draws;
        stats_anew();
        printf("stats: begun anew: passes %u, draws %u, the slider's entry %u draws before and %u after, the clock kept %d, the array named still %d\n",
               (unsigned)pass_stats.passes, (unsigned)pass_stats.draws, (unsigned)slider_draws, (unsigned)home_widgets[slider].draws, pass_stats.clock_us == runner_clock,
               pass_stats.per_widget == home_widgets && pass_stats.per_widget_tree == &stats_ui);
        pass_stats.per_widget = NULL, pass_stats.per_widget_tree = NULL; /* the tree goes with this block */
        /* the frames of a slide to a page: a frame costs 5 ms by the runner's own clock here and the loop waits 15 more */
        {
            static dgx_ui_node_t frames_page_nodes[2];
            dgx_ui_t             frames_page;
            dgx_ui_transition_t  frames_slide = {.from = dgx_vscreen_init(200, 160, 16, DgxScreenRGB), .to = dgx_vscreen_init(200, 160, 16, DgxScreenRGB), .ms = 200};
            dgx_ui_init(&frames_page, frames_page_nodes, 2, 1);
            dgx_ui_add(&frames_page, -1, &col_w);
            slide_stats.clock_us = runner_clock_still;
            dgx_ui_page_open_by(&stats_ui, &frames_page, &stats_paint, &frames_slide, DGX_UI_SLIDE_LEFT);
            int      frames_loops = 0;
            uint32_t frames_now   = 20000;
            while (frames_slide.running && frames_loops < 100) {
                frame_measured(&frames_slide, &stats_paint, frames_now);
                runner_us += 20000, frames_now += 20, ++frames_loops;
            }
            char frames_line[240];
            dgx_ui_frame_stats_text(&slide_stats, "slide", frames_line, sizeof frames_line);
            printf("stats: %d frames of the loop 20 ms apart: %s\n", frames_loops, frames_line);
            dgx_ui_page_back(&stats_ui);
            dgx_ui_free(&frames_page);
            dgx_screen_destroy(&frames_slide.from), dgx_screen_destroy(&frames_slide.to);
        }
        dgx_ui_paint_free(&stats_paint), dgx_ui_free(&stats_ui);
        dgx_screen_destroy(&stats_screen);
    }

    /* letters with soft edges: a heading in a font of 4 bits a pixel: the colours between the paper and the ink in its box */
    {
        static dgx_ui_node_t soft_nodes[2];
        dgx_screen_t        *soft_screen = dgx_vscreen_init(200, 48, 16, DgxScreenRGB);
        dgx_ui_paint_t       soft_paint  = {.scr = soft_screen, .colors = table, .color_count = COLORS, .background = PAPER};
        dgx_ui_t             soft_ui;
        dgx_ui_init(&soft_ui, soft_nodes, 2, 1);
        dgx_ui_add(&soft_ui, -1, &col_w);
        const int heading = heading_add(&soft_ui, 0);
        dgx_ui_text_set(&soft_ui, heading, "Living room");
        dgx_ui_paint(&soft_ui, &soft_paint, 0);
        const dgx_ui_node_t *heading_node = dgx_ui_node(&soft_ui, heading);
        uint32_t             seen[32];
        int                  colours = 0;
        for (int y = heading_node->y; y < heading_node->y + heading_node->h; ++y)
            for (int x = heading_node->x; x < heading_node->x + heading_node->w; ++x) {
                const uint32_t pixel = dgx_get_pixel(soft_screen, x, y);
                int            known = 0;
                for (int k = 0; k < colours; ++k) known |= seen[k] == pixel;
                if (!known && colours < 32) seen[colours++] = pixel;
            }
        printf("soft: the heading's box holds %d colours: the paper, the ink and %d between\n", colours, colours - 2);
        dgx_ui_paint_free(&soft_paint), dgx_ui_free(&soft_ui);
        dgx_screen_destroy(&soft_screen);
    }

    /* one knob: turns move the focus, a click on the slider begins its change and a click ends it; the list walked; a finger elsewhere,
       a finger on the slider itself, a popup over it and a page closed and gone let the slider go */
    {
        static dgx_ui_node_t         dial_nodes[6], dial_page_nodes[3];
        static char                  dial_button_buf[8], dial_list_buf[24];
        static dgx_ui_textbox_t      dial_button_box, dial_list_box;
        static const dgx_ui_widget_t dial_button_w = {.kind = DGX_UI_BUTTON, .text = &dial_button_box, .want_h = 14, .look = &dgx_ui_look_button};
        static int32_t               dial_volume, dial_page_volume, dial_row, dial_switched; /* the places of the dial's widgets */
        static const dgx_ui_widget_t dial_volume_w = {.kind = DGX_UI_SLIDER, .min = 0, .max = 100, .step = 5, .want_h = 12, .value = &dial_volume, .look = &dgx_ui_look_slider};
        static const dgx_ui_widget_t dial_page_volume_w = {.kind = DGX_UI_SLIDER, .min = 0, .max = 100, .step = 5, .want_h = 12, .value = &dial_page_volume, .look = &dgx_ui_look_slider};
        static const dgx_ui_widget_t dial_list_w   = {.kind = DGX_UI_LIST, .min = 0, .max = 2, .text = &dial_list_box, .want_h = 42, .value = &dial_row, .look = &dgx_ui_look_list};
        static const dgx_ui_widget_t dial_switch_w = {.kind = DGX_UI_SWITCH, .max = 1, .want_w = 26, .want_h = 12, .value = &dial_switched, .look = &dgx_ui_look_switch};
        dgx_screen_t                *dial_screen   = dgx_vscreen_init(160, 120, 16, DgxScreenRGB);
        dgx_ui_paint_t               dial_paint    = {.scr = dial_screen, .colors = table, .color_count = COLORS, .background = PAPER};
        dgx_ui_t                     dial_ui;
        static dgx_ui_t              dial_page;
        dgx_ui_msg_t                 dial_msg;
        dgx_ui_textbox_init(&dial_button_box, dial_button_buf, sizeof dial_button_buf, 1, &st_centre);
        dgx_ui_textbox_init(&dial_list_box, dial_list_buf, sizeof dial_list_buf, 1, &st_left);
        dgx_ui_init(&dial_ui, dial_nodes, 6, 1);
        dgx_ui_add(&dial_ui, -1, &col_w);
        const int button = dgx_ui_add(&dial_ui, 0, &dial_button_w), volume = dgx_ui_add(&dial_ui, 0, &dial_volume_w);
        const int rows = dgx_ui_add(&dial_ui, 0, &dial_list_w), toggle = dgx_ui_add(&dial_ui, 0, &dial_switch_w);
        dgx_ui_text_set(&dial_ui, button, "Go"), dgx_ui_text_set(&dial_ui, rows, "one\ntwo\nthree");
        dgx_ui_paint(&dial_ui, &dial_paint, 0);
#define DIAL(turned, was_clicked)                                                                                  \
    do {                                                                                                           \
        const dial_t dial_sent = {.steps = (turned), .clicked = (was_clicked)};                                    \
        dgx_ui_msg_value(&dial_msg, MSG_DIAL, &dial_sent, sizeof dial_sent), dial_message(&dial_ui, &dial_msg);    \
        dial_tidy(&dial_ui), dgx_ui_paint(&dial_ui, &dial_paint, 0);                                               \
    } while (0)
#define DIAL_PRESSED(tree, widget) ((dgx_ui_node((tree), (widget))->state.bits & DGX_UI_PRESSED) != 0)
        DIAL(1, false);
        DIAL(1, false);
        const int focus_at_slider = dial_ui.focus == volume;
        DIAL(0, true);
        const int pressed_in = DIAL_PRESSED(&dial_ui, volume);
        DIAL(3, false);
        const int moved_to = dgx_ui_value(&dial_ui, volume), focus_stays = dial_ui.focus == volume;
        DIAL(-1, true);
        printf("dial: two turns reach the slider %d; a click marks it %d; three turns move it to %d, the focus staying %d; a turn back and a click: %d, unmarked %d\n",
               focus_at_slider, pressed_in, moved_to, focus_stays, dgx_ui_value(&dial_ui, volume), !DIAL_PRESSED(&dial_ui, volume));
        DIAL(1, false);
        const int at_list = dial_ui.focus == rows;
        DIAL(2, false);
        const int picked = dgx_ui_value(&dial_ui, rows);
        DIAL(1, false);
        const int at_switch = dial_ui.focus == toggle;
        DIAL(0, true);
        printf("dial: the next turn reaches the list %d, two more pick its row %d, one more leaves it for the switch %d, a click turns it to %d\n", at_list, picked, at_switch,
               dgx_ui_value(&dial_ui, toggle));
        /* the slider being changed, and a finger presses the button: the slider is let go at the next pass */
        dgx_ui_focus(&dial_ui, volume);
        DIAL(0, true);
        const int marked = DIAL_PRESSED(&dial_ui, volume);
        const dgx_ui_node_t *button_node = dgx_ui_node(&dial_ui, button);
        dgx_ui_pointer(&dial_ui, &(dgx_ui_pointer_t){.x = (int16_t)(button_node->x + 4), .y = (int16_t)(button_node->y + 4), .down = true, .pressed = true, .ms = 100});
        dial_tidy(&dial_ui), dgx_ui_paint(&dial_ui, &dial_paint, 0);
        const int let_go = !DIAL_PRESSED(&dial_ui, volume);
        dgx_ui_pointer(&dial_ui, &(dgx_ui_pointer_t){.x = (int16_t)(button_node->x + 4), .y = (int16_t)(button_node->y + 4), .released = true, .ms = 150});
        DIAL(1, false);
        printf("dial: marked %d; a finger on the button lets the slider go %d; the next turn moves the focus, to the slider %d, its value kept %d\n", marked, let_go,
               dial_ui.focus == volume, dgx_ui_value(&dial_ui, volume) == 10);
        /* a finger takes the marked slider itself and lets it go: the change is over */
        DIAL(0, true);
        const dgx_ui_node_t *volume_node = dgx_ui_node(&dial_ui, volume);
        dgx_ui_pointer(&dial_ui, &(dgx_ui_pointer_t){.x = (int16_t)(volume_node->x + 8), .y = (int16_t)(volume_node->y + 4), .down = true, .pressed = true, .ms = 300});
        dgx_ui_pointer(&dial_ui, &(dgx_ui_pointer_t){.x = (int16_t)(volume_node->x + 8), .y = (int16_t)(volume_node->y + 4), .released = true, .ms = 350});
        dial_tidy(&dial_ui);
        const int value_by_finger = dgx_ui_value(&dial_ui, volume);
        DIAL(1, false);
        printf("dial: a finger took the marked slider and let it go: unmarked %d; the next turn moves the focus on %d, the value as the finger left it %d\n",
               !DIAL_PRESSED(&dial_ui, volume), dial_ui.focus == rows, dgx_ui_value(&dial_ui, volume) == value_by_finger);
        /* a page with a slider of its own, changed by the dial; the page closed, freed and its memory zeroed: nothing of it is read */
        dgx_ui_focus(&dial_ui, volume);
        DIAL(0, true); /* the home's slider marked, and a page opens over it: let go */
        const int marked_under = DIAL_PRESSED(&dial_ui, volume);
        dgx_ui_init(&dial_page, dial_page_nodes, 3, 1);
        dgx_ui_add(&dial_page, -1, &col_w);
        const int page_volume = dgx_ui_add(&dial_page, 0, &dial_page_volume_w);
        dgx_ui_page_open(&dial_ui, &dial_page);
        dial_tidy(&dial_ui);
        printf("dial: the home's slider marked %d; a page opened over it lets it go %d\n", marked_under, !DIAL_PRESSED(&dial_ui, volume));
        dgx_ui_focus(&dial_ui, button);
        dgx_ui_focus(&dial_page, page_volume);
        DIAL(0, true);
        DIAL(2, false);
        const int page_marked = DIAL_PRESSED(&dial_page, page_volume), page_value = dgx_ui_value(&dial_page, page_volume);
        dgx_ui_page_back(&dial_ui);
        dgx_ui_free(&dial_page);
        memset(&dial_page, 0, sizeof dial_page), memset(dial_page_nodes, 0, sizeof dial_page_nodes);
        dial_tidy(&dial_ui), dgx_ui_paint(&dial_ui, &dial_paint, 0);
        const int home_value = dgx_ui_value(&dial_ui, volume);
        DIAL(1, false);
        printf("dial: a slider of a page marked %d and moved to %d; the page closed and gone: the next turn moves the home's focus to its slider %d and changes no value %d\n",
               page_marked, page_value, dial_ui.focus == volume, dgx_ui_value(&dial_ui, volume) == home_value);
#undef DIAL
#undef DIAL_PRESSED
        dgx_ui_paint_free(&dial_paint), dgx_ui_free(&dial_ui);
        dgx_screen_destroy(&dial_screen);
    }

    /* settings that stay: nothing kept at first; a page edits a copy; Undo, Save, a restart that loads; a blob of another size; a value out of bounds */
    {
        static dgx_ui_node_t prefs_nodes[5];
        dgx_screen_t        *prefs_screen = dgx_vscreen_init(160, 60, 16, DgxScreenRGB);
        dgx_ui_paint_t       prefs_paint  = {.scr = prefs_screen, .colors = table, .color_count = COLORS, .background = PAPER};
        dgx_ui_t             prefs_page;
        prefs_load();
        printf("prefs: nothing kept: the first values %d %d %d\n", (int)prefs[PREF_BACKLIGHT], (int)prefs[PREF_VOLUME], (int)prefs[PREF_NIGHT]);
        dgx_ui_init(&prefs_page, prefs_nodes, 5, 1);
        dgx_ui_add(&prefs_page, -1, &col_w);
        const int backlight = dgx_ui_add(&prefs_page, 0, &pref_backlight_w), volume = dgx_ui_add(&prefs_page, 0, &pref_volume_w);
        const int night = dgx_ui_add(&prefs_page, 0, &pref_night_w);
        prefs_edit_begin();
        dgx_ui_paint(&prefs_page, &prefs_paint, 0);
        const int shown_at_first = dgx_ui_node(&prefs_page, backlight)->state_drawn.value;
        dgx_ui_set_value_as_user(&prefs_page, backlight, 30), dgx_ui_set_value_as_user(&prefs_page, night, 1); /* the user moves a slider and turns the switch */
        dgx_ui_paint(&prefs_page, &prefs_paint, 20);
        const bool changed = prefs_edit_changed();
        const int  edited = (int)prefs_edited[PREF_BACKLIGHT], used_meanwhile = (int)prefs[PREF_BACKLIGHT];
        prefs_edit_undo();
        dgx_ui_paint(&prefs_page, &prefs_paint, 40);
        printf("prefs: the page shows %d; the user sets 30 and night: changed %d, the copy %d, the program still reads %d; Undo: drawn %d again with no call, changed %d\n",
               shown_at_first, changed, edited, used_meanwhile, (int)dgx_ui_node(&prefs_page, backlight)->state_drawn.value, prefs_edit_changed());
        dgx_ui_set_value_as_user(&prefs_page, volume, 55);
        const bool saved = prefs_edit_save();
        printf("prefs: the volume set to 55 and saved %d: the program reads %d, committed %d time(s), %d bytes kept\n", saved, (int)prefs[PREF_VOLUME], nvs_commits,
               (int)nvs_blob_size);
        /* a restart: the first values again, then loaded */
        prefs[PREF_BACKLIGHT] = 70, prefs[PREF_VOLUME] = 40, prefs[PREF_NIGHT] = 0;
        prefs_load();
        printf("prefs: after a restart loaded: %d %d %d\n", (int)prefs[PREF_BACKLIGHT], (int)prefs[PREF_VOLUME], (int)prefs[PREF_NIGHT]);
        /* a blob of another size is not taken; a kept value out of today's bounds is shown within them and left as it is */
        nvs_blob_size = 8, prefs[PREF_VOLUME] = 40;
        prefs_load();
        const int after_short = (int)prefs[PREF_VOLUME];
        prefs[PREF_BACKLIGHT] = 0; /* kept by a program whose slider began at 0; today's begins at 5 */
        prefs_edit_begin();
        dgx_ui_paint(&prefs_page, &prefs_paint, 60);
        printf("prefs: a blob of 8 bytes not taken: the volume stays %d; a backlight of 0 kept: shown as %d, the variable still %d\n", after_short,
               (int)dgx_ui_value(&prefs_page, backlight), (int)prefs_edited[PREF_BACKLIGHT]);
        dgx_ui_paint_free(&prefs_paint), dgx_ui_free(&prefs_page);
        dgx_screen_destroy(&prefs_screen);
    }

    /* one of several, and a tick: a click chooses and tells once; chosen again tells nothing; the program writes the variable; the keys; the checkbox */
    {
        static dgx_ui_node_t air_nodes[6];
        dgx_screen_t        *air_screen = dgx_vscreen_init(160, 100, 16, DgxScreenRGB);
        dgx_ui_paint_t       air_paint  = {.scr = air_screen, .colors = table, .color_count = COLORS, .background = PAPER};
        dgx_ui_t             air_ui;
        dgx_ui_init(&air_ui, air_nodes, 6, 1);
        dgx_ui_add(&air_ui, -1, &col_w);
        air_add(&air_ui, 0); /* the buttons at 1, 2, 3, the checkbox at 4 */
        dgx_ui_paint(&air_ui, &air_paint, 0);
#define AIR_CHOSEN() (dgx_ui_radio_chosen(dgx_ui_node(&air_ui, 1)) * 1 + dgx_ui_radio_chosen(dgx_ui_node(&air_ui, 2)) * 2 + dgx_ui_radio_chosen(dgx_ui_node(&air_ui, 3)) * 4)
#define AIR_CLICK(widget, at_ms)                                                                                                                              \
    do {                                                                                                                                                      \
        const dgx_ui_node_t *clicked = dgx_ui_node(&air_ui, (widget));                                                                                        \
        dgx_ui_pointer(&air_ui, &(dgx_ui_pointer_t){.x = (int16_t)(clicked->x + 20), .y = (int16_t)(clicked->y + 2), .down = true, .pressed = true, .ms = (at_ms)}); \
        dgx_ui_pointer(&air_ui, &(dgx_ui_pointer_t){.x = (int16_t)(clicked->x + 20), .y = (int16_t)(clicked->y + 2), .released = true, .ms = (at_ms) + 50});         \
        dgx_ui_paint(&air_ui, &air_paint, (at_ms) + 60);                                                                                                      \
    } while (0)
        const int at_start = AIR_CHOSEN();
        AIR_CLICK(3, 100); /* on the text of "Night" */
        const int after_click = AIR_CHOSEN(), told_once = air_calls;
        AIR_CLICK(3, 300);
        printf("radio: at the start the buttons chosen %d (1: Auto); a click on Night: %d (4), the variable %d, told %d time(s) of mode %d; clicked again: told %d\n", at_start,
               after_click, (int)air_mode, told_once, air_mode_told, air_calls);
        air_mode = AIR_DAY; /* the program writes it: a message, the settings loaded */
        dgx_ui_paint(&air_ui, &air_paint, 500);
        const int by_program = AIR_CHOSEN(), drawn_day = (int)dgx_ui_node(&air_ui, 2)->state_drawn.value, drawn_night = (int)dgx_ui_node(&air_ui, 3)->state_drawn.value;
        air_mode = 7;
        dgx_ui_paint(&air_ui, &air_paint, 520);
        printf("radio: the program writes Day: chosen %d (2), drawn as chosen: Day %d, Night %d, told still %d; a number of no button: chosen %d, the variable %d\n", by_program,
               drawn_day, drawn_night, air_calls, AIR_CHOSEN(), (int)air_mode);
        dgx_ui_focus(&air_ui, 1);
        dgx_ui_key(&air_ui, DGX_UI_KEY_NEXT);
        dgx_ui_key(&air_ui, DGX_UI_KEY_ACTIVATE);
        printf("radio: the focus on Auto, the key next and the key that activates: the variable %d, the value of any button %d\n", (int)air_mode, (int)dgx_ui_value(&air_ui, 3));
        AIR_CLICK(4, 700); /* the checkbox */
        const int unticked = (int)beep_wanted, told_off = beep_told == 0;
        AIR_CLICK(4, 900);
        printf("radio: the checkbox clicked: %d, told so %d; clicked again: %d, told %d time(s) in all\n", unticked, told_off, (int)beep_wanted, beep_calls);
#undef AIR_CHOSEN
#undef AIR_CLICK
        dgx_ui_paint_free(&air_paint), dgx_ui_free(&air_ui);
        dgx_screen_destroy(&air_screen);
    }

    /* a caption from a string constant: drawn with no call; set refused; pointed at another constant and told: drawn in the other language */
    {
        static dgx_ui_node_t         caption_nodes[4];
        static const dgx_ui_widget_t caption_yes_w = {.kind = DGX_UI_BUTTON, .text = &caption_box[CAPTION_YES], .want_h = 16, .look = &dgx_ui_look_button};
        static const dgx_ui_widget_t caption_no_w  = {.kind = DGX_UI_BUTTON, .text = &caption_box[CAPTION_NO], .want_h = 16, .look = &dgx_ui_look_button};
        dgx_screen_t                *caption_screen = dgx_vscreen_init(120, 60, 16, DgxScreenRGB);
        dgx_ui_paint_t               caption_paint  = {.scr = caption_screen, .colors = table, .color_count = COLORS, .background = PAPER};
        dgx_ui_t                     caption_ui;
        dgx_ui_init(&caption_ui, caption_nodes, 4, 1);
        dgx_ui_add(&caption_ui, -1, &col_w);
        const int widgets[CAPTIONS] = {dgx_ui_add(&caption_ui, 0, &caption_yes_w), dgx_ui_add(&caption_ui, 0, &caption_no_w)};
        dgx_ui_paint(&caption_ui, &caption_paint, 0);
        int ink_first = 0, ink_second = 0;
        const dgx_ui_node_t *yes_node = dgx_ui_node(&caption_ui, widgets[CAPTION_YES]);
        for (int y = yes_node->y; y < yes_node->y + yes_node->h; ++y)
            for (int x = yes_node->x; x < yes_node->x + yes_node->w; ++x) ink_first += dgx_get_pixel(caption_screen, x, y) == table[INK];
        const bool set_refused = !dgx_ui_text_set(&caption_ui, widgets[CAPTION_YES], "Sure") && strcmp(caption_box[CAPTION_YES].buf, "Yes") == 0;
        captions_language(&caption_ui, widgets, 1);
        dgx_ui_paint(&caption_ui, &caption_paint, 20);
        for (int y = yes_node->y; y < yes_node->y + yes_node->h; ++y)
            for (int x = yes_node->x; x < yes_node->x + yes_node->w; ++x) ink_second += dgx_get_pixel(caption_screen, x, y) == table[INK];
        printf("caption: \"Yes\" drawn with no call, %d pixels of ink; a text set on it refused %d; pointed at \"%s\" and told: %d pixels of ink, another picture %d\n", ink_first,
               set_refused, caption_box[CAPTION_YES].buf, ink_second, ink_second != ink_first && ink_second > 0);
        dgx_ui_paint_free(&caption_paint), dgx_ui_free(&caption_ui);
        dgx_screen_destroy(&caption_screen);
    }

    /* the handwritten greeting: which variant each text takes in a label 200 pixels wide, the base line kept, letters coming */
    {
        dgx_screen_t                *hand_screen = dgx_vscreen_init(200, 80, 16, DgxScreenRGB); /* a column 200 pixels wide */
        dgx_ui_paint_t               hand_paint  = {.scr = hand_screen, .colors = table, .color_count = COLORS, .background = PAPER};
        static const dgx_ui_widget_t hand_root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
        static dgx_ui_node_t         hand_nodes[4];
        dgx_ui_t                     hand_ui;
        greeting_init(), signature_init();
        dgx_ui_init(&hand_ui, hand_nodes, 4, 1);
        dgx_ui_add(&hand_ui, -1, &hand_root_w);
        const int greeting = dgx_ui_add(&hand_ui, 0, &greeting_w), signature = dgx_ui_add(&hand_ui, 0, &signature_w);
        static const char *const said[] = {"Hello", "Good afternoon to you", "Good afternoon, dear guests of the house",
                                           "Good afternoon, dear guests of the house on the hill"};
        for (unsigned k = 0; k < sizeof said / sizeof *said; ++k) {
            dgx_ui_text_set(&hand_ui, greeting, said[k]);
            dgx_ui_paint(&hand_ui, &hand_paint, 0);
            const dgx_ui_node_t *label = dgx_ui_node(&hand_ui, greeting);
            /* the lowest row of ink in the columns of the first letter, which has nothing under its base line: G or H */
            int base_row = -1, last_column = -1;
            for (int row = label->y; row < label->y + label->h; ++row)
                for (int column = label->x; column < label->x + label->w; ++column)
                    if (dgx_get_pixel(hand_screen, column, row) != table[PAPER]) {
                        if (column < label->x + 8 && row > base_row) base_row = row;
                        if (column > last_column) last_column = column;
                    }
            printf("hand: '%s': variant %d in a label %dx%d, ink to column %d, the first letter's foot on row %d\n", said[k], greeting_box.variant, label->w,
                   label->h, last_column, base_row);
        }
        greeting_say(&hand_ui, greeting, "Hello");
        int passes = 0, frame = -1;
        uint32_t written_ms = 0;
        for (uint32_t now = 1000; now < 3000; now += DGX_UI_HAND_FRAME_MS) {
            const bool more = dgx_ui_paint(&hand_ui, &hand_paint, now);
            ++passes;
            if (!more) {
                written_ms = now - 1000;
                break;
            }
            frame = (int)hand_paint.wake_ms;
        }
        printf("hand: 'Hello' at 60 ms a letter: written in %u ms, %d passes, the look asking for a pass every %d ms; none asked after %d\n", written_ms, passes,
               frame, hand_paint.wake_ms == DGX_UI_WAKE_NONE);
        /* the signature by a soft pen in its canvas: more shades than ink and paper */
        dgx_ui_text_set(&hand_ui, signature, "Soft");
        dgx_ui_paint(&hand_ui, &hand_paint, 4000);
        const dgx_ui_node_t *signature_node = dgx_ui_node(&hand_ui, signature);
        int shades = 0, inked = 0;
        uint32_t seen[8];
        for (int row = signature_node->y; row < signature_node->y + signature_node->h; ++row)
            for (int column = signature_node->x; column < signature_node->x + signature_node->w; ++column) {
                const uint32_t pixel = dgx_get_pixel(hand_screen, column, row);
                if (pixel == table[PAPER]) continue;
                ++inked;
                int known = 0;
                for (int k = 0; k < shades; ++k) known |= seen[k] == pixel;
                if (!known && shades < 8) seen[shades++] = pixel;
            }
        printf("hand: the signature by a soft pen in a canvas %dx%d: %d pixels of ink in %d shades\n", signature_node->w, signature_node->h, inked, shades);
        dgx_ui_free(&hand_ui);
        dgx_ui_paint_free(&hand_paint);
        dgx_screen_destroy(&hand_screen);
    }

    /* the handwritten note of several lines: its lines, the letters coming line after line, the bitmap font for one too long */
    {
        dgx_screen_t                *note_screen = dgx_vscreen_init(200, 60, 16, DgxScreenRGB); /* three lines of 20 */
        dgx_ui_paint_t               note_paint  = {.scr = note_screen, .colors = table, .color_count = COLORS, .background = PAPER};
        static const dgx_ui_widget_t note_root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
        static dgx_ui_node_t         note_nodes[2];
        dgx_ui_t                     note_ui;
        note_init();
        dgx_ui_init(&note_ui, note_nodes, 2, 1);
        dgx_ui_add(&note_ui, -1, &note_root_w);
        const int note = dgx_ui_add(&note_ui, 0, &note_w);
        dgx_ui_text_set(&note_ui, note, "The quick brown fox jumps over the lazy dog and runs on and on");
        dgx_ui_paint(&note_ui, &note_paint, 0);
        const int lines = note_box.total_lines, variant_short = note_box.variant;
        dgx_ui_text_appear(&note_ui, note, 40);
        int passes = 0, frame = -1, changed_least = 1 << 30, changed_most = 0;
        static uint16_t before[200 * 60];
        for (uint32_t now = 1000; now < 9000; now += DGX_UI_HAND_FRAME_MS) {
            for (int row = 0; row < 60; ++row)
                for (int column = 0; column < 200; ++column) before[row * 200 + column] = (uint16_t)dgx_get_pixel(note_screen, column, row);
            const bool more = dgx_ui_paint(&note_ui, &note_paint, now);
            int        changed = 0;
            for (int row = 0; row < 60; ++row)
                for (int column = 0; column < 200; ++column) changed += before[row * 200 + column] != (uint16_t)dgx_get_pixel(note_screen, column, row);
            if (passes && more) changed_least = changed < changed_least ? changed : changed_least, changed_most = changed > changed_most ? changed : changed_most;
            ++passes;
            if (!more) break;
            frame = (int)note_paint.wake_ms;
        }
        dgx_ui_text_set(&note_ui, note, "The quick brown fox jumps over the lazy dog and runs on and on over the hills and far away until the night comes down upon the land");
        dgx_ui_paint(&note_ui, &note_paint, 20000);
        printf("note: %d lines in a note 200x%d by variant %d; its letters at 40 ms written in %d passes, a pass asked every %d ms, a pass changing %d to %d "
               "pixels of the screen; a note too long takes variant %d\n",
               lines, dgx_ui_node(&note_ui, note)->h, variant_short, passes, frame, changed_least, changed_most, note_box.variant);
        dgx_ui_free(&note_ui);
        dgx_ui_paint_free(&note_paint);
        dgx_screen_destroy(&note_screen);
    }

    /* a widget of the program's own worked by the finger and the keys: the pad of two axes, and the bar that is tapped */
    {
        dgx_screen_t                *xs = dgx_vscreen_init(40, 64, 16, DgxScreenRGB); /* a column 40 pixels wide */
        dgx_ui_paint_t               xp = {.scr = xs, .colors = table, .color_count = COLORS, .background = PAPER};
        static const dgx_ui_widget_t xrow_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
        static dgx_ui_node_t         xn[6];
        dgx_ui_t                     x;
        dgx_ui_init(&x, xn, 6, 1);
        dgx_ui_add(&x, -1, &xrow_w);
        const int pad = dgx_ui_add(&x, 0, &xy_w), tap = dgx_ui_add(&x, 0, &tap_w);
        dgx_ui_paint(&x, &xp, 0);
        const dgx_ui_node_t *pn = dgx_ui_node(&x, pad), *tn = dgx_ui_node(&x, tap);
        printf("own: the pad at %d,%d %dx%d, hit %d; the bar at %d,%d %dx%d, hit %d\n", pn->x, pn->y, pn->w, pn->h,
               dgx_ui_hit(&x, pn->x + 30, pn->y + 10) == pad, tn->x, tn->y, tn->w, tn->h, dgx_ui_hit(&x, tn->x + 1, tn->y + 1) == tap);
        /* step 1: down at 30, 10 */
        dgx_ui_pointer(&x, &(dgx_ui_pointer_t){.x = (int16_t)(pn->x + 30), .y = (int16_t)(pn->y + 10), .down = true, .pressed = true, .ms = 1000});
        printf("own: down at 30,10: y %d, value %d, on_event() told %d time(s) the point %d,%d; pressed %d, focused %d, dragging %d\n", xy_data.y,
               dgx_ui_value_of(pn), point_calls, point_x, point_y, !!(pn->state.bits & DGX_UI_PRESSED), x.focus == pad, dgx_ui_dragging(&x));
        dgx_ui_paint(&x, &xp, 10);
        int knob = 0, kx0 = 999, ky0 = 999, field = 0;
        for (int yy = 0; yy < pn->h; ++yy)
            for (int xx = 0; xx < pn->w; ++xx) {
                const uint32_t c = dgx_get_pixel(xs, pn->x + xx, pn->y + yy);
                if (c == table[ACCENT_DOWN]) ++knob, kx0 = xx < kx0 ? xx : kx0, ky0 = yy < ky0 ? yy : ky0;
                else if (c == table[FILL]) ++field;
            }
        printf("own: drawn pressed: %d pixels of the knob from %d,%d, %d of the field, of %d\n", knob, kx0, ky0, field, pn->w * pn->h);
        /* step 2: a move out of the widget, up and to the left of it and under it */
        dgx_ui_pointer(&x, &(dgx_ui_pointer_t){.x = (int16_t)(pn->x - 10), .y = (int16_t)(pn->y + 70), .down = true, .ms = 1050});
        printf("own: moved to -10,70: y %d, value %d, dragging %d\n", xy_data.y, dgx_ui_value_of(pn), dgx_ui_dragging(&x));
        /* step 3: let go: no click */
        point_calls = 0;
        dgx_ui_pointer(&x, &(dgx_ui_pointer_t){.x = (int16_t)(pn->x - 10), .y = (int16_t)(pn->y + 70), .released = true, .ms = 1100});
        printf("own: let go: pressed %d, on_event() told %d more time(s)\n", !!(pn->state.bits & DGX_UI_PRESSED), point_calls);
        /* let go, the pad keeps the focus: the knob hollow, its border of 6 by 6 pixels in the ink */
        dgx_ui_paint(&x, &xp, 1100);
        int border = 0, inside = 0;
        for (int yy = 0; yy < pn->h; ++yy)
            for (int xx = 0; xx < pn->w; ++xx) border += dgx_get_pixel(xs, pn->x + xx, pn->y + yy) == table[INK], inside += dgx_get_pixel(xs, pn->x + xx, pn->y + yy) == table[FILL];
        printf("own: drawn focused: %d pixels of the knob's border, %d of the field and the knob's inside, of %d\n", border, inside, pn->w * pn->h);
        /* step 4: the keys */
        const bool more = dgx_ui_key(&x, DGX_UI_KEY_MORE);
        printf("own: MORE taken %d, value %d; ACTIVATE taken %d; NEXT taken %d, the focus stays on the pad (nobody else takes it) %d\n", more,
               dgx_ui_value_of(pn), dgx_ui_key(&x, DGX_UI_KEY_ACTIVATE), dgx_ui_key(&x, DGX_UI_KEY_NEXT), x.focus == pad);
        /* the bar that is tapped: pressed, the focus left on the pad */
        dgx_ui_pointer(&x, &(dgx_ui_pointer_t){.x = (int16_t)(tn->x + 1), .y = (int16_t)(tn->y + 1), .down = true, .pressed = true, .ms = 2000});
        printf("own: the bar pressed %d, the focus still on the pad %d\n", !!(tn->state.bits & DGX_UI_PRESSED), x.focus == pad);
        dgx_ui_pointer(&x, &(dgx_ui_pointer_t){.x = (int16_t)(tn->x + 1), .y = (int16_t)(tn->y + 1), .released = true, .ms = 2050});
        dgx_ui_free(&x);
        dgx_screen_destroy(&xs);
    }

    /* the icon: a message of 3 bars, then of none */
    const int   sig = dgx_ui_add(&ui, root, &signal_w);
    dgx_ui_msg_t msg;
    signal_t     s = {.bars = 3};
    dgx_ui_msg_value(&msg, MSG_SIGNAL, &s, sizeof s), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    const int v3 = dgx_ui_value(&ui, sig);
    s.bars = 0;
    dgx_ui_msg_value(&msg, MSG_SIGNAL, &s, sizeof s), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    printf("icon: value %d after 3 bars, %d after none (outside 1..4: no picture); its box %dx%d\n", v3, dgx_ui_value(&ui, sig),
           dgx_ui_node(&ui, sig)->w, dgx_ui_node(&ui, sig)->h);

    /* the slider and the switch by keys */
    const int sl = dgx_ui_add(&ui, root, &backlight_w), sw = dgx_ui_add(&ui, root, &wifi_w);
    dgx_ui_focus(&ui, sl), dgx_ui_key(&ui, DGX_UI_KEY_MORE);
    dgx_ui_focus(&ui, sw), dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE);
    backlight_show(30);
    printf("slider: started at 70, MORE told backlight_set(%d); set to 30 by the program, on_event() not told again (%d); switch told wifi_enable(%d)\n", backlight,
           backlight, wifi);

    /* the button, and the alarm that comes and goes */
    const int save = save_add(&ui, root);
    dgx_ui_focus(&ui, save), dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE);
    alarm_set(&ui, root, "filter!");
    const int a1 = dgx_ui_find(&ui, root, TAG_ALARM), first = dgx_ui_node(&ui, root)->child;
    alarm_set(&ui, root, NULL);
    printf("button: activated by the key, saves %d; alarm: found %d, it is the first child %d, after removal found %d\n", saves, a1 >= 0, a1 == first,
           dgx_ui_find(&ui, root, TAG_ALARM));

    /* the texts of lines, the buttons of the board, the colours */
    texts_add(&ui, root);
    bool more = dgx_ui_paint(&ui, &paint, 0);
    printf("lines: asks for time while the letters come: %d\n", more);
    /* the talk that waits for its text, on the hint that appears letter by letter: a key while it comes shows it all
       at the next pass, a key once it is seen sets the next line, which appears again */
    int hint = -1;
    for (int k = 0; k < ui.count; ++k) hint = dgx_ui_node(&ui, k) && dgx_ui_node(&ui, k)->widget == &hint_w ? k : hint;
    dgx_ui_text_set(&ui, hint, talk[0]);
    dgx_ui_paint(&ui, &paint, 10);
    const int talk_coming = dgx_ui_text_state(&ui, hint);
    talk_key(&ui, hint);
    dgx_ui_paint(&ui, &paint, 11);
    const int talk_seen = dgx_ui_text_state(&ui, hint);
    talk_key(&ui, hint);
    dgx_ui_paint(&ui, &paint, 12);
    printf("talk: coming %d; a key, and at the next pass seen %d; a key more: line %d, coming again %d\n", talk_coming == DGX_UI_TEXT_COMING,
           talk_seen == DGX_UI_TEXT_SEEN, talk_at, dgx_ui_text_state(&ui, hint) == DGX_UI_TEXT_COMING);
    buttons_t b = {.held = DGX_UI_GAMEPAD_DOWN};
    dgx_ui_msg_value(&msg, MSG_BUTTONS, &b, sizeof b), buttons_message(&ui, &msg, 100);
    const int f1 = ui.focus;
    b.held       = DGX_UI_GAMEPAD_BACK;
    dgx_ui_msg_value(&msg, MSG_BUTTONS, &b, sizeof b), buttons_message(&ui, &msg, 200);
    printf("buttons: DOWN moved the focus from %d to %d; BACK was the program's: menu_back %d time(s)\n", save, f1, backs);
    colors_set(&ui, &paint, table);

    /* the popup: opened, Yes activated by keys */
    ask_open(&ui);
    dgx_ui_paint(&ui, &paint, 300);
    const int saves0 = saves;
    dgx_ui_key(&ui, DGX_UI_KEY_NEXT), dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE);
    printf("popup: open %d before; Yes by keys saved %d more time(s), open after: %d\n", 1, saves - saves0, ui.popup != NULL);
    dgx_ui_paint(&ui, &paint, 400);
    dgx_ui_paint_free(&paint);
    /* the row of three buttons: right does nothing by default, and moves the focus with the program's table */
    {
        static const dgx_ui_widget_t  line_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 20};
        static const dgx_ui_widget_t  b_w    = {.kind = DGX_UI_BUTTON, .want_w = 40};
        static dgx_ui_node_t          rn[6];
        dgx_ui_t                      r;
        dgx_ui_init(&r, rn, 6, 1);
        const int line = dgx_ui_add(&r, -1, &line_w), back = dgx_ui_add(&r, line, &b_w), ok = dgx_ui_add(&r, line, &b_w);
        dgx_ui_add(&r, line, &b_w);
        dgx_ui_screen(&r, 200, 100, 0);
        dgx_ui_focus(&r, back);
        const bool took_default = dgx_ui_buttons(&r, DGX_UI_GAMEPAD_RIGHT, 0);
        const int  f_default    = r.focus;
        dgx_ui_buttons(&r, 0, 10);
        dialog_keys(&r, true);
        const bool took_row = dgx_ui_buttons(&r, DGX_UI_GAMEPAD_RIGHT, 20);
        printf("row of buttons: by default right is taken %d and the focus stays on Back %d; with row_keys taken %d and the focus is on OK %d\n", took_default,
               f_default == back, took_row, r.focus == ok);
    }
    /* the list: a finger picks a row and its click activates; between two rows nothing; a swipe takes the pick along;
       the keys go through the rows, then on */
    {
        static dgx_ui_node_t ln[4];
        static dgx_ui_t      l;
        static const dgx_ui_widget_t after_w = {.kind = DGX_UI_BUTTON, .want_h = 14};
        dgx_ui_init(&l, ln, 4, 1);
        const int top = dgx_ui_add(&l, -1, &col_w), list = mode_add(&l, top), after = dgx_ui_add(&l, top, &after_w);
        dgx_ui_screen(&l, 200, 100, 0), dgx_ui_layout(&l);
        const dgx_ui_node_t *n = dgx_ui_node(&l, list);
        const int16_t        x = (int16_t)(n->x + 5), y0 = n->y;
        dgx_ui_pointer(&l, &(dgx_ui_pointer_t){.x = x, .y = (int16_t)(y0 + 14 * 2 + 7), .down = true, .pressed = true, .ms = 10});
        const int picked_down = dgx_ui_value_of(n), modes_down = modes;
        dgx_ui_pointer(&l, &(dgx_ui_pointer_t){.x = x, .y = (int16_t)(y0 + 14 * 2 + 7), .released = true, .ms = 60});
        printf("list: %d high for 4 rows; a finger down on row 2 picks %d, mode_set() called %d time(s) yet; let go: mode %d\n", n->h, picked_down, modes_down, mode);
        dgx_ui_pointer(&l, &(dgx_ui_pointer_t){.x = x, .y = (int16_t)(y0 + 14), .down = true, .pressed = true, .ms = 100});
        const int at_edge = dgx_ui_value_of(n);
        dgx_ui_pointer(&l, &(dgx_ui_pointer_t){.x = x, .y = (int16_t)(y0 + 14), .released = true, .ms = 150});
        dgx_ui_pointer(&l, &(dgx_ui_pointer_t){.x = x, .y = (int16_t)(y0 + 7), .down = true, .pressed = true, .ms = 200});
        dgx_ui_pointer(&l, &(dgx_ui_pointer_t){.x = x, .y = (int16_t)(y0 + 7 + 28), .down = true, .ms = 250});
        const int swiped = dgx_ui_value_of(n), modes_before = modes;
        dgx_ui_pointer(&l, &(dgx_ui_pointer_t){.x = x, .y = (int16_t)(y0 + 7 + 28), .released = true, .ms = 300});
        printf("list: a finger on the line between rows 0 and 1 leaves the pick %d; down on row 0 and moved two rows: %d, no click %d\n", at_edge, swiped,
               modes == modes_before);
        dgx_ui_set_value(&l, list, 2);
        dgx_ui_key(&l, DGX_UI_KEY_NEXT);
        const int next1 = dgx_ui_value_of(n), f1 = l.focus;
        dgx_ui_key(&l, DGX_UI_KEY_NEXT);
        printf("list: NEXT on row 2: the pick %d, the focus still on the list %d; NEXT on the last row: the focus on the next widget %d\n", next1, f1 == list,
               l.focus == after);
        dgx_ui_free(&l);
    }
    /* the moment a scrolling text names, and the mirror of a screen with a popup open */
    {
        static dgx_ui_node_t wn[4], wp[2];
        static dgx_ui_t      w, wpop;
        static char          news_buf[120], ask2_buf[8];
        static dgx_ui_textbox_t      news_box, ask2_box;
        static dgx_ui_text_style_t   st_news;
        static const dgx_ui_widget_t news_w = {.kind = DGX_UI_LABEL, .text = &news_box, .look = &dgx_ui_look_lines};
        static const dgx_ui_widget_t wp_w   = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 3};
        static const dgx_ui_widget_t ask2_w = {.kind = DGX_UI_LABEL, .text = &ask2_box, .want_h = 14};
        dgx_screen_t                *scr    = dgx_vscreen_init(320, 240, 16, DgxScreenRGB);
        static uint32_t              tb[COLORS];
        for (int k = 0; k < COLORS; ++k) tb[k] = dgx_screen_rgb(scr, (uint8_t)(30 * k), (uint8_t)(250 - 30 * k), (uint8_t)(10 + 25 * k));
        dgx_ui_paint_t pp = {.scr = scr, .colors = tb, .color_count = COLORS, .background = PAPER};
        st_news = (dgx_ui_text_style_t){.fonts = small, .count = 1, .flags = DGX_UI_STYLE_IMPORTANT, .scroll_ms = 2000};
        dgx_ui_textbox_init(&news_box, news_buf, sizeof news_buf, 2, &st_news);
        dgx_ui_textbox_init(&ask2_box, ask2_buf, sizeof ask2_buf, 1, &st_left);
        dgx_ui_init(&w, wn, 4, 2);
        const int top = dgx_ui_add(&w, -1, &col_w), news = dgx_ui_add(&w, top, &news_w);
        dgx_ui_text_set(&w, news, "one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen sixteen seventeen eighteen "
                                  "nineteen twenty");
        const bool     more0 = dgx_ui_paint(&w, &pp, 0);
        const uint32_t at0   = dgx_ui_wake(&pp, &w, 0);
        dgx_ui_paint(&w, &pp, 500); /* a message woke the loop at 500 */
        const uint32_t at500 = dgx_ui_wake(&pp, &w, 500);
        printf("wake: the text asks for time %d; the loop may wait %u ms at 0 and %u at 500; ", more0, (unsigned)at0, (unsigned)at500);
        dgx_ui_text_set(&w, news, "short");
        dgx_ui_paint(&w, &pp, 600);
        printf("a text that does not scroll: nobody asks %d\n", dgx_ui_wake(&pp, &w, 600) == DGX_UI_WAKE_NONE);
        dgx_ui_init(&wpop, wp, 2, 2), dgx_ui_add(&wpop, -1, &wp_w);
        dgx_ui_text_set(&wpop, dgx_ui_add(&wpop, 0, &ask2_w), "Sure?");
        dgx_ui_popup_open(&w, &wpop, 60, 80, 200, 60, DGX_UI_POPUP_MODAL);
        dgx_ui_paint(&w, &pp, 700);
        mirror(&w, &pp, NULL);
        dgx_screen_t *whole = dgx_vscreen_from_array(mirror_all, 320, 240, 16, DgxScreenRGB);
        int           off   = 0;
        for (int y = 0; y < 240; ++y)
            for (int x = 0; x < 320; ++x) off += dgx_get_pixel(whole, x, y) != dgx_get_pixel(scr, x, y);
        printf("mirror: %d pixels sent in strips; with a popup open it differs from the screen in %d pixels; the paint draws on its screen again %d\n", mirror_got,
               off, pp.scr == scr);
        /* with a keyboard of DGX-DI open over the rows from 203 down, a band that begins inside a strip */
        dgx_di_keyboard_t kbd = {.open = true, .x = 0, .y = 203, .w = 320, .h = 37};
        mirror_got            = 0;
        mirror(&w, &pp, &kbd);
        int band_px = 0, above = 0;
        for (int y = 0; y < 240; ++y)
            for (int x = 0; x < 320; ++x) {
                const bool key = dgx_get_pixel(whole, x, y) == 0xffff;
                if (y >= 203) band_px += key;
                else above += dgx_get_pixel(whole, x, y) != dgx_get_pixel(scr, x, y);
            }
        printf("mirror: with the keyboard's band from row 203: %d of its %d pixels are the keyboard's, the rows above it differ from the screen in %d\n", band_px,
               320 * 37, above);
        dgx_screen_destroy(&whole);
        dgx_ui_paint_free(&pp), dgx_ui_free(&w), dgx_ui_free(&wpop);
        scr->destroy(&scr);
    }
    /* a list that scrolls: twelve months in 56 pixels are four rows at a time, the window following the pick */
    {
        static dgx_ui_node_t sn[2];
        static dgx_ui_t      s;
        static char          months_buf[64];
        static dgx_ui_textbox_t      months_box;
        static int32_t               month_picked;
        static const dgx_ui_widget_t months_w = {.kind = DGX_UI_LIST, .text = &months_box, .max = 11, .want_h = 56, .value = &month_picked, .look = &dgx_ui_look_list};
        dgx_ui_textbox_init(&months_box, months_buf, sizeof months_buf, 1, &st_left);
        dgx_ui_init(&s, sn, 2, 1);
        const int top = dgx_ui_add(&s, -1, &col_w), list = dgx_ui_add(&s, top, &months_w);
        dgx_ui_text_set(&s, list, "Jan\nFeb\nMar\nApr\nMay\nJun\nJul\nAug\nSep\nOct\nNov\nDec");
        dgx_ui_screen(&s, 200, 100, 0), dgx_ui_layout(&s);
        dgx_ui_focus(&s, list);
        for (int k = 0; k < 4; ++k) dgx_ui_key(&s, DGX_UI_KEY_NEXT);
        const int at4 = months_box.top;
        for (int k = 0; k < 7; ++k) dgx_ui_key(&s, DGX_UI_KEY_NEXT);
        const int at11 = months_box.top;
        dgx_ui_text_scroll(&s, list, -8);
        printf("scroll: the pick on the fifth row: the window from %d (rows 1..4); on the twelfth: from %d (four rows at a time); scrolled by the program: from %d, "
               "the pick still %d\n", at4, at11, months_box.top, dgx_ui_value(&s, list));
        dgx_ui_free(&s);
    }
    /* pages: the settings over the clock, "about" over the settings, and back twice; the clock fed under them */
    {
        static dgx_ui_node_t hn[2];
        static dgx_ui_t      home;
        static char          home_buf[16];
        static dgx_ui_textbox_t      home_box;
        static const dgx_ui_widget_t home_w = {.kind = DGX_UI_LABEL, .text = &home_box, .want_h = 14};
        dgx_screen_t                *scr    = dgx_vscreen_init(160, 60, 16, DgxScreenRGB);
        static uint32_t              tb[COLORS];
        for (int k = 0; k < COLORS; ++k) tb[k] = dgx_screen_rgb(scr, (uint8_t)(30 * k), (uint8_t)(250 - 30 * k), (uint8_t)(10 + 25 * k));
        dgx_ui_paint_t pp = {.scr = scr, .colors = tb, .color_count = COLORS, .background = PAPER};
        dgx_ui_textbox_init(&home_box, home_buf, sizeof home_buf, 1, &st_left);
        dgx_ui_init(&home, hn, 2, 1);
        const int top = dgx_ui_add(&home, -1, &col_w), label = dgx_ui_add(&home, top, &home_w);
        /* the pages of the recipe: the settings with a "back" button and a button for "about" */
        pages_home = &home;
        const int saves_before = saves; /* settings_save() of the button recipe counts them */
        dgx_ui_init(&page_settings, page_settings_nodes, 8, 1), dgx_ui_add(&page_settings, -1, &settings_root_w);
        const int back_button = dgx_ui_add(&page_settings, 0, &settings_back_w), about_button = dgx_ui_add(&page_settings, 0, &settings_about_w);
        dgx_ui_init(&page_about, page_about_nodes, 4, 1), dgx_ui_add(&page_about, -1, &about_root_w);
        const bool opened_before_pass = dgx_ui_page_open(&home, &page_settings); /* before the first pass: laid at it */
        dgx_ui_paint(&home, &pp, 0);
        const dgx_ui_node_t *n = dgx_ui_node(&home, label), *settings_root = dgx_ui_node(&page_settings, 0);
        const int            root_w = settings_root->w, root_h = settings_root->h;
        dgx_ui_page_back(&home); /* the root told: saved once */
        const int saved_first = saves - saves_before;
        dgx_ui_paint(&home, &pp, 0);
        int ink0 = 0, ink1 = 0, ink2 = 0;
        const bool o1 = dgx_ui_page_open(&home, &page_settings);
        dgx_ui_paint(&home, &pp, 1);
        dgx_ui_text_set(&home, label, "23:59"); /* a value that came for the clock while the settings show */
        dgx_ui_paint(&home, &pp, 2);
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) ink0 += dgx_get_pixel(scr, x, y) == tb[INK];
        /* "about" by its button of the settings, focused and activated by the keys */
        dgx_ui_focus(&page_settings, about_button);
        const bool o2      = dgx_ui_key(&home, DGX_UI_KEY_ACTIVATE);
        const bool stacked = home.popup == &page_settings && page_settings.popup == &page_about;
        const bool b1      = dgx_ui_key(&home, DGX_UI_KEY_BACK); /* the key: "about" closed */
        const bool settings_top = home.popup == &page_settings && !page_settings.popup;
        dgx_ui_paint(&home, &pp, 3);
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) ink1 += dgx_get_pixel(scr, x, y) == tb[INK];
        /* the page's own "back" button, by the finger */
        const dgx_ui_node_t *back_node = dgx_ui_node(&page_settings, back_button);
        const int16_t        back_x = (int16_t)(back_node->x + 3), back_y = (int16_t)(back_node->y + 3);
        dgx_ui_pointer(&home, &(dgx_ui_pointer_t){.x = back_x, .y = back_y, .down = true, .pressed = true, .ms = 100});
        dgx_ui_pointer(&home, &(dgx_ui_pointer_t){.x = back_x, .y = back_y, .released = true, .ms = 150});
        const bool b2 = !home.popup;
        dgx_ui_paint(&home, &pp, 4);
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) ink2 += dgx_get_pixel(scr, x, y) == tb[INK];
        printf("pages: a page opened before the first pass %d, laid %dx%d; opened %d, \"about\" by its button %d, stacked %d; under the settings the clock's new "
               "text is not drawn (ink %d, %d); the key back %d: the settings on top %d; the page's button: the clock on top %d, drawn with the text (ink %d); "
               "settings_save() called %d and %d times; back on the clock %d, dgx_ui_page_back() %d\n",
               opened_before_pass, root_w, root_h, o1, o2, stacked, ink0, ink1, b1, settings_top, b2, ink2, saved_first, saves - saves_before,
               dgx_ui_key(&home, DGX_UI_KEY_BACK), dgx_ui_page_back(&home));
        dgx_ui_paint_free(&pp), dgx_ui_free(&home), dgx_ui_free(&page_settings), dgx_ui_free(&page_about);
        scr->destroy(&scr);
    }
    /* a graph that sweeps: a column a value written alone; whole after a popup */
    {
        static dgx_ui_node_t tn[2], tp[1];
        static dgx_ui_t      t, tpop;
        static const dgx_ui_widget_t tp_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN};
        panel_t                     *panel = panel_make();
        static uint32_t              c24[COLORS];
        for (int k = 0; k < COLORS; ++k) c24[k] = dgx_screen_rgb(&panel->sbus.scr, (uint8_t)(40 * k), (uint8_t)(250 - 30 * k), (uint8_t)(20 * k));
        dgx_ui_paint_t pp = {.scr = &panel->sbus.scr, .colors = c24, .color_count = COLORS, .background = PAPER};
        dgx_ui_init(&t, tn, 2, 1);
        const int top = dgx_ui_add(&t, -1, &col_w), graph = dgx_ui_add(&t, top, &trace_w);
        dgx_ui_paint(&t, &pp, 0);
        const dgx_ui_node_t *n = dgx_ui_node(&t, graph);
        for (int k = 0; k < 5; ++k) trace_add(500), dgx_ui_paint(&t, &pp, 1 + k);
        panel_count_from_now(panel);
        trace_add(1000), dgx_ui_paint(&t, &pp, 10);
        const int one = panel_written(panel), tw = panel_twice(panel);
        dgx_ui_init(&tpop, tp, 1, 1), dgx_ui_add(&tpop, -1, &tp_w);
        dgx_ui_popup_open(&t, &tpop, 0, 0, 40, 30, 0), dgx_ui_paint(&t, &pp, 11);
        dgx_ui_popup_close(&t), dgx_ui_paint(&t, &pp, 12);
        int right = 1;
        for (int k = 0; k < 8; ++k) {
            const int h = k < 5 ? 10 : k == 5 ? 20 : 0;
            right &= (h ? panel->mem[n->y + n->h - h][n->x + k] == c24[ACCENT] : 1) && panel->mem[n->y + n->h - h - 1][n->x + k] == c24[PAPER];
        }
        printf("sweep: its box %dx%d; the sixth value: %d pixels written (a column is %d), twice %d; a popup opened and closed over it: the columns right %d\n",
               n->w, n->h, one, n->h, tw, right);
        dgx_ui_paint_free(&pp), dgx_ui_free(&t), dgx_ui_free(&tpop);
        free(panel);
    }
    /* a graph in a canvas: a column a value, only the new one drawn; a popup opened and closed puts it again; full, it
       begins again */
    {
        static dgx_ui_node_t gn[2], pn2[1];
        static dgx_ui_t      g, gp;
        static const dgx_ui_widget_t gp_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN};
        dgx_screen_t                *scr  = dgx_vscreen_init(160, 60, 16, DgxScreenRGB);
        static uint32_t              tb[COLORS];
        for (int k = 0; k < COLORS; ++k) tb[k] = dgx_screen_rgb(scr, (uint8_t)(30 * k), (uint8_t)(250 - 30 * k), (uint8_t)(10 + 25 * k));
        dgx_ui_paint_t pp = {.scr = scr, .colors = tb, .color_count = COLORS, .background = PAPER};
        dgx_ui_init(&g, gn, 2, 2);
        const int top = dgx_ui_add(&g, -1, &col_w), graph = dgx_ui_add(&g, top, &history_w);
        dgx_ui_paint(&g, &pp, 0);
        const dgx_ui_node_t *n = dgx_ui_node(&g, graph);
        history_add(1000), dgx_ui_paint(&g, &pp, 1); /* 20 pixels high */
        history_add(500), dgx_ui_paint(&g, &pp, 2);  /* 10 */
        const int bottom = n->y + n->h - 1;
        printf("graph: its box %dx%d; column 0 is 20 high %d, column 1 is 10 high %d, column 2 paper %d\n", n->w, n->h,
               dgx_get_pixel(scr, n->x, bottom - 19) == tb[ACCENT] && dgx_get_pixel(scr, n->x, bottom - 20) == tb[PAPER],
               dgx_get_pixel(scr, n->x + 1, bottom - 9) == tb[ACCENT] && dgx_get_pixel(scr, n->x + 1, bottom - 10) == tb[PAPER],
               dgx_get_pixel(scr, n->x + 2, bottom) == tb[PAPER]);
        dgx_ui_init(&gp, pn2, 1, 1), dgx_ui_add(&gp, -1, &gp_w);
        dgx_ui_popup_open(&g, &gp, 0, 0, 80, 60, 0), dgx_ui_paint(&g, &pp, 3);
        const bool hidden = dgx_get_pixel(scr, n->x, bottom) != tb[ACCENT];
        dgx_ui_popup_close(&g), dgx_ui_paint(&g, &pp, 4);
        printf("graph: under a popup its column is hidden %d; the popup closed, the columns are there again %d\n", hidden,
               dgx_get_pixel(scr, n->x, bottom - 19) == tb[ACCENT] && dgx_get_pixel(scr, n->x + 1, bottom - 9) == tb[ACCENT]);
        for (int k = 2; k < HISTORY; ++k) history_add(100);
        dgx_ui_paint(&g, &pp, 5);
        history_add(2000), dgx_ui_paint(&g, &pp, 6); /* full: begun again with one column of 40 */
        printf("graph: full and one more value: begun again, column 0 is 40 high %d, column 1 paper %d, the widget's value %d\n",
               dgx_get_pixel(scr, n->x, bottom - 39) == tb[ACCENT], dgx_get_pixel(scr, n->x + 1, bottom) == tb[PAPER], dgx_ui_value_of(n));
        dgx_ui_paint_free(&pp), dgx_ui_free(&g), dgx_ui_free(&gp);
        scr->destroy(&scr);
    }
    /* a picture under the widgets: the map between the letters of a label, a frame's fill, the label changed, the map
       swapped */
    {
        static dgx_ui_node_t mn[4];
        static dgx_ui_t      m;
        static char          wind_buf[16], in_buf[8];
        static dgx_ui_textbox_t wind_box, in_box;
        static const dgx_ui_widget_t wind_w  = {.kind = DGX_UI_LABEL, .text = &wind_box, .want_h = 14};
        static const dgx_ui_widget_t panel_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2, .want_h = 20};
        static const dgx_ui_widget_t in_w    = {.kind = DGX_UI_LABEL, .text = &in_box, .want_h = 14};
        dgx_screen_t                *scr = dgx_vscreen_init(320, 240, 16, DgxScreenRGB);
        static uint32_t              tb[COLORS];
        for (int k = 0; k < COLORS; ++k) tb[k] = dgx_screen_rgb(scr, (uint8_t)(200 - 20 * k), (uint8_t)(30 * k), (uint8_t)(250 - 30 * k));
        dgx_ui_paint_t pp = {.scr = scr, .colors = tb, .color_count = COLORS, .background = PAPER};
        dgx_ui_textbox_init(&wind_box, wind_buf, sizeof wind_buf, 1, &st_left);
        dgx_ui_textbox_init(&in_box, in_buf, sizeof in_buf, 1, &st_left);
        dgx_ui_init(&m, mn, 4, 1);
        const int top = dgx_ui_add(&m, -1, &col_w), wind = dgx_ui_add(&m, top, &wind_w), fr = dgx_ui_add(&m, top, &panel_w);
        dgx_ui_text_set(&m, dgx_ui_add(&m, fr, &in_w), "+4 C");
        dgx_ui_text_set(&m, wind, "5 m/s");
        const bool set = map_under(&pp);
        dgx_ui_paint(&m, &pp, 0);
        const dgx_ui_node_t *wn = dgx_ui_node(&m, wind), *fn = dgx_ui_node(&m, fr);
        const uint32_t       map0 = dgx_get_pixel(map_screen, 0, 0);
        int                  ink = 0, map_px = 0, other = 0;
        for (int y = wn->y; y < wn->y + wn->h; ++y)
            for (int x = wn->x; x < wn->x + wn->w; ++x) {
                const uint32_t c = dgx_get_pixel(scr, x, y);
                c == tb[INK] ? ++ink : c == map0 ? ++map_px : ++other;
            }
        printf("map: set %d; the label's box: %d pixels of ink, %d of the map, %d of anything else (the paper would be one); the corner of the screen is the map %d, "
               "the inside of the frame its fill %d\n",
               set, ink, map_px, other, dgx_get_pixel(scr, 319, 239) == map0, dgx_get_pixel(scr, fn->x + fn->w - 4, fn->y + 4) == tb[2]);
        map_rain(&m, &pp);
        const uint32_t rain = dgx_get_pixel(map_screen, 0, 0);
        printf("map: swapped and told: the corner is the rain's colour %d (another than the map's %d), the label's paper the rain's %d, the frame still its fill %d\n",
               dgx_get_pixel(scr, 319, 239) == rain, rain != map0, dgx_get_pixel(scr, wn->x + wn->w - 2, wn->y + 1) == rain,
               dgx_get_pixel(scr, fn->x + fn->w - 4, fn->y + 4) == tb[2]);
        dgx_ui_background_set(&pp, NULL, NULL), dgx_ui_redraw(&m, 0), dgx_ui_paint(&m, &pp, 1);
        printf("map: taken away and the tree redrawn: the corner is the paper %d\n", dgx_get_pixel(scr, 319, 239) == tb[PAPER]);
        dgx_ui_paint_free(&pp), dgx_ui_free(&m);
        scr->destroy(&scr);
    }
    /* a display of 24 bits: the thermometer's label from 21.5 to 21.6, straight and through the strips */
    {
        static dgx_ui_node_t tn[2];
        static dgx_ui_t      t;
        static char          temp_buf[16];
        static dgx_ui_textbox_t temp_box;
        static const dgx_ui_widget_t temp_w = {.kind = DGX_UI_LABEL, .text = &temp_box, .want_h = 14};
        panel_t             *panel = panel_make();
        static uint32_t      c24[COLORS];
        for (int k = 0; k < COLORS; ++k) c24[k] = dgx_screen_rgb(&panel->sbus.scr, (uint8_t)(40 * k), (uint8_t)(250 - 30 * k), (uint8_t)(20 * k));
        dgx_ui_paint_t pp = {.scr = &panel->sbus.scr, .colors = c24, .color_count = COLORS, .background = PAPER};
        dgx_ui_textbox_init(&temp_box, temp_buf, sizeof temp_buf, 1, &st_left);
        dgx_ui_init(&t, tn, 2, 1);
        const int top = dgx_ui_add(&t, -1, &col_w), temp = dgx_ui_add(&t, top, &temp_w);
        dgx_ui_text_set(&t, temp, "21.5 C"), dgx_ui_paint(&t, &pp, 0);
        panel_count_from_now(panel);
        dgx_ui_text_set(&t, temp, "21.6 C"), dgx_ui_paint(&t, &pp, 1);
        const int straight = panel_twice(panel);
        deep_paint_init(&pp);
        dgx_ui_text_set(&t, temp, "21.5 C"), dgx_ui_paint(&t, &pp, 2);
        panel_count_from_now(panel);
        dgx_ui_text_set(&t, temp, "21.6 C"), dgx_ui_paint(&t, &pp, 3);
        const dgx_ui_node_t *n = dgx_ui_node(&t, temp);
        printf("24 bits: the label changed, drawn straight: %d pixels written twice; through the strips: %d twice, %d written, its box is %d, in %d window(s)\n",
               straight, panel_twice(panel), panel_written(panel), n->w * n->h, panel->windows);
        dgx_ui_paint_free(&pp), dgx_ui_free(&t);
        free(panel);
    }
    /* the battery of 4 bits: its charge in the colour its map names, another map for the other state; no buffer, paper */
    {
        static dgx_ui_node_t bn[2];
        static dgx_ui_t      b;
        dgx_screen_t        *scr = dgx_vscreen_init(64, 32, 16, DgxScreenRGB);
        static uint32_t      tb[COLORS];
        for (int k = 0; k < COLORS; ++k) tb[k] = dgx_screen_rgb(scr, (uint8_t)(30 * k), (uint8_t)(255 - 30 * k), (uint8_t)(17 * k));
        dgx_ui_paint_t pp = {.scr = scr, .colors = tb, .color_count = COLORS, .background = PAPER};
        dgx_ui_init(&b, bn, 2, 1);
        const int top = dgx_ui_add(&b, -1, &col_w), bat = dgx_ui_add(&b, top, &battery_w);
        dgx_ui_paint(&b, &pp, 0);
        const dgx_ui_node_t *n  = dgx_ui_node(&b, bat);
        const int            cx = n->x + (n->w - 12) / 2 + 2, cy = n->y + (n->h - 6) / 2 + 2; /* a pixel of the charge, value 2 */
        const bool           paper_without = dgx_get_pixel(scr, cx, cy) == tb[PAPER];
        paint_buffer_give(&pp);
        dgx_ui_redraw(&b, bat), dgx_ui_paint(&b, &pp, 1);
        const bool ok = dgx_get_pixel(scr, cx, cy) == tb[ACCENT] && dgx_get_pixel(scr, cx - 2, cy) == tb[INK] && dgx_get_pixel(scr, cx + 6, cy) == tb[FILL];
        dgx_ui_set_value(&b, bat, 1), dgx_ui_paint(&b, &pp, 2);
        printf("battery: without a buffer paper %d; with it the case, the charge and the rest in their colours %d; the other map: the charge of the low colour %d, a "
               "hole at the corner paper %d\n",
               paper_without, ok, dgx_get_pixel(scr, cx, cy) == tb[ACCENT_DOWN], dgx_get_pixel(scr, n->x + (n->w - 12) / 2 + 11, n->y + (n->h - 6) / 2) == tb[PAPER]);
        dgx_ui_paint_free(&pp), dgx_ui_free(&b);
        scr->destroy(&scr);
    }
    /* the dropdown: clicked, its list under it; a tap chooses and tells once; outside, the option it had, a slide and
       BACK tell nothing; the keys open it and choose */
    {
        static dgx_ui_node_t dn[4];
        static dgx_ui_t      d;
        dgx_ui_init(&d, dn, 4, 1);
        const int  top = dgx_ui_add(&d, -1, &col_w), fan = fan_add(&d, top);
        const bool early = dgx_ui_dropdown_open(&d, fan, &fan_list, NULL, NULL); /* the screen not known yet */
        dgx_ui_screen(&d, 200, 100, 0), dgx_ui_layout(&d);
        const dgx_ui_node_t *n = dgx_ui_node(&d, fan);
#define TAP(X, Y, MS)                                                                                                    \
    dgx_ui_pointer(&d, &(dgx_ui_pointer_t){.x = (int16_t)(X), .y = (int16_t)(Y), .down = true, .pressed = true, .ms = (MS)}), \
        dgx_ui_pointer(&d, &(dgx_ui_pointer_t){.x = (int16_t)(X), .y = (int16_t)(Y), .released = true, .ms = (MS) + 40})
        TAP(n->x + 5, n->y + 5, 100);
        const dgx_ui_rect_t r  = fan_list.tree.root_rect;
        const int           rh = (r.h - 2) / 3;
        printf("dropdown: before the screen is known opened %d; at %d,%d %dx%d, clicked: its list at %d,%d %dx%d, rows of %d, the pick %d\n", early, n->x, n->y,
               n->w, n->h, r.x, r.y, r.w, r.h, rh, dgx_ui_value(&fan_list.tree, 1));
        TAP(r.x + 5, r.y + 1 + 2 * rh + rh / 2, 200);
        printf("dropdown: a tap on High: the value %d, fan_set(%d) called %d time(s), the list open %d\n", dgx_ui_value_of(n), fan_speed, fans, d.popup != NULL);
        TAP(n->x + 5, n->y + 5, 300);
        TAP(n->x + 5, 95, 400); /* outside the list */
        const int after_outside = fans, open_outside = d.popup != NULL;
        TAP(n->x + 5, n->y + 5, 500);
        TAP(r.x + 5, r.y + 1 + 2 * rh + rh / 2, 600); /* High, the option it has */
        printf("dropdown: a tap outside: open %d, fan_set() called %d time(s) in all; a tap on the option it had: open %d, called %d\n", open_outside,
               after_outside, d.popup != NULL, fans);
        /* a finger moved along the list, let go */
        TAP(n->x + 5, n->y + 5, 700);
        dgx_ui_pointer(&d, &(dgx_ui_pointer_t){.x = (int16_t)(r.x + 5), .y = (int16_t)(r.y + 1 + rh / 2), .down = true, .pressed = true, .ms = 800});
        dgx_ui_pointer(&d, &(dgx_ui_pointer_t){.x = (int16_t)(r.x + 5), .y = (int16_t)(r.y + 1 + rh + rh / 2), .down = true, .ms = 840});
        const int slid = dgx_ui_value(&fan_list.tree, 1);
        dgx_ui_pointer(&d, &(dgx_ui_pointer_t){.x = (int16_t)(r.x + 5), .y = (int16_t)(r.y + 1 + rh + rh / 2), .released = true, .ms = 880});
        printf("dropdown: down on Low and moved to Medium: the list's pick %d; let go: the list open %d, the value %d, called %d\n", slid, d.popup != NULL,
               dgx_ui_value_of(n), fans);
        /* BACK is the program's: it closes the list, the value kept */
        const bool back_taken = dgx_ui_key(&d, DGX_UI_KEY_BACK);
        dgx_ui_popup_close(&d);
        printf("dropdown: BACK taken by a widget %d; closed by the program: the value %d, called %d\n", back_taken, dgx_ui_value_of(n), fans);
        /* the keys: the key that activates opens the list, previous moves the pick, the key that activates chooses */
        dgx_ui_focus(&d, fan);
        dgx_ui_key(&d, DGX_UI_KEY_ACTIVATE);
        const bool by_key = d.popup == &fan_list.tree;
        dgx_ui_key(&d, DGX_UI_KEY_PREV), dgx_ui_key(&d, DGX_UI_KEY_ACTIVATE);
        printf("dropdown: opened by the key %d; PREV and the key that activates: the value %d, fan_set(%d), called %d, open %d\n", by_key,
               dgx_ui_value_of(n), fan_speed, fans, d.popup != NULL);
#undef TAP
        dgx_ui_free(&d);
    }
    /* the strip over the keypad: a code of 123 touched, 9 typed and cancelled, then typed and done; a PIN in stars */
    {
        static dgx_ui_node_t fn[4];
        static dgx_ui_t      f;
        dgx_screen_t        *scr = dgx_vscreen_init(320, 240, 16, DgxScreenRGB);
        dgx_ui_paint_t       pp  = {.scr = scr, .colors = table, .color_count = COLORS, .background = PAPER};
        dgx_ui_textbox_init(&code_box, code_buf, sizeof code_buf, 1, &st_left);
        dgx_ui_textbox_init(&pin_box, pin_buf, sizeof pin_buf, 1, &st_left);
        dgx_ui_init(&f, fn, 4, 1);
        const int top = dgx_ui_add(&f, -1, &col_w), code = dgx_ui_add(&f, top, &code_w), pin = dgx_ui_add(&f, top, &pin_w);
        dgx_ui_screen(&f, 320, 240, 0);
        dgx_ui_text_set(&f, code, "123");
        edit_ui = &f, edit_scr = scr, edit_mm_w = 58, edit_mm_h = 43;
        dgx_ui_layout(&f);
        const dgx_ui_node_t *cn = dgx_ui_node(&f, code), *pn = dgx_ui_node(&f, pin);
        const dgx_ui_pointer_t down = {.x = (int16_t)(cn->x + 5), .y = (int16_t)(cn->y + 5), .down = true, .pressed = true, .ms = 10};
        const dgx_ui_pointer_t up   = {.x = down.x, .y = down.y, .released = true, .ms = 60};
        dgx_ui_pointer(&f, &down), dgx_ui_pointer(&f, &up);
        keyboard_cover(&pp, &keypad_kb), dgx_ui_paint(&f, &pp, 100);
        const bool     opened = f.popup == &strip_ui && keypad_kb.open, pressed = cn->state.bits & DGX_UI_PRESSED;
        const uint32_t nine[] = {'9'}, cancel[] = {DGX_UI_CP_CANCEL}, done[] = {DGX_UI_CP_DONE};
        dgx_ui_typed(&f, nine, 1);
        printf("strip: clicked, open %d, the field pressed %d; 9 typed: the strip \"%s\", the field \"%s\"\n", opened, pressed, edit_buf, code_buf);
        dgx_ui_typed(&f, cancel, 1);
        keyboard_cover(&pp, &keypad_kb), dgx_ui_paint(&f, &pp, 200);
        printf("strip: cancel: open %d, the field \"%s\", the focus on it %d\n", f.popup != NULL || keypad_kb.open, code_buf, f.focus == code);
        dgx_ui_pointer(&f, &down), dgx_ui_pointer(&f, &up);
        dgx_ui_typed(&f, nine, 1), dgx_ui_typed(&f, done, 1);
        printf("strip: clicked again, 9 and done: open %d, the field \"%s\"\n", f.popup != NULL || keypad_kb.open, code_buf);
        /* a swipe that begins on the field opens nothing */
        const dgx_ui_pointer_t swiped = {.x = (int16_t)(cn->x + 60), .y = down.y, .down = true, .swipe = true, .ms = 80};
        dgx_ui_pointer(&f, &down), dgx_ui_pointer(&f, &swiped), dgx_ui_pointer(&f, &(dgx_ui_pointer_t){.x = swiped.x, .y = swiped.y, .released = true, .ms = 120});
        printf("strip: a swipe begun on the field: open %d\n", f.popup != NULL || keypad_kb.open);
        /* the keys: the walk goes on from the code to the PIN and opens nothing; the key that activates opens the strip */
        dgx_ui_buttons(&f, DGX_UI_GAMEPAD_DOWN, 1000), dgx_ui_buttons(&f, 0, 1010);
        const bool walked = f.focus == pin && !f.popup;
        dgx_ui_buttons(&f, DGX_UI_GAMEPAD_A, 1020), dgx_ui_buttons(&f, 0, 1030);
        printf("strip: DOWN from the code: the focus on the PIN and nothing open %d; A on it: open %d\n", walked, f.popup == &strip_ui);
        /* a button of the board that means done: after dgx_ui_buttons() got it, the strip stays closed at the next tick */
        dgx_ui_buttons(&f, DGX_UI_GAMEPAD_A, 1100), edit_close(true);
        dgx_ui_buttons(&f, DGX_UI_GAMEPAD_A, 1115);
        const bool stays = !f.popup;
        dgx_ui_buttons(&f, 0, 1130);
        /* and instead of it: the button is a new press at the next tick, and the strip opens again */
        dgx_ui_buttons(&f, DGX_UI_GAMEPAD_A, 1200), dgx_ui_buttons(&f, 0, 1210); /* open */
        edit_close(true);                                                    /* the program's own A, dgx_ui_buttons() not called */
        dgx_ui_buttons(&f, DGX_UI_GAMEPAD_A, 1315);
        printf("strip: done by a button after dgx_ui_buttons(): stays closed %d; instead of it: open again %d\n", stays, f.popup == &strip_ui);
        dgx_ui_buttons(&f, 0, 1330), edit_close(false);
        /* the PIN: the strip in the secret look, the letters kept */
        const dgx_ui_pointer_t pd = {.x = (int16_t)(pn->x + 5), .y = (int16_t)(pn->y + 5), .down = true, .pressed = true, .ms = 2000};
        dgx_ui_pointer(&f, &pd), dgx_ui_pointer(&f, &(dgx_ui_pointer_t){.x = pd.x, .y = pd.y, .released = true, .ms = 2050});
        const bool     secret = f.popup == &strip_ui && dgx_ui_widget(&strip_ui, 1)->look == &look_pin;
        const uint32_t four[] = {'4', '2', DGX_UI_CP_DONE};
        dgx_ui_typed(&f, four, 3);
        keyboard_cover(&pp, &keypad_kb), dgx_ui_paint(&f, &pp, 400);
        printf("strip: the PIN's strip is secret %d; 4, 2 and done: the PIN's text box holds \"%s\"\n", secret, pin_buf);
        dgx_ui_paint_free(&pp), dgx_ui_free(&f);
        scr->destroy(&scr);
    }
    ring_pictures(argc > 1 ? argv[1] : NULL);
    /* a page that works only while it is seen: the ticker's stub says whether it runs */
    {
        static dgx_ui_node_t         air_home_nodes[2], air_page_nodes[2], air_about_nodes[2], air_ask_nodes[2];
        static const dgx_ui_widget_t plain_root_w = {.kind = DGX_UI_FRAME};
        dgx_ui_t                     air_home, air_page, air_about, air_ask;
        dgx_ui_init(&air_home, air_home_nodes, 2, 1), dgx_ui_add(&air_home, -1, &plain_root_w);
        dgx_ui_init(&air_page, air_page_nodes, 2, 1), dgx_ui_add(&air_page, -1, &air_root_w);
        dgx_ui_init(&air_about, air_about_nodes, 2, 1), dgx_ui_add(&air_about, -1, &plain_root_w);
        dgx_ui_init(&air_ask, air_ask_nodes, 2, 1), dgx_ui_add(&air_ask, -1, &plain_root_w);
        ticker_running = 0, ticker_what = -1, ticker_ms = 0;
        dgx_ui_page_open(&air_home, &air_page); /* step 1 */
        const int at_open = ticker_running, poll_asked = ticker_what == MSG_AIR_POLL, every_ms = ticker_ms;
        dgx_ui_popup_open(&air_page, &air_ask, 10, 10, 60, 30, DGX_UI_POPUP_MODAL); /* step 2: a dialog */
        const int under_a_dialog = ticker_running;
        dgx_ui_popup_close(&air_page);
        const int dialog_closed = ticker_running;
        dgx_ui_page_open(&air_home, &air_about); /* step 3: a second page */
        const int under_a_page = ticker_running;
        dgx_ui_page_back(&air_home); /* step 4 */
        const int shown_again = ticker_running;
        dgx_ui_page_back(&air_home); /* step 5 */
        const int          closed = ticker_running;
        const dgx_ui_msg_t tick   = {.what = MSG_AIR_POLL};
        air_message(&tick);
        printf("air: the ticker runs: the page opened %d (its message the poll's %d, every %d ms), a dialog over it %d, the dialog closed %d, a page over it %d, shown "
               "again %d, the page closed %d; a tick asks the sensor %d time(s)\n",
               at_open, poll_asked, every_ms, under_a_dialog, dialog_closed, under_a_page, shown_again, closed, air_reads);
        dgx_ui_free(&air_ask), dgx_ui_free(&air_about), dgx_ui_free(&air_page), dgx_ui_free(&air_home);
    }

    /* a level bar: the colour by the value's band, the tank filled from the bottom, and only the part that changed written */
    {
        static dgx_ui_node_t         level_nodes[3];
        static const dgx_ui_widget_t level_root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .gap = 2};
        static uint32_t              level_table[COLORS];
        dgx_ui_t                     levels;
        panel_t                     *panel = panel_make();
        for (int k = 0; k < COLORS; ++k) level_table[k] = dgx_screen_rgb(&panel->sbus.scr, (uint8_t)(40 * k), (uint8_t)(250 - 30 * k), (uint8_t)(20 * k));
        dgx_ui_paint_t level_paint = {.scr = &panel->sbus.scr, .colors = level_table, .color_count = COLORS, .background = PAPER};
        dgx_ui_init(&levels, level_nodes, 3, 1);
        dgx_ui_add(&levels, -1, &level_root_w);
        const int charge = dgx_ui_add(&levels, 0, &charge_w), tank = dgx_ui_add(&levels, 0, &tank_w);
        static const int values[5]                      = {10, 20, 30, 50, 80};
        static const int wanted[5]                      = {ACCENT_DOWN, BORDER, BORDER, ACCENT, ACCENT};
        int              right_colors = 0, right_lengths = 0;
        const dgx_ui_node_t *bar = NULL;
        for (int k = 0; k < 5; ++k) {
            charge_percent = values[k];
            dgx_ui_paint(&levels, &level_paint, (uint32_t)k);
            bar = dgx_ui_node(&levels, charge);
            int filled_columns = 0;
            for (int x = 0; x < bar->w; ++x) filled_columns += panel->mem[bar->y][bar->x + x] != level_table[FILL];
            right_colors += panel->mem[bar->y][bar->x] == level_table[wanted[k]];
            right_lengths += filled_columns == bar->w * values[k] / 100;
        }
        /* 80 to 81 of a hundred, the colour the same: the columns between the two lengths and no more */
        panel_count_from_now(panel);
        charge_percent = 81;
        dgx_ui_paint(&levels, &level_paint, 10);
        const int grown = panel_written(panel), grown_twice = panel_twice(panel), columns = bar->w * 81 / 100 - bar->w * 80 / 100;
        tank_litres = 100;
        dgx_ui_paint(&levels, &level_paint, 11);
        const dgx_ui_node_t *box = dgx_ui_node(&levels, tank);
        int                  filled_rows = 0, zone_rows = 0, below_zone_rows = 0;
        for (int y = 0; y < box->h; ++y) filled_rows += panel->mem[box->y + y][box->x] == level_table[ACCENT];
        /* full: the zone above 160 litres in its own colour, what is under it as it was */
        uint32_t half[PANEL_H];
        for (int y = 0; y < box->h; ++y) half[y] = panel->mem[box->y + y][box->x];
        panel_count_from_now(panel);
        tank_litres = 200;
        dgx_ui_paint(&levels, &level_paint, 12);
        const int full_written = panel_written(panel), full_twice = panel_twice(panel);
        for (int y = 0; y < box->h; ++y) zone_rows += panel->mem[box->y + y][box->x] == level_table[ACCENT_DOWN], below_zone_rows += panel->mem[box->y + y][box->x] == level_table[ACCENT];
        (void)half;
        printf("level: the tank full: %d rows of the zone above 160 litres, %d under it; 100 to 200 wrote %d pixels, %d row(s) of %d, %d twice\n", zone_rows, below_zone_rows,
               full_written, full_written / box->w, box->w, full_twice);
        tank_litres = 100;
        dgx_ui_paint(&levels, &level_paint, 13);
        printf("level: the charge at 10, 20, 30, 50, 80: the colour of its band %d of 5, the length right %d of 5; 80 to 81 in a bar of %d pixels: %d pixels written, "
               "%d column(s) of %d rows, %d twice; the tank %dx%d at 100 of 200: %d rows filled, the lowest filled %d, the top empty %d\n",
               right_colors, right_lengths, bar->w, grown, columns, bar->h, grown_twice, box->w, box->h, filled_rows,
               panel->mem[box->y + box->h - 1][box->x] == level_table[ACCENT], panel->mem[box->y][box->x] == level_table[FILL]);
        dgx_ui_paint_free(&level_paint), dgx_ui_free(&levels);
        free(panel);
    }

    /* a bar over the pages: the tabs open their pages in the place under the bar, the bar's back closes them */
    {
        static dgx_ui_node_t         tab_home_nodes[8], tab_air_nodes[2], tab_log_nodes[2];
        static const dgx_ui_widget_t tab_page_root_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2};
        dgx_ui_t                     bar_home;
        dgx_screen_t                *bar_screen = dgx_vscreen_init(200, 120, 16, DgxScreenRGB);
        dgx_ui_paint_t               bar_paint  = {.scr = bar_screen, .colors = table, .color_count = COLORS, .background = PAPER};
        dgx_ui_init(&bar_home, tab_home_nodes, 8, 1);
        dgx_ui_init(&tab_air, tab_air_nodes, 2, 1), dgx_ui_add(&tab_air, -1, &tab_page_root_w);
        dgx_ui_init(&tab_log, tab_log_nodes, 2, 1), dgx_ui_add(&tab_log, -1, &tab_page_root_w);
        tab_build(&bar_home);
        dgx_ui_paint(&bar_home, &bar_paint, 0);
        /* the widgets as tab_build() added them: the root, the bar, the clock, the two tabs, back, the place */
        const dgx_ui_node_t *air_tab = dgx_ui_node(&bar_home, 3), *log_tab = dgx_ui_node(&bar_home, 4), *back = dgx_ui_node(&bar_home, 5), *place = dgx_ui_node(&bar_home, 6);
        const dgx_ui_node_t *taps[3] = {air_tab, log_tab, back};
        int                  open_air = 0, open_log = 0, in_place = 0, closed = 0;
        for (int k = 0; k < 3; ++k) {
            const int16_t x = (int16_t)(taps[k]->x + taps[k]->w / 2), y = (int16_t)(taps[k]->y + taps[k]->h / 2);
            dgx_ui_pointer(&bar_home, &(dgx_ui_pointer_t){.x = x, .y = y, .down = true, .pressed = true, .ms = 1000u * (unsigned)k});
            dgx_ui_pointer(&bar_home, &(dgx_ui_pointer_t){.x = x, .y = y, .released = true, .ms = 1000u * (unsigned)k + 50});
            dgx_ui_paint(&bar_home, &bar_paint, 1000u * (unsigned)k + 100);
            const dgx_ui_t *page = bar_home.popup;
            if (k == 0) open_air = page == &tab_air;
            if (k == 1) open_log = page == &tab_log && !tab_air.level;
            if (k < 2 && page) {
                const dgx_ui_node_t *root = dgx_ui_node(page, 0);
                in_place += root->x == place->x && root->y == place->y && root->w == place->w && root->h == place->h;
            }
            if (k == 2) closed = !page;
        }
        printf("bar: the place under the bar at %d,%d %dx%d; a tap on Air opens its page %d, on Log the other in its stead %d, each over the place %d of 2; Back in the bar "
               "closes it %d\n",
               place->x, place->y, place->w, place->h, open_air, open_log, in_place, closed);
        dgx_ui_paint_free(&bar_paint), dgx_ui_free(&tab_log), dgx_ui_free(&tab_air), dgx_ui_free(&bar_home);
        dgx_screen_destroy(&bar_screen);
    }

    /* a button of a picture: a button, a toggle and two tabs in a row; their sizes, what a tap does, which picture shows */
    {
        static dgx_ui_node_t         icon_nodes[6];
        static const dgx_ui_widget_t icon_row_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .gap = 3, .pad = 2};
        dgx_ui_t                     icons;
        dgx_screen_t                *icon_screen = dgx_vscreen_init(100, 40, 16, DgxScreenRGB);
        dgx_ui_paint_t               icon_paint  = {.scr = icon_screen, .colors = table, .color_count = COLORS, .background = PAPER};
        dgx_ui_init(&icons, icon_nodes, 6, 1);
        dgx_ui_add(&icons, -1, &icon_row_w);
        const int back = dgx_ui_add(&icons, 0, &icon_back_w), sound = dgx_ui_add(&icons, 0, &icon_sound_w);
        const int tab_one = dgx_ui_add(&icons, 0, &icon_tab_w[0]), tab_two = dgx_ui_add(&icons, 0, &icon_tab_w[1]);
        icon_sound_on = 0, icon_tab_shown = 1, icon_backs = 0;
        dgx_ui_paint(&icons, &icon_paint, 0);
        const dgx_ui_node_t *nodes_of[4] = {dgx_ui_node(&icons, back), dgx_ui_node(&icons, sound), dgx_ui_node(&icons, tab_one), dgx_ui_node(&icons, tab_two)};
        int                  ink[4][3]; /* the ink in each box: at the start, after the taps, after the key on the toggle */
        int                  backs_told = 0, sound_after_tap = 0, tab_after_tap = 0;
        for (int round = 0; round < 3; ++round) {
            if (round == 1)
                for (int k = 0; k < 4; ++k) {
                    if (k == 2) continue; /* the first tab is not tapped: it loses the choice to the second */
                    const int16_t x = (int16_t)(nodes_of[k]->x + nodes_of[k]->w / 2), y = (int16_t)(nodes_of[k]->y + nodes_of[k]->h / 2);
                    dgx_ui_pointer(&icons, &(dgx_ui_pointer_t){.x = x, .y = y, .down = true, .pressed = true, .ms = 1000u * (unsigned)k});
                    dgx_ui_pointer(&icons, &(dgx_ui_pointer_t){.x = x, .y = y, .released = true, .ms = 1000u * (unsigned)k + 50});
                }
            if (round == 1) backs_told = icon_backs, sound_after_tap = icon_sound_on != 0, tab_after_tap = (int)icon_tab_shown;
            if (round == 2) dgx_ui_focus(&icons, sound), dgx_ui_key(&icons, DGX_UI_KEY_ACTIVATE); /* the toggle off again, by the key */
            dgx_ui_focus(&icons, -1); /* no frame, so that the ink in a box is the picture's alone */
            dgx_ui_paint(&icons, &icon_paint, 100u + (unsigned)round);
            for (int k = 0; k < 4; ++k) {
                ink[k][round] = 0;
                for (int y = nodes_of[k]->y; y < nodes_of[k]->y + nodes_of[k]->h; ++y)
                    for (int x = nodes_of[k]->x; x < nodes_of[k]->x + nodes_of[k]->w; ++x) ink[k][round] += dgx_get_pixel(icon_screen, x, y) == table[INK];
            }
        }
        printf("icons: each %dx%d, a picture of 8 and 5 points; the taps: back told %d time(s), the sound on %d, the tab shown %d; the ink of the back's picture %d %d %d, of the "
               "sound's %d (off) %d (on) %d (off again by the key, the place %d), of the tabs' %d and %d, then %d and %d\n",
               nodes_of[0]->w, nodes_of[0]->h, backs_told, sound_after_tap, tab_after_tap, ink[0][0], ink[0][1], ink[0][2], ink[1][0], ink[1][1], ink[1][2], (int)icon_sound_on,
               ink[2][0], ink[3][0], ink[2][1], ink[3][1]);
        dgx_ui_paint_free(&icon_paint), dgx_ui_free(&icons);
        dgx_screen_destroy(&icon_screen);
    }

    /* a stepper: a tap is a step; a finger held steps at the press and then every 100 ms from 500 ms on, to the end of the range */
    {
        static dgx_ui_node_t         step_nodes[6];
        static const dgx_ui_widget_t step_root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2};
        dgx_ui_t                     steps;
        dgx_screen_t                *step_screen = dgx_vscreen_init(120, 40, 16, DgxScreenRGB);
        dgx_ui_paint_t               step_paint  = {.scr = step_screen, .colors = table, .color_count = COLORS, .background = PAPER};
        dgx_ui_init(&steps, step_nodes, 6, 1);
        const int step_root = dgx_ui_add(&steps, -1, &step_root_w);
        step_minutes = 15;
        step_add(&steps, step_root);
        dgx_ui_paint(&steps, &step_paint, 0);
        /* the widgets as step_add() added them: the row, minus, the label, plus */
        const dgx_ui_node_t *minus = dgx_ui_node(&steps, 2), *plus = dgx_ui_node(&steps, 4);
        const int16_t        plus_x = (int16_t)(plus->x + plus->w / 2), plus_y = (int16_t)(plus->y + plus->h / 2);
        const int16_t        minus_x = (int16_t)(minus->x + minus->w / 2), minus_y = (int16_t)(minus->y + minus->h / 2);
        /* a tap on plus */
        dgx_ui_pointer(&steps, &(dgx_ui_pointer_t){.x = plus_x, .y = plus_y, .down = true, .pressed = true, .ms = 1000});
        const int at_press = (int)step_minutes;
        dgx_ui_pointer(&steps, &(dgx_ui_pointer_t){.x = plus_x, .y = plus_y, .released = true, .ms = 1050});
        dgx_ui_time(&steps, 1050);
        const int after_tap = (int)step_minutes;
        /* plus held for two seconds, the loop's step every 10 ms */
        dgx_ui_pointer(&steps, &(dgx_ui_pointer_t){.x = plus_x, .y = plus_y, .down = true, .pressed = true, .ms = 2000});
        for (uint32_t now = 2000; now <= 4000; now += 10) dgx_ui_time(&steps, now);
        const int      after_hold = (int)step_minutes;
        const uint32_t wait_held  = dgx_ui_wake(NULL, &steps, 4000);
        /* held on, past the end of the range */
        for (uint32_t now = 4010; now <= 6000; now += 10) dgx_ui_time(&steps, now);
        const int at_the_end = (int)step_minutes;
        dgx_ui_pointer(&steps, &(dgx_ui_pointer_t){.x = plus_x, .y = plus_y, .released = true, .ms = 6000});
        dgx_ui_time(&steps, 6000);
        const uint32_t wait_let_go = dgx_ui_wake(NULL, &steps, 6000);
        /* minus, without the loop's step: once at the press and no more */
        dgx_ui_pointer(&steps, &(dgx_ui_pointer_t){.x = minus_x, .y = minus_y, .down = true, .pressed = true, .ms = 7000});
        const int without_time = (int)step_minutes;
        dgx_ui_pointer(&steps, &(dgx_ui_pointer_t){.x = minus_x, .y = minus_y, .released = true, .ms = 9000});
        dgx_ui_paint(&steps, &step_paint, 9000);
        printf("stepper: from 15 a tap on plus: %d at the press, %d after the release; held for 2 s: %d (the press and 16 more of 5), the loop to wake in %u ms; held on: %d, "
               "the end of the range; let go: the wait %s; minus held 2 s with no dgx_ui_time(): %d; the label \"%s\"\n",
               at_press, after_tap, after_hold, (unsigned)wait_held, at_the_end, wait_let_go == DGX_UI_WAKE_NONE ? "none" : "SOME", without_time, step_buf);
        dgx_ui_paint_free(&step_paint), dgx_ui_free(&steps);
        dgx_screen_destroy(&step_screen);
    }

    /* the screen dimmed after 30 s with nobody and dark after 2 min, by a loop that sleeps what dgx_ui_wake() and the
       second stage say; a tap while dark is spent, a tap while dimmed presses; then a saver page by the root's take() */
    {
        static dgx_ui_node_t         rest_nodes[3], saver_home_nodes[3], saver_nodes[2];
        static const dgx_ui_widget_t rest_root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2};
        static const dgx_ui_widget_t saver_home_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .take = saver_take};
        static const dgx_ui_widget_t rest_button_w = {.kind = DGX_UI_BUTTON, .want_w = 40, .want_h = 14, .look = &dgx_ui_look_button};
        static const dgx_ui_widget_t saver_root_w  = {.kind = DGX_UI_BOX};
        dgx_ui_t                     rest_home, saver_home;
        dgx_screen_t                *rest_screen = dgx_vscreen_init(120, 60, 16, DgxScreenRGB);
        dgx_ui_paint_t               rest_paint  = {.scr = rest_screen, .colors = table, .color_count = COLORS, .background = PAPER};
        dgx_ui_init(&rest_home, rest_nodes, 3, 1);
        dgx_ui_add(&rest_home, -1, &rest_root_w);
        const int rest_button = dgx_ui_add(&rest_home, 0, &rest_button_w);
        backlight_percent = 80, backlight = -1;
        uint32_t now = 5000;
        dgx_ui_paint(&rest_home, &rest_paint, now);
        const dgx_ui_node_t *rest_place = dgx_ui_node(&rest_home, rest_button);
        const int16_t        rest_x = (int16_t)(rest_place->x + rest_place->w / 2), rest_y = (int16_t)(rest_place->y + rest_place->h / 2);
        const uint32_t       named_at = now;
        screen_idle_start(&rest_home, now);
        /* the loop of the recipe on a clock of its own: the time, the light, the pass, asleep for the least of the two waits */
        uint32_t dimmed_at = 0, dark_at = 0;
        int      turns = 0, dimmed_light = -1;
        for (; turns < 20; ++turns) {
            dgx_ui_time(&rest_home, now);
            const uint32_t to_dark = screen_light_step(now);
            dgx_ui_paint(&rest_home, &rest_paint, now);
            if (screen_light == SCREEN_DIMMED && !dimmed_at) dimmed_at = now, dimmed_light = backlight;
            if (screen_light == SCREEN_DARK) {
                dark_at = now;
                break;
            }
            uint32_t ms = dgx_ui_wake(&rest_paint, &rest_home, now);
            if (to_dark < ms) ms = to_dark;
            if (ms == DGX_UI_WAKE_NONE) break;
            now += ms ? ms : 1;
        }
        const int      dark_light  = backlight;
        const uint32_t dark_wait   = dgx_ui_wake(&rest_paint, &rest_home, now);
        /* a tap on the button while dark: spent */
        now += 60000;
        const bool dark_taken = dgx_ui_pointer(&rest_home, &(dgx_ui_pointer_t){.x = rest_x, .y = rest_y, .down = true, .pressed = true, .ms = now});
        const bool dark_press = dgx_ui_node(&rest_home, rest_button)->state.bits & DGX_UI_PRESSED;
        dgx_ui_pointer(&rest_home, &(dgx_ui_pointer_t){.x = rest_x, .y = rest_y, .released = true, .ms = now + 80});
        now += 100;
        dgx_ui_time(&rest_home, now), screen_light_step(now);
        const int lit_light = backlight;
        /* half a minute on: dimmed again; a tap then presses the button */
        now += dgx_ui_wake(&rest_paint, &rest_home, now);
        dgx_ui_time(&rest_home, now), screen_light_step(now);
        const int  dimmed_again = screen_light == SCREEN_DIMMED;
        dgx_ui_pointer(&rest_home, &(dgx_ui_pointer_t){.x = rest_x, .y = rest_y, .down = true, .pressed = true, .ms = now});
        const bool dimmed_press = dgx_ui_node(&rest_home, rest_button)->state.bits & DGX_UI_PRESSED;
        dgx_ui_pointer(&rest_home, &(dgx_ui_pointer_t){.x = rest_x, .y = rest_y, .released = true, .ms = now + 80});
        now += 100;
        dgx_ui_time(&rest_home, now), screen_light_step(now);
        printf("idle: named at 0, dimmed at %u ms to %d%% of 80, dark at %u ms to %d%%, in %d turns of the loop, then no wait (%s); a tap while dark: taken %d, the button pressed %d, "
               "the light back to %d%%; dimmed again %d, a tap then: the button pressed %d, the light %d%%\n",
               (unsigned)(dimmed_at - named_at), dimmed_light, (unsigned)(dark_at - named_at), dark_light, turns + 1, dark_wait == DGX_UI_WAKE_NONE ? "none" : "SOME", dark_taken, dark_press,
               lit_light, dimmed_again, dimmed_press, backlight);

        /* the saver: home's root opens a page at the 1 and closes it at the 0 */
        dgx_ui_init(&saver_home, saver_home_nodes, 3, 1), dgx_ui_init(&saver_page, saver_nodes, 2, 1);
        dgx_ui_add(&saver_home, -1, &saver_home_w);
        const int saver_button = dgx_ui_add(&saver_home, 0, &rest_button_w);
        dgx_ui_add(&saver_page, -1, &saver_root_w);
        dgx_ui_paint_free(&rest_paint);
        rest_paint = (dgx_ui_paint_t){.scr = rest_screen, .colors = table, .color_count = COLORS, .background = PAPER};
        now += 1000;
        dgx_ui_paint(&saver_home, &rest_paint, now);
        screen_idle.wake_acts = false;
        screen_idle_start(&saver_home, now);
        now += DIM_AFTER_MS;
        dgx_ui_time(&saver_home, now), dgx_ui_paint(&saver_home, &rest_paint, now);
        const bool saver_shown = saver_home.popup == &saver_page;
        const bool saver_taken = dgx_ui_pointer(&saver_home, &(dgx_ui_pointer_t){.x = rest_x, .y = rest_y, .down = true, .pressed = true, .ms = now + 500});
        dgx_ui_pointer(&saver_home, &(dgx_ui_pointer_t){.x = rest_x, .y = rest_y, .released = true, .ms = now + 580});
        now += 600;
        dgx_ui_time(&saver_home, now), dgx_ui_paint(&saver_home, &rest_paint, now);
        const bool saver_gone   = saver_home.popup == NULL;
        const bool woke_pressed = dgx_ui_node(&saver_home, saver_button)->state.bits & DGX_UI_PRESSED;
        dgx_ui_pointer(&saver_home, &(dgx_ui_pointer_t){.x = rest_x, .y = rest_y, .down = true, .pressed = true, .ms = now + 100});
        const bool next_pressed = dgx_ui_node(&saver_home, saver_button)->state.bits & DGX_UI_PRESSED;
        dgx_ui_pointer(&saver_home, &(dgx_ui_pointer_t){.x = rest_x, .y = rest_y, .released = true, .ms = now + 180});
        printf("saver: the page over home when the idle came %d; the tap that wakes it taken %d, the page gone %d, home's button pressed by it %d; the next tap presses it %d\n", saver_shown,
               saver_taken, saver_gone, woke_pressed, next_pressed);
        dgx_ui_idle(&saver_home, NULL, now);
        dgx_ui_paint_free(&rest_paint), dgx_ui_free(&saver_page), dgx_ui_free(&saver_home), dgx_ui_free(&rest_home);
        dgx_screen_destroy(&rest_screen);
    }

    /* a page from a description: loaded, its label found by the id and set, the slider's value the program's place, the
       page opened over a home and closed by its button's action; a description with a typo adds nothing and says where */
    {
        static dgx_ui_node_t         described_home_nodes[2];
        static const dgx_ui_widget_t described_home_w = {.kind = DGX_UI_BOX};
        dgx_ui_t                     described_home;
        dgx_screen_t                *described_screen = dgx_vscreen_init(160, 100, 16, DgxScreenRGB);
        dgx_ui_paint_t               described_paint  = {.scr = described_screen, .colors = table, .color_count = COLORS, .background = PAPER};
        dgx_ui_load_error_t          load_err         = {0};
        dgx_ui_init(&described_home, described_home_nodes, 2, 1);
        dgx_ui_add(&described_home, -1, &described_home_w);
        const bool loaded = described_page_load(&load_err);
        dgx_ui_paint(&described_home, &described_paint, 0);
        const bool opened = loaded && dgx_ui_page_open(&described_home, &described_page);
        dgx_ui_paint(&described_home, &described_paint, 10);
        /* the widgets as described: 0 the column, 1 the label, 2 the slider, 3 the switch, 4 the button */
        const char *title_text = loaded ? dgx_ui_widget(&described_page, described_title)->text->buf : "";
        dgx_ui_focus(&described_page, 2);
        dgx_ui_key(&described_home, DGX_UI_KEY_MORE);
        const int            volume_after = (int)described_volume;
        const dgx_ui_node_t *back         = dgx_ui_node(&described_page, 4);
        const int16_t        back_x = (int16_t)(back->x + back->w / 2), back_y = (int16_t)(back->y + back->h / 2);
        dgx_ui_pointer(&described_home, &(dgx_ui_pointer_t){.x = back_x, .y = back_y, .down = true, .pressed = true, .ms = 100});
        dgx_ui_pointer(&described_home, &(dgx_ui_pointer_t){.x = back_x, .y = back_y, .released = true, .ms = 150});
        const bool closed = described_home.popup == NULL;
        static const char   wrong[]   = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"sldier\"}}";
        dgx_ui_load_need_t  wrong_need;
        dgx_ui_load_error_t wrong_err = {0};
        const bool          wrong_ok  = dgx_ui_load_need(wrong, sizeof wrong - 1, &described_names, &wrong_need, &wrong_err);
        dgx_ui_load_need_t  told_need = {0};
        dgx_ui_load_error_t told_err  = {0};
        dgx_ui_load_need(described_json, sizeof described_json - 1, &described_names, &told_need, &told_err);
        printf("described: the tree named \"%s\", %d nodes, a depth of %d; ", told_need.name, told_need.nodes, told_need.depth);
        printf("described: loaded %d (error %d), the label at %d reads \"%s\", %d widgets; opened as a page %d; MORE on its slider: the program's volume %d (from 40 by 5); "
               "a tap on Back closes it %d; a kind \"sldier\": taken %d, code %d (KIND %d) at byte %d, \"%s\"\n",
               loaded, load_err.code, described_title, title_text, described_page.count, opened, volume_after, closed, wrong_ok, wrong_err.code, DGX_UI_LOAD_KIND, wrong_err.offset,
               wrong_err.name);
        dgx_ui_paint_free(&described_paint), dgx_ui_free(&described_page), dgx_ui_free(&described_home);
        free(described_memory);
        dgx_screen_destroy(&described_screen);
    }

    { /* a description made C when the program is built: the tree sound_add() adds beside the one the loader lays of the
         very file on the tables made of the names file */
        const bool   added = made_page_add();
        const int    made_count = made_page.count, made_title = room_title;
        char         made_text[40] = "";
        if (added) snprintf(made_text, sizeof made_text, "%s", dgx_ui_widget(&made_page, room_title)->text->buf);
        static dgx_ui_widget_t made_widgets[8];
        for (int k = 0; k < made_count && k < 8; ++k) made_widgets[k] = *dgx_ui_widget(&made_page, k);
        dgx_ui_free(&made_page);
        FILE  *file = fopen(MADE_DIR "/sound.json", "rb");
        char   json[2000];
        size_t len = file ? fread(json, 1, sizeof json, file) : 0;
        if (file) fclose(file);
        dgx_ui_load_need_t  made_need = {0};
        dgx_ui_load_error_t made_err  = {0};
        void               *made_memory = dgx_ui_load_need(json, len, &vocab_names, &made_need, &made_err) ? malloc(made_need.bytes) : NULL;
        const int           top = made_memory ? made_page_load(json, len, made_memory, made_need.bytes, &made_err) : -1;
        int                 same = top >= 0 && made_page.count == made_count && room_title == made_title;
        for (int k = 0; same && k < made_count; ++k) {
            const dgx_ui_widget_t *a = &made_widgets[k], *b = dgx_ui_widget(&made_page, k);
            same = a->kind == b->kind && a->flags == b->flags && a->pad == b->pad && a->gap == b->gap && a->want_w == b->want_w && a->want_h == b->want_h && a->min == b->min &&
                   a->max == b->max && a->step == b->step && a->value == b->value && a->look == b->look && a->on_event == b->on_event && !a->text == !b->text;
        }
        printf("made: sound_add() added %d (%d widgets), the label at %d reads \"%s\"; the same file loaded on vocab_names: the tree named \"%s\", error %d, the same "
               "widgets %d\n",
               added, made_count, made_title, made_text, made_need.name, made_err.code, same);
        dgx_ui_free(&made_page), free(made_memory);
    }

    { /* an appearance made C when the program is built: day_take() and the page of the last recipe made on it, which
         the checker makes under the prefix sound_day beside the page made with no appearance */
        dgx_screen_t  *day_screen = dgx_vscreen_init(64, 48, 16, DgxScreenRGB);
        dgx_ui_paint_t day_paint  = {.scr = day_screen};
        day_take(day_screen, &day_paint);
        static dgx_ui_t      day_page;
        static dgx_ui_node_t day_nodes[8];
        dgx_ui_init(&day_page, day_nodes, 8, 1);
        const int top = sound_day_add(&day_page, -1);
        int       of_the_appearance = 0, of_its_style = 0;
        for (int k = 0; top >= 0 && k < day_page.count; ++k) {
            const dgx_ui_widget_t *w = dgx_ui_widget(&day_page, k);
            of_the_appearance += (w->kind == DGX_UI_SLIDER && w->look == &day_look[0]) || (w->kind == DGX_UI_BUTTON && w->look == &day_look[1]);
            of_its_style += w->text && w->text->style == &day_style[0];
        }
        printf("appearance: day made C, %d colours, accent the index %d packed %s; its style's font %s with nothing called; the page on it: added %d, a slider and a button of its looks %d, "
               "texts of its style %d\n",
               day_appearance.color_count, (int)day_color_accent, day_colors[day_color_accent] == dgx_screen_rgb(day_screen, 0xf0, 0xa0, 0x3c) ? "right" : "WRONG",
               day_style[0].fonts[0].font == &dgx_font_body ? "there" : "NOT THERE", top >= 0, of_the_appearance, of_its_style);
        dgx_ui_free(&day_page), dgx_screen_destroy(&day_screen);
    }

    /* one tree on every board: the face laid on the CYD's 320x240 at the scale 1 and on 1280x800 at 3, its halves and
       their gap filling the row to the pixel, its frames' pad of points as many pixels as the scale */
    {
        static const int boards[2][3] = {{320, 240, 1}, {1280, 800, 3}};
        for (int b = 0; b < 2; ++b) {
            static dgx_ui_node_t face_nodes[12];
            dgx_ui_t             face_ui;
            dgx_ui_init(&face_ui, face_nodes, 12, boards[b][2]);
            dgx_ui_screen(&face_ui, boards[b][0], boards[b][1], 0);
            const int face = face_add(&face_ui, -1);
            dgx_ui_layout(&face_ui);
            const dgx_ui_node_t *whole = dgx_ui_node(&face_ui, face), *time = dgx_ui_node(&face_ui, face + 1), *row = dgx_ui_node(&face_ui, face + 2);
            const dgx_ui_node_t *left = dgx_ui_node(&face_ui, face + 3), *right = dgx_ui_node(&face_ui, face + 5), *warm = dgx_ui_node(&face_ui, face + 4);
            const dgx_ui_node_t *status = dgx_ui_node(&face_ui, face + 7);
            const int gap = right->x - (left->x + left->w);
            printf("face on %dx%d at %d: the time %dx%d, the halves %d and %d with a gap of %d in a row of %d (%s), a frame's pad %d, the status line %d high\n",
                   boards[b][0], boards[b][1], boards[b][2], time->w, time->h, left->w, right->w, gap, row->w,
                   left->w + gap + right->w == row->w && left->x == row->x ? "filled" : "NOT FILLED", warm->x - left->x, status->h);
            if (whole->w != boards[b][0]) printf("face: NOT the whole screen\n");
            dgx_ui_free(&face_ui);
        }
    }
    puts("cookbook: ran to the end");
    return 0;
}
