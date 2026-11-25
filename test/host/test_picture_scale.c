/*
 * A picture's scale (dgx_ui_picture_t::scale): a pixel of it so many pixels of the screen, 1 for 0, scale times the
 * tree's unit else, at most 255. At a unit of 1, 2 and 3 and a scale of 0, 1 and 2, for a picture of 1 bit, a picture
 * button's, one of RGB565 and ones of 4 and 8 bits through a table: the widget as large as the picture so many times,
 * and every pixel of its box the picture's pixel it falls in, or the paper around. At a factor of 3, on a panel behind a
 * bus with the strips named and a strip of 10 rows, an edge of a strip inside a scaled pixel: the same pixels as drawn
 * straight, no row doubled or missed, every pixel once; a part repainted over a rectangle that cuts scaled pixels: every
 * pixel inside it as in the whole, none outside it written; a box smaller than its picture so many times, on a screen
 * turned either way: nothing written outside the box; a picture of 1 bit over a drawn background: the background where
 * it has no ink. The factor at most 255.
 */
#ifndef PANEL_BITS
#define PANEL_BITS 16
#endif
#define PANEL_W 72
#define PANEL_H 60
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_image.h"
#include "dgx_ui_paint.h"
#include "dgx_vscreen.h"
#include "panel.h"

static uint32_t colors[4];

/* the pictures: 1 bit 5x4 of an origin inside it, 4 bits 5x4, 8 bits 4x4, RGB565 3x2; each pixel told apart from its
   neighbours; at 3 the four rows are 12, an edge of a strip of 10 inside the fourth */
static const uint8_t     bits1[4] = {0xa8, 0x50, 0xf8, 0x88};
static const dgx_image_t im1      = {.width = 5, .height = 4, .format = DGX_IMAGE_1BPP, .data = bits1, .origin_x = 2, .origin_y = 1};
static const uint8_t     bits4[3 * 4] = {0x12, 0x31, 0x20, 0x23, 0x12, 0x30, 0x31, 0x23, 0x10, 0x02, 0x31, 0x20};
static const uint8_t     map4[15]     = {0, 1, 2, 3};
static const dgx_image_t im4          = {.width = 5, .height = 4, .format = DGX_IMAGE_4BPP, .data = bits4};
static const uint8_t     bits8[4 * 4] = {1, 2, 3, 0, 3, 1, 2, 1, 2, 0, 1, 3, 0, 3, 2, 1};
static const uint8_t     map8[255]    = {0, 1, 2, 3};
static const dgx_image_t im8          = {.width = 4, .height = 4, .format = DGX_IMAGE_8BPP, .data = bits8};
static uint16_t          rgb[6]; /* big-endian RGB565, as the vscreen over an array takes it: set from the colours */

enum { ONE, BUTTON, RGB, FOUR, EIGHT, KINDS };
static dgx_ui_picture_t pictures[KINDS];

/* the colour of a picture's pixel, an index of the painter's table */
static uint32_t pixel_of(int kind, int x, int y)
{
    /* a value of 4 or 8 bits v: the table's entry v - 1, 0 the paper behind it, as DGX's blit takes a map */
    int v;
    switch (kind) {
    case ONE: return (bits1[y] >> (7 - x)) & 1 ? 1 : 0;      /* the ink, colour 1; the paper 0 */
    case BUTTON: return (bits1[y] >> (7 - x)) & 1 ? 1 : 2;   /* the ink on the button's face, colour 2 */
    case FOUR: v = (bits4[y * 3 + x / 2] >> (x % 2 ? 0 : 4)) & 15; return v ? map4[v - 1] : 0;
    case EIGHT: v = bits8[y * 4 + x]; return v ? map8[v - 1] : 0;
    default: return 0;
    }
}

static const dgx_ui_look_t *look_of(int kind)
{
    return kind == ONE ? &dgx_ui_look_picture : kind == BUTTON ? &dgx_ui_look_picture_button : kind == RGB ? &dgx_ui_look_rgb565 : &dgx_ui_look_indexed;
}

static dgx_ui_node_t   nodes[4];
static dgx_ui_t        ui;
static dgx_ui_widget_t pic_w;
static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
static int             pic_i;

/* a tree of one picture's widget at a unit, of its natural size or of the size given */
static void make(int kind, int unit, int want_w, int want_h)
{
    static bool made;
    if (made) dgx_ui_free(&ui);
    made  = true;
    pic_w = (dgx_ui_widget_t){.kind = kind == BUTTON ? DGX_UI_BUTTON : DGX_UI_PICTURE, .picture = &pictures[kind], .look = look_of(kind),
                              .want_w = (int16_t)want_w, .want_h = (int16_t)want_h};
    dgx_ui_init(&ui, nodes, 4, unit);
    dgx_ui_add(&ui, -1, &root_w);
    pic_i = dgx_ui_add(&ui, 0, &pic_w);
}

