/*
 * Pictures on a panel of 18 or 24 bits (PANEL_BITS), through the strips of pictures (dgx_ui_strips_pictures), against
 * a model of what each pixel is: an indexed picture of 8 and of 4 bits, its pixel the painter's colour of its value,
 * its zeros the paper, the box around it paper, at a scale of 1, 2 and 3; on a drawn background its zeros the
 * background; RGB565 spread to 8 bits a channel, its high bits repeated, white 255 and black 0 at both ends and the
 * middle of each channel; a frame of nine pictures with a child, against the same tree drawn at 16 bits, each colour
 * taken back to its index. Buffers of several sizes, the picture's strips of 1, 2, 3, 7 rows and more; no pixel written
 * twice, a window for a block of rows and not one a row. Without the strips of pictures, or with dgx_ui_strips alone,
 * the box paper. A virtual screen of 24 bits as the paint's screen, a display of DSI's kind, the same pixels.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_image.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#define PANEL_W 120
#define PANEL_H 90
#include "panel.h"

static const uint32_t rgb24[8] = {0x000000, 0xffffff, 0x203040, 0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0xff00ff};
static uint32_t       colors[8];

static uint32_t deep(uint32_t c) { return PANEL_BITS == 18 ? c & 0xfcfcfcu : c; }

/* an 8-bit picture 20 x 37, its value (x + 3y) % 8, and a 4-bit one 7 x 9, (2x + y) % 16 */
enum { W8 = 20, H8 = 37, W4 = 7, H4 = 9 };
static uint8_t     data8[W8 * H8], data4[(W4 + 1) / 2 * H4];
static dgx_image_t image8, image4;
static int         value8(int x, int y) { return (x + 3 * y) % 8; }
static int         value4(int x, int y) { return (2 * x + y) % 8; } /* within the table's eight */

/* RGB565 of 4 x 6: white, black, each channel's middle, and mixes; high byte first. Taller than a strip of a small
   buffer: its rows past the first strip read from where the clip begins */
enum { W565 = 4, H565 = 6 };
static uint16_t       rgb565[W565 * H565];
static uint16_t       rgb565_values[W565 * H565];
static const uint16_t rgb565_ends[8] = {0xffff, 0x0000, 0x8000, 0x0400, 0x0010, 0x7bef, 0xf81f, 0x07e0};

static void make(void)
{
    for (int y = 0; y < H8; ++y)
        for (int x = 0; x < W8; ++x) data8[y * W8 + x] = (uint8_t)value8(x, y);
    for (int y = 0; y < H4; ++y)
        for (int x = 0; x < W4; ++x) data4[y * ((W4 + 1) / 2) + x / 2] |= (uint8_t)(value4(x, y) << (x % 2 ? 0 : 4));
    image8 = (dgx_image_t){.width = W8, .height = H8, .format = DGX_IMAGE_8BPP, .data = data8};
    image4 = (dgx_image_t){.width = W4, .height = H4, .format = DGX_IMAGE_4BPP, .data = data4};
    for (int k = 0; k < W565 * H565; ++k) {
        rgb565_values[k] = k < 8 ? rgb565_ends[k] : (uint16_t)(k * 2731u);
        rgb565[k]        = (uint16_t)(rgb565_values[k] >> 8 | rgb565_values[k] << 8); /* high byte first in memory */
    }
    for (int k = 0; k < 8; ++k) colors[k] = deep(rgb24[k]);
}

/* RGB565 spread as the step promises, the high bits repeated into the low ones */
static uint32_t spread(uint16_t v)
{
    const uint32_t r5 = v >> 11, g6 = v >> 5 & 63, b5 = v & 31;
    return deep((r5 << 3 | r5 >> 2) << 16 | (g6 << 2 | g6 >> 4) << 8 | (b5 << 3 | b5 >> 2));
}

static uint32_t pixel(dgx_screen_t *scr, panel_t *panel, int x, int y)
{
    return panel ? panel->mem[y][x] : dgx_get_pixel(scr, x, y);
}

/* ---- an indexed picture and an RGB565 one, each in a box larger than it ---- */

static dgx_ui_picture_t pic8, pic4, pic565;

/* the pixels of a box that differ from the model: the picture in the middle at scale k, its zeros paper or the
   background, the rest paper */
static int off_model(dgx_screen_t *scr, panel_t *panel, const dgx_ui_node_t *n, int w, int h, int k, int which, dgx_screen_t *background)
{
    const int hx = n->x + (n->w - w * k) / 2, hy = n->y + (n->h - h * k) / 2;
    int       off = 0;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x) {
            uint32_t want = colors[0];
            if (x >= hx && x < hx + w * k && y >= hy && y < hy + h * k) {
                const int px = (x - hx) / k, py = (y - hy) / k;
                if (which == 565) want = spread(rgb565_values[py * w + px]);
                else {
                    const int v = which == 8 ? value8(px, py) : value4(px, py);
                    want        = v ? colors[v] : background ? dgx_get_pixel(background, x, y) : colors[0];
                }
            } else if (background) {
                want = dgx_get_pixel(background, x, y);
            }
            off += pixel(scr, panel, x, y) != want;
        }
    return off;
}

