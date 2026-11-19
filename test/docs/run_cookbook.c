/* The recipes of the cookbook run: its C blocks, as the checker joined them, with the program's own functions stubbed. */
#include "docs_en_cookbook.md.c" /* written by check_docs.py into its build directory: give it with -I */
#include <stdio.h>
#include "dgx_vscreen.h"
#define PANEL_BITS 24 /* the model of a display behind a bus, of test/host: it counts the writes of each pixel */
#include "panel.h"

static int saves, backs, menus, backlight = -1, wifi = -1;
void settings_save(void) { ++saves; }
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
void wifi_connect(const char *password) { (void)password; }
const uint16_t logo_pixels[64 * 32];
const uint16_t map_pixels[320 * 240]  = {[0 ... 320 * 240 - 1] = 0x1084}; /* two maps of one colour each */
const uint16_t rain_pixels[320 * 240] = {[0 ... 320 * 240 - 1] = 0x1f00};
int64_t    esp_timer_get_time(void) { return 0; }
void       vTaskDelay(TickType_t t) { (void)t; }
BaseType_t xQueueSend(QueueHandle_t q, const void *i, TickType_t w) { return pdTRUE; }
bool dgx_ui_post(QueueHandle_t q, int what, const void *data, size_t size) { return true; }
bool dgx_ui_post_ref(QueueHandle_t q, int what, void *data, size_t size, void (*done)(void *, size_t)) { return true; }
bool dgx_ui_post_from_isr(QueueHandle_t q, int what, const void *data, size_t size, BaseType_t *woken) { return true; }
bool dgx_ui_ticker_start(dgx_ui_ticker_t *t, QueueHandle_t q, int what, uint32_t ms) { return true; }
int  dgx_di_pointer_read(dgx_di_pointer_t *p, uint32_t now) { return 0; }
bool dgx_di_pointer_first(const dgx_di_pointer_t *p, int *x, int *y) { *x = *y = 0; return false; }
void dgx_di_touch_take(dgx_di_touch_t *t, int x, int y, bool down, uint32_t now) {}
int  dgx_di_keyboard_take(dgx_di_keyboard_t *kb, dgx_di_pointer_t *p, uint32_t now, uint32_t *typed, int max) { return 0; }
/* the keyboard of the strip's recipe: its band the lower part of the display, from 36 pixels down, as on a CYD */
const uint8_t keypad[1];
const size_t  keypad_size = sizeof keypad;
bool dgx_di_keyboard_open(dgx_di_keyboard_t *kb, dgx_screen_t *d, int mm_w, int mm_h, const uint8_t *layout, size_t size, dgx_font_t *font,
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
    static uint32_t      table[COLORS];
    table[PAPER] = dgx_screen_rgb(scr, 12, 14, 20), table[INK] = dgx_screen_rgb(scr, 235, 235, 225);
    dgx_ui_paint_t paint = {.scr = scr, .colors = table, .color_count = COLORS, .background = PAPER};
    dgx_ui_init(&ui, nodes, 8, 1);
    const int ring = ring_add(&ui, scr);
    dgx_ui_msg_t    msg;
    const ppm_t     v = {.ppm = 1340};
    const climate_t c = {.t10 = 215, .rh = 41};
    dgx_ui_msg_value(&msg, MSG_PPM, &v, sizeof v), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    dgx_ui_msg_value(&msg, MSG_CLIMATE, &c, sizeof c), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    dgx_ui_paint(&ui, &paint, 0);
    shot(scr, dir, "ring.ppm");
    /* a second value: only the steps that changed and the number's field */
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
        static const dgx_ui_widget_t layer_slider_w = {.kind = DGX_UI_SLIDER, .max = 100, .want_h = 12, .look = &dgx_ui_look_slider};
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
        printf("own: down at 30,10: y %d, value %d, on() told %d time(s) the point %d,%d; pressed %d, focused %d, dragging %d\n", xy_data.y,
               pn->state_value.value, point_calls, point_x, point_y, !!(pn->state_value.bits & DGX_UI_PRESSED), x.focus == pad, dgx_ui_dragging(&x));
        dgx_ui_paint(&x, &xp, 10);
        int knob = 0, kx0 = 999, ky0 = 999, field = 0;
        for (int yy = 0; yy < pn->h; ++yy)
            for (int xx = 0; xx < pn->w; ++xx) {
                const uint32_t c = dgx_get_pixel(xs, pn->x + xx, pn->y + yy);
                if (c == table[ACCENT]) ++knob, kx0 = xx < kx0 ? xx : kx0, ky0 = yy < ky0 ? yy : ky0;
                else if (c == table[FILL]) ++field;
            }
        printf("own: drawn pressed: %d pixels of the knob from %d,%d, %d of the field, of %d\n", knob, kx0, ky0, field, pn->w * pn->h);
        /* step 2: a move out of the widget, up and to the left of it and under it */
        dgx_ui_pointer(&x, &(dgx_ui_pointer_t){.x = (int16_t)(pn->x - 10), .y = (int16_t)(pn->y + 70), .down = true, .ms = 1050});
        printf("own: moved to -10,70: y %d, value %d, dragging %d\n", xy_data.y, pn->state_value.value, dgx_ui_dragging(&x));
        /* step 3: let go: no click */
        point_calls = 0;
        dgx_ui_pointer(&x, &(dgx_ui_pointer_t){.x = (int16_t)(pn->x - 10), .y = (int16_t)(pn->y + 70), .released = true, .ms = 1100});
        printf("own: let go: pressed %d, on() told %d more time(s)\n", !!(pn->state_value.bits & DGX_UI_PRESSED), point_calls);
        /* step 4: the keys */
        const bool more = dgx_ui_key(&x, DGX_UI_KEY_MORE);
        printf("own: MORE taken %d, value %d; ACTIVATE taken %d; NEXT taken %d, the focus stays on the pad (nobody else takes it) %d\n", more,
               pn->state_value.value, dgx_ui_key(&x, DGX_UI_KEY_ACTIVATE), dgx_ui_key(&x, DGX_UI_KEY_NEXT), x.focus == pad);
        /* the bar that is tapped: pressed, the focus left on the pad */
        dgx_ui_pointer(&x, &(dgx_ui_pointer_t){.x = (int16_t)(tn->x + 1), .y = (int16_t)(tn->y + 1), .down = true, .pressed = true, .ms = 2000});
        printf("own: the bar pressed %d, the focus still on the pad %d\n", !!(tn->state_value.bits & DGX_UI_PRESSED), x.focus == pad);
        dgx_ui_pointer(&x, &(dgx_ui_pointer_t){.x = (int16_t)(tn->x + 1), .y = (int16_t)(tn->y + 1), .released = true, .ms = 2050});
        dgx_ui_free(&x);
        dgx_screen_destroy(&xs);
    }

    /* the icon: a message of 3 bars, then of none */
    const int   sig = dgx_ui_add(&ui, root, &signal_w);
    dgx_ui_msg_t msg;
    signal_t     s = {.bars = 3};
    dgx_ui_msg_value(&msg, MSG_SIGNAL, &s, sizeof s), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    const int v3 = dgx_ui_node(&ui, sig)->state_value.value;
    s.bars = 0;
    dgx_ui_msg_value(&msg, MSG_SIGNAL, &s, sizeof s), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    printf("icon: value %d after 3 bars, %d after none (outside 1..4: no picture); its box %dx%d\n", v3, dgx_ui_node(&ui, sig)->state_value.value,
           dgx_ui_node(&ui, sig)->w, dgx_ui_node(&ui, sig)->h);

    /* the slider and the switch by keys */
    const int sl = dgx_ui_add(&ui, root, &backlight_w), sw = dgx_ui_add(&ui, root, &wifi_w);
    dgx_ui_focus(&ui, sl), dgx_ui_key(&ui, DGX_UI_KEY_MORE);
    dgx_ui_focus(&ui, sw), dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE);
    backlight_show(&ui, sl, 30);
    printf("slider: started at 70, MORE told backlight_set(%d); set to 30 by the program, on() not told again (%d); switch told wifi_enable(%d)\n", backlight,
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
    buttons_t b = {.held = DGX_UI_PAD_DOWN};
    dgx_ui_msg_value(&msg, MSG_BUTTONS, &b, sizeof b), buttons_message(&ui, &msg, 100);
    const int f1 = ui.focus;
    b.held       = DGX_UI_PAD_BACK;
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
        const bool took_default = dgx_ui_buttons(&r, DGX_UI_PAD_RIGHT, 0);
        const int  f_default    = r.focus;
        dgx_ui_buttons(&r, 0, 10);
        dialog_keys(&r, true);
        const bool took_row = dgx_ui_buttons(&r, DGX_UI_PAD_RIGHT, 20);
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
        const int picked_down = n->state_value.value, modes_down = modes;
        dgx_ui_pointer(&l, &(dgx_ui_pointer_t){.x = x, .y = (int16_t)(y0 + 14 * 2 + 7), .released = true, .ms = 60});
        printf("list: %d high for 4 rows; a finger down on row 2 picks %d, mode_set() called %d time(s) yet; let go: mode %d\n", n->h, picked_down, modes_down, mode);
        dgx_ui_pointer(&l, &(dgx_ui_pointer_t){.x = x, .y = (int16_t)(y0 + 14), .down = true, .pressed = true, .ms = 100});
        const int at_edge = n->state_value.value;
        dgx_ui_pointer(&l, &(dgx_ui_pointer_t){.x = x, .y = (int16_t)(y0 + 14), .released = true, .ms = 150});
        dgx_ui_pointer(&l, &(dgx_ui_pointer_t){.x = x, .y = (int16_t)(y0 + 7), .down = true, .pressed = true, .ms = 200});
        dgx_ui_pointer(&l, &(dgx_ui_pointer_t){.x = x, .y = (int16_t)(y0 + 7 + 28), .down = true, .ms = 250});
        const int swiped = n->state_value.value, modes_before = modes;
        dgx_ui_pointer(&l, &(dgx_ui_pointer_t){.x = x, .y = (int16_t)(y0 + 7 + 28), .released = true, .ms = 300});
        printf("list: a finger on the line between rows 0 and 1 leaves the pick %d; down on row 0 and moved two rows: %d, no click %d\n", at_edge, swiped,
               modes == modes_before);
        dgx_ui_set_value(&l, list, 2);
        dgx_ui_key(&l, DGX_UI_KEY_NEXT);
        const int next1 = n->state_value.value, f1 = l.focus;
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
        st_news = (dgx_ui_text_style_t){.variants = small, .count = 1, .flags = DGX_UI_TEXT_IMPORTANT, .scroll_ms = 2000};
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
        static const dgx_ui_widget_t months_w = {.kind = DGX_UI_LIST, .text = &months_box, .max = 11, .want_h = 56, .look = &dgx_ui_look_list};
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
               "the pick still %d\n", at4, at11, months_box.top, dgx_ui_node(&s, list)->state_value.value);
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
        for (int k = 0; k < 5; ++k) trace_add(&t, graph, 500), dgx_ui_paint(&t, &pp, 1 + k);
        panel_count_from_now(panel);
        trace_add(&t, graph, 1000), dgx_ui_paint(&t, &pp, 10);
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
        history_add(&g, graph, 1000), dgx_ui_paint(&g, &pp, 1); /* 20 pixels high */
        history_add(&g, graph, 500), dgx_ui_paint(&g, &pp, 2);  /* 10 */
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
        for (int k = 2; k < HISTORY; ++k) history_add(&g, graph, 100);
        dgx_ui_paint(&g, &pp, 5);
        history_add(&g, graph, 2000), dgx_ui_paint(&g, &pp, 6); /* full: begun again with one column of 40 */
        printf("graph: full and one more value: begun again, column 0 is 40 high %d, column 1 paper %d, the widget's value %d\n",
               dgx_get_pixel(scr, n->x, bottom - 39) == tb[ACCENT], dgx_get_pixel(scr, n->x + 1, bottom) == tb[PAPER], n->state_value.value);
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
               n->w, n->h, r.x, r.y, r.w, r.h, rh, dgx_ui_node(&fan_list.tree, 1)->state_value.value);
        TAP(r.x + 5, r.y + 1 + 2 * rh + rh / 2, 200);
        printf("dropdown: a tap on High: the value %d, fan_set(%d) called %d time(s), the list open %d\n", n->state_value.value, fan_speed, fans, d.popup != NULL);
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
        const int slid = dgx_ui_node(&fan_list.tree, 1)->state_value.value;
        dgx_ui_pointer(&d, &(dgx_ui_pointer_t){.x = (int16_t)(r.x + 5), .y = (int16_t)(r.y + 1 + rh + rh / 2), .released = true, .ms = 880});
        printf("dropdown: down on Low and moved to Medium: the list's pick %d; let go: the list open %d, the value %d, called %d\n", slid, d.popup != NULL,
               n->state_value.value, fans);
        /* BACK is the program's: it closes the list, the value kept */
        const bool back_taken = dgx_ui_key(&d, DGX_UI_KEY_BACK);
        dgx_ui_popup_close(&d);
        printf("dropdown: BACK taken by a widget %d; closed by the program: the value %d, called %d\n", back_taken, n->state_value.value, fans);
        /* the keys: the key that activates opens the list, previous moves the pick, the key that activates chooses */
        dgx_ui_focus(&d, fan);
        dgx_ui_key(&d, DGX_UI_KEY_ACTIVATE);
        const bool by_key = d.popup == &fan_list.tree;
        dgx_ui_key(&d, DGX_UI_KEY_PREV), dgx_ui_key(&d, DGX_UI_KEY_ACTIVATE);
        printf("dropdown: opened by the key %d; PREV and the key that activates: the value %d, fan_set(%d), called %d, open %d\n", by_key,
               n->state_value.value, fan_speed, fans, d.popup != NULL);
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
        const bool     opened = f.popup == &strip_ui && keypad_kb.open, pressed = cn->state_value.bits & DGX_UI_PRESSED;
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
        dgx_ui_buttons(&f, DGX_UI_PAD_DOWN, 1000), dgx_ui_buttons(&f, 0, 1010);
        const bool walked = f.focus == pin && !f.popup;
        dgx_ui_buttons(&f, DGX_UI_PAD_A, 1020), dgx_ui_buttons(&f, 0, 1030);
        printf("strip: DOWN from the code: the focus on the PIN and nothing open %d; A on it: open %d\n", walked, f.popup == &strip_ui);
        /* a button of the board that means done: after dgx_ui_buttons() got it, the strip stays closed at the next tick */
        dgx_ui_buttons(&f, DGX_UI_PAD_A, 1100), edit_close(true);
        dgx_ui_buttons(&f, DGX_UI_PAD_A, 1115);
        const bool stays = !f.popup;
        dgx_ui_buttons(&f, 0, 1130);
        /* and instead of it: the button is a new press at the next tick, and the strip opens again */
        dgx_ui_buttons(&f, DGX_UI_PAD_A, 1200), dgx_ui_buttons(&f, 0, 1210); /* open */
        edit_close(true);                                                    /* the program's own A, dgx_ui_buttons() not called */
        dgx_ui_buttons(&f, DGX_UI_PAD_A, 1315);
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
    puts("cookbook: ran to the end");
    return 0;
}
