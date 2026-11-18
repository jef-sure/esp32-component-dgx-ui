/*
 * The look of indexed pictures through the paint's buffer: a picture of 4 bits and one of 8, in one strip and in
 * several, every pixel of the box once, each pixel the painter's colour of its value through the map, its 0 the
 * paper, the box around it paper; a picture of 1 bit as the look of a picture draws it; no buffer, the box paper; the
 * screen composed in strips as drawn. A popup over a part of a picture: nothing written under it; a buffer of one row
 * exactly, and of a width that does not divide it; a map entry and a value past the table; an icon between pictures of
 * two sizes; a picture wider than its box, cut by it.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_image.h"
#include "dgx_ui_paint.h"
#include "dgx_vscreen.h"
#include "panel.h"

static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};

/* 6x5 of 4 bits, the value (x + y) % 4: a row of 3 bytes, the left pixel in the high bits */
static uint8_t           bits4[5 * 3];
static const uint8_t     map4[15] = {1, 2, 3}; /* 1, 2, 3: the ink, the fill, the accent */
static const dgx_image_t im4      = {.width = 6, .height = 5, .format = DGX_IMAGE_4BPP, .data = bits4};
/* 5x4 of 8 bits, the value (x * y) % 4 */
static uint8_t           bits8[4 * 5];
static const uint8_t     map8[255] = {3, 1, 2};
static const dgx_image_t im8       = {.width = 5, .height = 4, .format = DGX_IMAGE_8BPP, .data = bits8};
/* 8x8 of 1 bit, a frame */
static const uint8_t     frame_bits[8] = {0xff, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xff};
static const dgx_image_t im1           = {.width = 8, .height = 8, .format = DGX_IMAGE_1BPP, .data = frame_bits};

static dgx_ui_picture_t pic4 = {.image = &im4, .map = map4}, pic8 = {.image = &im8, .map = map8}, pic1 = {.image = &im1};
/* past the table of 4: a map entry 200, and with no map a value 250 */
static const uint8_t     far_map[15] = {200, 200, 200};
static uint8_t           far_bits[2 * 3];
static const dgx_image_t far8        = {.width = 3, .height = 2, .format = DGX_IMAGE_8BPP, .data = far_bits};
static dgx_ui_picture_t  far_pics[2] = {{.image = &im4, .map = far_map}, {.image = &far8}};
/* an icon of a picture of 4 bits and one of 8, of two sizes */
static dgx_ui_picture_t  two[2];

static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 1, .gap = 1};
static const dgx_ui_widget_t w4     = {.kind = DGX_UI_PICTURE, .picture = &pic4, .want_w = 10, .want_h = 9, .look = &dgx_ui_look_indexed};
static const dgx_ui_widget_t w8     = {.kind = DGX_UI_PICTURE, .picture = &pic8, .want_w = 9, .want_h = 6, .look = &dgx_ui_look_indexed};
static const dgx_ui_widget_t w1     = {.kind = DGX_UI_PICTURE, .picture = &pic1, .want_h = 10, .look = &dgx_ui_look_indexed};
static const dgx_ui_widget_t w1_old = {.kind = DGX_UI_PICTURE, .picture = &pic1, .want_h = 10, .look = &dgx_ui_look_picture};
static const dgx_ui_widget_t icon_w = {.kind = DGX_UI_ICON, .picture = two, .min = 0, .max = 1, .want_w = 10, .want_h = 7, .look = &dgx_ui_look_indexed};
static const dgx_ui_widget_t far_w  = {.kind = DGX_UI_ICON, .picture = far_pics, .min = 0, .max = 1, .want_w = 8, .want_h = 6, .look = &dgx_ui_look_indexed};
static const dgx_ui_widget_t thin_w = {.kind = DGX_UI_PICTURE, .picture = &pic4, .want_w = 4, .want_h = 5, .look = &dgx_ui_look_indexed};
static const dgx_ui_widget_t pop_w  = {.kind = DGX_UI_FRAME};