static dgx_screen_t *background;
static void          draw_background(dgx_ui_paint_t *paint, int x, int y, int w, int h, void *ctx)
{
    (void)ctx;
    dgx_vscreen_region_to_screen(paint->scr, x - paint->ox, y - paint->oy, background, x, y, w, h, 0);
}

static int pictures(dgx_screen_t *scr, panel_t *panel, size_t buffer_size, int k, bool drawn, const dgx_ui_strips_t *strips, int *windows)
{
    static uint8_t                     buffer[16384];
    static const dgx_ui_picture_look_t paper  = {.ink = 1, .paper = DGX_UI_PAPER};
    static const dgx_ui_look_t         look8  = {.draw = dgx_ui_draw_indexed, .params = &paper, .fill = DGX_UI_PAPER};
    static const dgx_ui_look_t         look565 = {.draw = dgx_ui_draw_rgb565, .params = &paper, .fill = DGX_UI_PAPER};
    pic8   = (dgx_ui_picture_t){.image = &image8, .scale = (uint8_t)(k > 1 ? k : 0)};
    pic4   = (dgx_ui_picture_t){.image = &image4, .scale = (uint8_t)(k > 1 ? k : 0)};
    pic565 = (dgx_ui_picture_t){.rgb565 = rgb565, .width = W565, .height = H565, .scale = (uint8_t)(k > 1 ? k : 0)};
    const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX};
    const dgx_ui_widget_t w8     = {.kind = DGX_UI_PICTURE, .at_x = 2, .at_y = 1, .want_w = 64, .want_h = 116 < PANEL_H ? 116 : 88, .picture = &pic8, .look = &look8};
    const dgx_ui_widget_t w4     = {.kind = DGX_UI_PICTURE, .at_x = 70, .at_y = 2, .want_w = 26, .want_h = 30, .picture = &pic4, .look = &look8};
    const dgx_ui_widget_t w565   = {.kind = DGX_UI_PICTURE, .at_x = 70, .at_y = 36, .want_w = 16, .want_h = 50, .picture = &pic565, .look = &look565};
    dgx_ui_node_t         nodes[4];
    dgx_ui_t              ui;
    dgx_ui_init(&ui, nodes, 4, 1);
    dgx_ui_add(&ui, -1, &root_w);
    const int a = dgx_ui_add(&ui, 0, &w8), b = dgx_ui_add(&ui, 0, &w4), c = dgx_ui_add(&ui, 0, &w565);
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = 8, .buffer = buffer, .buffer_size = buffer_size, .strips = strips};
    if (drawn) dgx_ui_background_set(&paint, draw_background, NULL);
    if (panel) panel_count_from_now(panel);
    dgx_ui_paint(&ui, &paint, 0);
    if (windows && panel) *windows = panel->windows;
    int off = off_model(scr, panel, dgx_ui_node(&ui, a), W8, H8, k, 8, drawn ? background : NULL) +
              off_model(scr, panel, dgx_ui_node(&ui, b), W4, H4, k, 4, drawn ? background : NULL) + off_model(scr, panel, dgx_ui_node(&ui, c), W565, H565, k, 565, drawn ? background : NULL);
    /* drawn again, the pictures alone: no pixel twice (the root, a box of neither, fills under its children) */
    if (panel) {
        dgx_ui_redraw(&ui, a), dgx_ui_redraw(&ui, b), dgx_ui_redraw(&ui, c);
        panel_count_from_now(panel);
        dgx_ui_paint(&ui, &paint, 20);
        if (panel_twice(panel)) off += 1000000;
    }
    dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
    return off;
}

/* ---- the frame of nine pictures: the same tree at 16 bits, its colours taken back to their indices ---- */

static uint8_t          nine_data[9][6];
static dgx_image_t      nine_images[9];
static dgx_ui_picture_t nine_pieces[9];