/* the expected colour at a pixel of the picture's hole: its pixel at (x / k, y / k) */
static uint32_t expected(int kind, int x, int y, int k)
{
    if (kind == RGB) {
        const uint16_t v = rgb[(y / k) * 3 + x / k];
        return (uint32_t)((v >> 8) | (v & 0xff) << 8);
    }
    return colors[pixel_of(kind, x / k, y / k)];
}

/* the pixels of a widget's hole not as the picture so many times, and of its box around the hole not of the paper
   (a button's face around it is its own: not counted) */
static int off_picture(dgx_screen_t *s, int kind, int k)
{
    const dgx_ui_node_t *n = dgx_ui_node(&ui, pic_i);
    const dgx_image_t   *im = pictures[kind].image;
    const int pw = (im ? im->width : pictures[kind].width) * k, ph = (im ? im->height : pictures[kind].height) * k;
    int       hx = n->x + (n->w - pw) / 2, hy = n->y + (n->h - ph) / 2;
    if (kind == BUTTON) { /* centred in its face, not its box: found by its ink, which reaches every edge of it */
        hx = hy = 1 << 30;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x)
                if (dgx_get_pixel(s, x, y) == colors[1]) hx = x < hx ? x : hx, hy = y < hy ? y : hy;
    }
    int off = 0;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x) {
            const bool in = x >= hx && y >= hy && x < hx + pw && y < hy + ph;
            if (!in && kind == BUTTON) continue;
            const uint32_t want = in ? expected(kind, x - hx, y - hy, k) : colors[0];
            if (getenv("SHOW") && dgx_get_pixel(s, x, y) != want && off < 4) fprintf(stderr, "    at %d,%d got %04x want %04x\n", x - hx, y - hy, (unsigned)dgx_get_pixel(s, x, y), (unsigned)want);
            off += dgx_get_pixel(s, x, y) != want;
        }
    return off;
}

static dgx_screen_t *fresh(void)
{
    dgx_screen_t *v = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    dgx_fill_rectangle(v, 0, 0, PANEL_W, PANEL_H, colors[0]);
    return v;
}