static dgx_ui_node_t nodes[6];
static dgx_ui_t      ui;
static int           p4, p8, p1;

/* the pixels of a widget's box, their colours, as a list from its top left */
static void box_pixels(const panel_t *panel, const dgx_ui_t *t, int i, uint16_t *out)
{
    const dgx_ui_node_t *n = dgx_ui_node(t, i);
    for (int y = 0; y < n->h; ++y)
        for (int x = 0; x < n->w; ++x) out[y * n->w + x] = panel->mem[n->y + y][n->x + x];
}

/* the picture of a widget on the panel as its values and the map say, the rest of its box paper */
static int wrong(const panel_t *panel, int i, const dgx_image_t *im, const uint8_t *map)
{
    const dgx_ui_node_t *n  = dgx_ui_node(&ui, i);
    const int            x0 = n->x + (n->w - im->width) / 2, y0 = n->y + (n->h - im->height) / 2;
    int                  bad = 0;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x) {
            int v = 0;
            if (x >= x0 && y >= y0 && x < x0 + im->width && y < y0 + im->height) {
                const int px = x - x0, py = y - y0;
                v = im->format == DGX_IMAGE_4BPP ? (im->data[py * 3 + px / 2] >> (px % 2 ? 0 : 4)) & 15 : im->data[py * im->width + px];
            }
            const uint32_t want = v ? colors[map[v - 1]] : colors[0];
            bad += panel->mem[y][x] != want;
        }
    return bad;
}

static bool once_in_box(panel_t *panel, int i)
{
    const dgx_ui_node_t *n = dgx_ui_node(&ui, i);
    int                  out = 0, in = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x)
            if (panel->writes[y][x]) (x >= n->x && x < n->x + n->w && y >= n->y && y < n->y + n->h) ? ++in : ++out;
    return out == 0 && in == n->w * n->h && panel_twice(panel) == 0;
}