static int nine(dgx_screen_t *scr, panel_t *panel, size_t buffer_size)
{
    static uint8_t buffer[16384];
    for (int k = 0; k < 9; ++k) {
        for (int j = 0; j < 6; ++j) nine_data[k][j] = (uint8_t)((j + k) % 8);
        nine_images[k] = (dgx_image_t){.width = 3, .height = 2, .format = DGX_IMAGE_8BPP, .data = nine_data[k]};
        nine_pieces[k] = (dgx_ui_picture_t){.image = &nine_images[k]};
    }
    static const dgx_ui_nine_look_t nine_params = {.pieces = nine_pieces, .fill = 2, .ink = 1};
    static const dgx_ui_look_t      nine_look   = {.draw = dgx_ui_draw_nine, .params = &nine_params, .fill = 2};
    static const dgx_ui_font_variant_t v[1]    = {{.font = &TerminusTTFMedium12}};
    static const dgx_ui_text_style_t   style   = {.fonts = v, .count = 1};
    static dgx_ui_textbox_t            label   = DGX_UI_TEXT_CONST("in", 1, &style);
    const dgx_ui_widget_t root_w  = {.kind = DGX_UI_BOX};
    const dgx_ui_widget_t frame_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 4, .at_x = 3, .at_y = 3, .want_w = 61, .want_h = 47, .look = &nine_look};
    const dgx_ui_widget_t text_w  = {.kind = DGX_UI_LABEL, .want_h = 14, .text = &label};
    dgx_ui_node_t         nodes[4];
    dgx_ui_t              ui;
    dgx_ui_init(&ui, nodes, 4, 1);
    dgx_ui_add(&ui, -1, &root_w);
    const int frame = dgx_ui_add(&ui, 0, &frame_w);
    dgx_ui_add(&ui, frame, &text_w);
    /* the reference at 16 bits, colours of their own, each told from the others */
    static const uint32_t colors16[8] = {0x0000, 0xffff, 0x2104, 0xf800, 0x07e0, 0x001f, 0xffe0, 0xf81f};
    dgx_screen_t         *ref         = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    dgx_ui_paint_t        at16        = {.scr = ref, .colors = colors16, .color_count = 8, .buffer = buffer, .buffer_size = sizeof buffer};
    dgx_ui_paint(&ui, &at16, 0);
    dgx_ui_paint_free(&at16);
    dgx_ui_redraw(&ui, 0);
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = 8, .buffer = buffer, .buffer_size = buffer_size, .strips = &dgx_ui_strips_pictures};
    if (panel) panel_count_from_now(panel);
    dgx_ui_paint(&ui, &paint, 0);
    const dgx_ui_node_t *n   = dgx_ui_node(&ui, frame);
    int                  off = 0;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x) {
            const uint32_t at = dgx_get_pixel(ref, x, y);
            int            k  = 0;
            while (k < 8 && colors16[k] != at) ++k;
            off += k == 8 || pixel(scr, panel, x, y) != colors[k];
        }
    if (panel) { /* drawn again, the frame alone: no pixel twice */
        dgx_ui_redraw(&ui, frame);
        panel_count_from_now(panel);
        dgx_ui_paint(&ui, &paint, 20);
        if (panel_twice(panel)) off += 1000000;
    }
    dgx_ui_paint_free(&paint), dgx_ui_free(&ui), dgx_screen_destroy(&ref);
    return off;
}

int main(void)
{
    make();
    panel_t      *panel = panel_make();
    dgx_screen_t *scr   = &panel->sbus.scr;
    /* the 565 ends */
    CHECK(spread(0xffff) == deep(0xffffff) && spread(0x0000) == 0 && spread(0x8000) == deep(0x840000) && spread(0x0400) == deep(0x008200) &&
          spread(0x0010) == deep(0x000084));
    /* the pictures at scales 1 to 3, buffers from tight to large: the picture's strips of a few rows and more */
    int wrong = 0, cases = 0, few_rows = 0;
    for (int k = 1; k <= 3; ++k)
        for (size_t size = 400 + 600 * (size_t)(k * k); size <= 16384; size = size * 3 / 2) { /* from a row of the picture's and its strip's */
            int       windows = 0;
            const int off     = pictures(scr, panel, size, k, false, &dgx_ui_strips_pictures, &windows);
            if (off && !wrong) fprintf(stderr, "  scale %d, a buffer of %u: %d pixels off\n", k, (unsigned)size, off);
            wrong += off != 0, ++cases;
            few_rows += windows < 200; /* blocks of rows, not a window a row of the 37 x k */
        }
    CHECK(wrong == 0 && cases > 12 && few_rows == cases);
    /* on a drawn background: the zeros of indexed pictures leave it */
    background = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) dgx_set_pixel(background, x, y, deep((uint32_t)(x * 2) << 16 | (uint32_t)(y * 2) << 8 | 0x40));
    CHECK(pictures(scr, panel, 4096, 1, true, &dgx_ui_strips_pictures, NULL) == 0);
    CHECK(pictures(scr, panel, 4096, 2, true, &dgx_ui_strips_pictures, NULL) == 0);
    /* without the strips of pictures: paper, as before */
    CHECK(pictures(scr, panel, 4096, 1, false, &dgx_ui_strips, NULL) > 100 && pictures(scr, panel, 4096, 1, false, NULL, NULL) > 100);
    /* the frame of nine pictures with a child, buffers of a row of it and more */
    int nine_off = 0;
    for (size_t size = 520; size <= 8192; size = size * 2) nine_off += nine(scr, panel, size); /* from a row of the strips' at half the buffer */
    CHECK(nine_off == 0);
    /* a virtual screen of the depth as the paint's screen: the same */
    dgx_screen_t *vs = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    CHECK(pictures(vs, NULL, 4096, 2, false, &dgx_ui_strips_pictures, NULL) == 0 && nine(vs, NULL, 4096) == 0);
    dgx_screen_destroy(&vs), dgx_screen_destroy(&background);
    free(panel);
    CHECK_DONE();
}