static void test_sizes_and_pixels(void)
{
    static uint8_t buffer[PANEL_W * 2 * 4];
    for (int kind = 0; kind < KINDS; ++kind)
        for (int unit = 1; unit <= 3; ++unit)
            for (int scale = 0; scale <= 2; ++scale) {
                pictures[kind].scale = (uint8_t)scale;
                const int k = scale ? scale * unit : 1;
                CHECK(dgx_ui_picture_scale(&(dgx_ui_t){.unit = (int16_t)unit}, &pictures[kind]) == k);
                make(kind, unit, 0, 0);
                dgx_screen_t  *v = fresh();
                dgx_ui_paint_t p = {.scr = v, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
                dgx_ui_paint(&ui, &p, 0);
                const dgx_ui_node_t *n  = dgx_ui_node(&ui, pic_i);
                const dgx_image_t   *im = pictures[kind].image;
                const int pw = (im ? im->width : pictures[kind].width) * k, ph = (im ? im->height : pictures[kind].height) * k;
                const int room = kind == BUTTON ? 5 * unit : 0;
                const int off  = off_picture(v, kind, k);
                if (n->w != pw + room || n->h != ph + room || off)
                    fprintf(stderr, "  kind %d unit %d scale %d: %dx%d for %dx%d, %d pixels off\n", kind, unit, scale, n->w, n->h, pw + room, ph + room, off);
                CHECK(n->w == pw + room && n->h == ph + room && off == 0);
                dgx_ui_paint_free(&p);
                v->destroy(&v);
            }
    for (int kind = 0; kind < KINDS; ++kind) pictures[kind].scale = 0;
    /* at most 255 */
    pictures[ONE].scale = 200;
    CHECK(dgx_ui_picture_scale(&(dgx_ui_t){.unit = 2}, &pictures[ONE]) == 255);
    pictures[ONE].scale = 0;
}

/* a drawn background of a pattern no look draws, for the strips on a display of 16 bits */
static void pattern(dgx_ui_paint_t *paint, int x, int y, int w, int h, void *ctx)
{
    for (int row = y; row < y + h; ++row)
        for (int column = x; column < x + w; ++column)
            dgx_set_pixel(paint->scr, column - paint->ox, row - paint->oy, (uint32_t)(((column * 3) & 0x1f) << 11 | ((row * 2) & 0x1f) << 5 | 0x10));
}

/* at a factor of 3 on a panel: in strips of 10 rows, an edge inside a scaled pixel, against the same drawn straight; a
   part repainted over a rectangle that cuts scaled pixels. Strips are drawn on a display of 18 bits, and on one of 16
   over a drawn background */
static void test_panel(void)
{
    for (int kind = 0; kind < KINDS; ++kind) {
        if (PANEL_BITS != 16 && kind != ONE && kind != BUTTON) continue; /* RGB565 and a table: a 16-bit screen's */
        pictures[kind].scale = 3;
        make(kind, 1, 0, 0);
        dgx_ui_set_root(&ui, 0, 0, PANEL_W, PANEL_H);
        dgx_ui_layout(&ui);
        /* the buffer: ten rows of the widget's width, and the room a picture of a table takes past them, less than a row */
        const dgx_ui_node_t *n     = dgx_ui_node(&ui, pic_i);
        const int            row   = n->w * (PANEL_BITS == 16 ? 2 : 3);
        const int            extra = pictures[kind].image && pictures[kind].image->format != DGX_IMAGE_1BPP ? 4 * pictures[kind].image->width : 0;
        static uint8_t       strip_buffer[4096], plain_buffer[PANEL_W * 4];
        CHECK(10 * row + extra <= (int)sizeof strip_buffer && extra < row);
        panel_t       *straight = panel_make(), *stripped = panel_make();
        dgx_ui_paint_t a = {.scr = &straight->sbus.scr, .colors = colors, .color_count = 4, .buffer = plain_buffer, .buffer_size = sizeof plain_buffer};
        dgx_ui_paint_t b = {.scr = &stripped->sbus.scr, .colors = colors, .color_count = 4, .buffer = strip_buffer, .buffer_size = (size_t)(10 * row + extra),
                            .strips = &dgx_ui_strips};
        if (PANEL_BITS == 16) dgx_ui_background_set(&a, pattern, NULL), dgx_ui_background_set(&b, pattern, NULL);
        dgx_ui_paint(&ui, &a, 0);
        make(kind, 1, 0, 0); /* the same tree anew: its first pass on the other panel */
        panel_count_from_now(stripped);
        const int windows = stripped->windows;
        dgx_ui_paint(&ui, &b, 0);
        int apart = 0;
        for (int y = 0; y < PANEL_H; ++y)
            for (int x = 0; x < PANEL_W; ++x) apart += straight->mem[y][x] != stripped->mem[y][x];
        n = dgx_ui_node(&ui, pic_i);
        /* the strips there: a window a strip, more than one over the widget's height of 12 or more rows */
        const bool strips = stripped->windows - windows >= (n->h + 9) / 10;
        if (apart || panel_twice(stripped) || !strips)
            fprintf(stderr, "  kind %d in strips: %d pixels apart, %d twice, %d windows\n", kind, apart, panel_twice(stripped), stripped->windows - windows);
        CHECK(apart == 0 && panel_twice(stripped) == 0 && strips);
        /* a part over a rectangle that cuts scaled pixels: as the whole inside it, nothing outside it */
        const int rx = n->x + 5, ry = n->y + 4, rw = 13, rh = 11;
        for (int y = ry; y < ry + rh; ++y)
            for (int x = rx; x < rx + rw; ++x) stripped->mem[y][x] = 0x5a5a;
        panel_count_from_now(stripped);
        dgx_ui_paint_area(&ui, &b, rx, ry, rw, rh);
        apart = 0;
        for (int y = 0; y < PANEL_H; ++y)
            for (int x = 0; x < PANEL_W; ++x) apart += straight->mem[y][x] != stripped->mem[y][x];
        const int outside = panel_written_outside(stripped, rx, ry, rx + rw, ry + rh);
        if (apart || outside) fprintf(stderr, "  kind %d, a part: %d pixels apart, %d written outside\n", kind, apart, outside);
        CHECK(apart == 0 && outside == 0);
        dgx_ui_paint_free(&a), dgx_ui_paint_free(&b);
        free(straight), free(stripped);
        pictures[kind].scale = 0;
    }
}

/* a box smaller than its picture three times, on a screen of 72x60 turned either way: nothing written outside it, the
   picture's paper (colour 2, a look's own) told from the tree's background (colour 3) */
static const dgx_ui_picture_look_t own_paper = {.ink = 1, .paper = 2};
static const dgx_ui_look_t         picture_own = {.draw = dgx_ui_draw_picture, .params = &own_paper, .fill = -1};
static const dgx_ui_look_t         rgb565_own  = {.draw = dgx_ui_draw_rgb565, .params = &own_paper, .fill = -1};
static const dgx_ui_look_t         indexed_own = {.draw = dgx_ui_draw_indexed, .params = &own_paper, .fill = -1};

static void test_small_box(void)
{
    static uint8_t buffer[PANEL_W * 2 * 4];
    for (int turned = 0; turned < 2; ++turned)
        for (int kind = 0; kind < KINDS; ++kind) {
            pictures[kind].scale = 3;
            make(kind, 1, 6, 5);
            if (kind != BUTTON) pic_w.look = kind == ONE ? &picture_own : kind == RGB ? &rgb565_own : &indexed_own;
            dgx_screen_t *v = fresh();
            if (turned) v->dir_x = DgxScreenRightLeft, v->swap_xy = true; /* 60x72 */
            dgx_fill_rectangle(v, 0, 0, v->width, v->height, 0x1234);
            dgx_ui_paint_t p = {.scr = v, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer, .background = 3};
            dgx_ui_paint(&ui, &p, 0);
            const dgx_ui_node_t *n = dgx_ui_node(&ui, pic_i);
            /* outside the widget only the tree's background, or the colour put before: no ink, no paper of the picture */
            int outside = 0;
            for (int y = 0; y < v->height; ++y)
                for (int x = 0; x < v->width; ++x)
                    if (!(x >= n->x && y >= n->y && x < n->x + n->w && y < n->y + n->h)) outside += dgx_get_pixel(v, x, y) != 0x1234 && dgx_get_pixel(v, x, y) != colors[3];
            if (outside) fprintf(stderr, "  kind %d turned %d: %d pixels of the picture outside its box\n", kind, turned, outside);
            CHECK(outside == 0);
            dgx_ui_paint_free(&p);
            v->destroy(&v);
            pictures[kind].scale = 0;
        }
}

/* over a drawn background, a picture of 1 bit at 3: the background where it has no ink */
static void test_background(void)
{
    pictures[ONE].scale = 3;
    make(ONE, 1, 0, 0);
    dgx_screen_t  *v = fresh();
    dgx_ui_paint_t p = {.scr = v, .colors = colors, .color_count = 4};
    dgx_ui_background_set(&p, pattern, NULL);
    dgx_ui_paint(&ui, &p, 0);
    const dgx_ui_node_t *n = dgx_ui_node(&ui, pic_i);
    int                  off = 0;
    for (int y = 0; y < n->h; ++y)
        for (int x = 0; x < n->w; ++x) {
            const uint32_t back = (uint32_t)((((n->x + x) * 3) & 0x1f) << 11 | (((n->y + y) * 2) & 0x1f) << 5 | 0x10);
            off += dgx_get_pixel(v, n->x + x, n->y + y) != (pixel_of(ONE, x / 3, y / 3) ? colors[1] : back);
        }
    CHECK(off == 0);
    dgx_ui_paint_free(&p);
    v->destroy(&v);
    pictures[ONE].scale = 0;
}

int main(void)
{
    dgx_screen_t *probe = dgx_vscreen_init(4, 4, 16, DgxScreenRGB);
    colors[0] = dgx_screen_rgb(probe, 10, 20, 40), colors[1] = dgx_screen_rgb(probe, 240, 240, 230);
    colors[2] = dgx_screen_rgb(probe, 60, 120, 200), colors[3] = dgx_screen_rgb(probe, 250, 160, 40);
    probe->destroy(&probe);
    for (int k = 0; k < 6; ++k) {
        const uint16_t c = (uint16_t)colors[(k + 1) % 4];
        rgb[k]           = (uint16_t)((c >> 8) | (c & 0xff) << 8);
    }
    pictures[ONE]    = (dgx_ui_picture_t){.image = &im1};
    pictures[BUTTON] = (dgx_ui_picture_t){.image = &im1};
    pictures[RGB]    = (dgx_ui_picture_t){.rgb565 = rgb, .width = 3, .height = 2};
    pictures[FOUR]   = (dgx_ui_picture_t){.image = &im4, .map = map4};
    pictures[EIGHT]  = (dgx_ui_picture_t){.image = &im8, .map = map8};
    if (PANEL_BITS == 16) {
        test_sizes_and_pixels();
        test_small_box();
        test_background();
    }
    test_panel();
    dgx_ui_free(&ui);
    CHECK_DONE();
}