int main(void)
{
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 6; ++x) bits4[y * 3 + x / 2] |= (uint8_t)(((x + y) % 4) << (x % 2 ? 0 : 4));
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 5; ++x) bits8[y * 5 + x] = (uint8_t)((x * y) % 4);
    panel_t *panel = panel_make();
    static uint8_t buffer[64];
    dgx_ui_paint_t pp = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
    dgx_ui_init(&ui, nodes, 6, 1);
    dgx_ui_add(&ui, -1, &root_w);
    p4 = dgx_ui_add(&ui, 0, &w4);
    p8 = dgx_ui_add(&ui, 0, &w8);
    p1 = dgx_ui_add(&ui, 0, &w1);

    /* a buffer of 64: each picture in one strip; every pixel of the screen once; the colours through the maps */
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &pp, 0);
    CHECK(panel_twice(panel) == 0 && panel_written(panel) == PANEL_W * PANEL_H);
    CHECK(wrong(panel, p4, &im4, map4) == 0 && wrong(panel, p8, &im8, map8) == 0);
    /* the frame of 1 bit: its corner the ink, its middle the paper */
    {
        const dgx_ui_node_t *n = dgx_ui_node(&ui, p1);
        const int            x0 = n->x + (n->w - 8) / 2, y0 = n->y + (n->h - 8) / 2;
        CHECK(panel->mem[y0][x0] == colors[1] && panel->mem[y0 + 3][x0 + 3] == colors[0]);
    }

    /* a buffer of 12: two rows of the 4-bit picture at a time, three strips; drawn again, its box once */
    pp.buffer_size = 12;
    dgx_ui_redraw(&ui, p4);
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &pp, 1);
    CHECK(once_in_box(panel, p4) && wrong(panel, p4, &im4, map4) == 0);
    /* a buffer of 5: one row of the 8-bit picture at a time */
    pp.buffer_size = 5;
    dgx_ui_redraw(&ui, p8);
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &pp, 2);
    CHECK(once_in_box(panel, p8) && wrong(panel, p8, &im8, map8) == 0);

    /* the screen composed in strips of 4 rows into a buffer of its own, the paint's buffer for the pictures */
    {
        pp.buffer_size = sizeof buffer;
        dgx_screen_t  *band = dgx_vscreen_init(PANEL_W, 4, 16, DgxScreenRGB);
        dgx_ui_paint_t pb   = {.scr = band, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
        int            off  = 0;
        for (int y0 = 0; y0 < PANEL_H; y0 += 4) {
            pb.oy = (int16_t)y0;
            dgx_ui_paint_area(&ui, &pb, 0, y0, PANEL_W, 4);
            for (int y = 0; y < 4 && y0 + y < PANEL_H; ++y)
                for (int x = 0; x < PANEL_W; ++x) off += dgx_get_pixel(band, x, y) != panel->mem[y0 + y][x];
        }
        CHECK(off == 0);
        dgx_ui_paint_free(&pb);
        band->destroy(&band);
    }

    /* no buffer, or one smaller than a row: the box paper */
    pp.buffer = NULL;
    dgx_ui_redraw(&ui, p4);
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &pp, 3);
    {
        const dgx_ui_node_t *n = dgx_ui_node(&ui, p4);
        int                  ink = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) ink += panel->mem[y][x] != colors[0];
        CHECK(once_in_box(panel, p4) && ink == 0);
    }
    pp.buffer = buffer, pp.buffer_size = 3;
    dgx_ui_redraw(&ui, p8);
    dgx_ui_paint(&ui, &pp, 4);
    CHECK(wrong(panel, p8, &im8, map8) > 0); /* paper where the picture was */

    pp.buffer_size = sizeof buffer;

    /* a buffer of one row exactly (6), and of 13: two rows and a byte to spare */
    pp.buffer_size = 6;
    dgx_ui_redraw(&ui, p4);
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &pp, 5);
    CHECK(once_in_box(panel, p4) && wrong(panel, p4, &im4, map4) == 0);
    pp.buffer_size = 13;
    dgx_ui_redraw(&ui, p4);
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &pp, 6);
    CHECK(once_in_box(panel, p4) && wrong(panel, p4, &im4, map4) == 0);
    pp.buffer_size = sizeof buffer;

    /* a popup over the right half of the 4-bit picture's box: drawn again, nothing under the popup, the rest once */
    {
        static dgx_ui_node_t pn[1];
        static dgx_ui_t      pop;
        dgx_ui_init(&pop, pn, 1, 1), dgx_ui_add(&pop, -1, &pop_w);
        const dgx_ui_node_t *n = dgx_ui_node(&ui, p4);
        const int            px = n->x + n->w / 2;
        CHECK(dgx_ui_popup_open(&ui, &pop, px, 0, PANEL_W - px, n->y + n->h + 2, 0));
        dgx_ui_paint(&ui, &pp, 7);
        dgx_ui_redraw(&ui, p4);
        panel_count_from_now(panel);
        dgx_ui_paint(&ui, &pp, 8);
        int under = 0, left = 0;
        for (int y = 0; y < PANEL_H; ++y)
            for (int x = 0; x < PANEL_W; ++x) {
                if (panel->writes[y][x] && x >= px && y < n->y + n->h + 2) ++under;
                if (panel->writes[y][x] && x < px && x >= n->x && y >= n->y && y < n->y + n->h) ++left;
            }
        CHECK(under == 0 && panel_twice(panel) == 0 && left == (px - n->x) * n->h);
        dgx_ui_popup_close(&ui);
        dgx_ui_paint(&ui, &pp, 9);
        CHECK(wrong(panel, p4, &im4, map4) == 0);
        dgx_ui_free(&pop);
    }

    /* a picture of 1 bit through this look, as the look of a picture draws it */
    {
        static uint16_t a[100 * 10], b[100 * 10];
        box_pixels(panel, &ui, p1, a);
        static dgx_ui_node_t on[2];
        static dgx_ui_t      old;
        dgx_ui_init(&old, on, 2, 1), dgx_ui_add(&old, -1, &root_w);
        const int o1 = dgx_ui_add(&old, 0, &w1_old);
        dgx_ui_paint(&old, &pp, 10);
        box_pixels(panel, &old, o1, b);
        CHECK(dgx_ui_node(&old, o1)->w == dgx_ui_node(&ui, p1)->w && memcmp(a, b, sizeof(uint16_t) * (size_t)(dgx_ui_node(&old, o1)->w * 10)) == 0);
        dgx_ui_free(&old);
    }

    /* an icon between a picture of 4 bits (6x5) and one of 8 (5x4): the paper around redrawn; a map entry and a value
       past the table of 4 drawn as its colour 0, nothing read past it (ASan); a picture wider than its box, cut by it */
    {
        two[0] = pic4, two[1] = pic8;
        static dgx_ui_node_t tn[4];
        static dgx_ui_t      t;
        dgx_ui_init(&t, tn, 4, 1), dgx_ui_add(&t, -1, &root_w);
        const int ic = dgx_ui_add(&t, 0, &icon_w), fr = dgx_ui_add(&t, 0, &far_w), th = dgx_ui_add(&t, 0, &thin_w);
        far_bits[0] = 250;
        dgx_ui_paint(&t, &pp, 11);
        dgx_ui_set_value(&t, ic, 1);
        panel_count_from_now(panel);
        dgx_ui_paint(&t, &pp, 12);
        {
            const dgx_ui_node_t *n = dgx_ui_node(&t, ic);
            const int            x0 = n->x + (n->w - 5) / 2, y0 = n->y + (n->h - 4) / 2;
            int                  bad = 0, out = 0;
            for (int y = n->y; y < n->y + n->h; ++y)
                for (int x = n->x; x < n->x + n->w; ++x) {
                    const int v = x >= x0 && y >= y0 && x < x0 + 5 && y < y0 + 4 ? bits8[(y - y0) * 5 + x - x0] : 0;
                    bad += panel->mem[y][x] != (v ? colors[map8[v - 1]] : colors[0]);
                }
            for (int y = 0; y < PANEL_H; ++y)
                for (int x = 0; x < PANEL_W; ++x) out += panel->writes[y][x] && !(x >= n->x && x < n->x + n->w && y >= n->y && y < n->y + n->h);
            CHECK(bad == 0 && out == 0 && panel_twice(panel) == 0);
        }
        {
            /* the 4-bit picture through a map of 200s: every pixel not 0 is colour 0 of a table of 4 (the table's 200) */
            const dgx_ui_node_t *n = dgx_ui_node(&t, fr);
            int                  lit = 0;
            for (int y = n->y; y < n->y + n->h; ++y)
                for (int x = n->x; x < n->x + n->w; ++x) lit += panel->mem[y][x] != 0;
            CHECK(lit == 0);
            dgx_ui_set_value(&t, fr, 1); /* the 8-bit one with no map, a value 250 */
            dgx_ui_paint(&t, &pp, 13);
        }
        {
            const dgx_ui_node_t *n = dgx_ui_node(&t, th);
            dgx_ui_redraw(&t, th);
            panel_count_from_now(panel);
            dgx_ui_paint(&t, &pp, 14);
            int out = 0;
            for (int y = 0; y < PANEL_H; ++y)
                for (int x = 0; x < PANEL_W; ++x) out += panel->writes[y][x] && !(x >= n->x && x < n->x + n->w && y >= n->y && y < n->y + n->h);
            CHECK(n->w == 4 && out == 0 && panel_twice(panel) == 0 && panel_written(panel) == n->w * n->h);
        }
        dgx_ui_free(&t);
    }

    dgx_ui_paint_free(&pp);
    CHECK(pp.indexed == NULL && pp.lut == NULL);
    dgx_ui_free(&ui);
    free(panel);
    CHECK_DONE();
}
