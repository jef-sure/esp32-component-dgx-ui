/*
 * The look of a frame of nine pictures (dgx_ui_look_nine), pixel by pixel against a model of its rule: the rule of
 * DGX-GE's talk view that draws a frame of a picture (esp32-component-dgx-ge, src/dgx_ge_talk_view.c,
 * dgx_ge_talk_view_show()): the fill under all, the corners where their cells begin, the edges and the middle laid again
 * and again from their cell's top-left corner and cut by it; the cells by the corners' sizes, the left and top of the
 * top-left, the right of the top-right, the bottom of the bottom-left. Here each piece at its own scale, a right edge
 * of scale 2 beside a top-right of scale 1 tiling inside the right column, cut by it; a box smaller than its corners
 * cut, the far corners at its far edges, none over another, nothing written outside the box. Every pixel of the box
 * once, a child over a corner and an edge drawn over the frame and not under it; the same pixels in strips of 1, 2, 3
 * and 7 rows and in one; at a unit of 1 and of 2; pieces of 1 bit in ink; no pieces, a piece of RGB565, no buffer, a
 * part drawn again. At 18 bits (test_nine18): the fill alone.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_image.h"
#include "dgx_ui_paint.h"
#include "panel.h"

static uint32_t colors[8] = {0x0000, 0xffff, 0x2104, 0xf800, 0x07e0, 0x001f, 0xffe0, 0xf81f};
enum { FILL = 2, INK = 1 };

/* nine pieces of 8 bits, the value (x + 2y + k) % 4 of piece k, 0 its zeros; a map each into the colours 3..7 */
static const struct {
    int w, h, scale;
} sizes[9] = {{3, 2, 1}, {2, 2, 1}, {4, 1, 1}, {3, 3, 1}, {2, 3, 1}, {3, 1, 2}, {3, 3, 1}, {1, 3, 1}, {3, 2, 1}};
/* the top-right lower than its row and the bottom-right narrower than its column: a corner once, the rest of its cell
   the fill; the middle with an origin of its own, which moves nothing */
static uint8_t          data8[9][16];
static uint8_t          maps[9][255];
static dgx_image_t      images8[9];
static dgx_ui_picture_t pieces8[9];
/* the same of 1 bit: the bit (x + y + k) % 2, a row of a byte, the left pixel the high bit */
static uint8_t          data1[9][4];
static dgx_image_t      images1[9];
static dgx_ui_picture_t pieces1[9];

static int value8(int k, int x, int y) { return (x + 2 * y + k) % 4; }
static int value1(int k, int x, int y) { return (x + y + k) % 2; }

static void make_pieces(void)
{
    for (int k = 0; k < 9; ++k) {
        for (int y = 0; y < sizes[k].h; ++y)
            for (int x = 0; x < sizes[k].w; ++x) {
                data8[k][y * sizes[k].w + x] = (uint8_t)value8(k, x, y);
                if (value1(k, x, y)) data1[k][y] |= (uint8_t)(0x80u >> x);
            }
        for (int v = 1; v < 4; ++v) maps[k][v - 1] = (uint8_t)(3 + (k + v) % 5);
        const int16_t origin = k == 4 ? 1 : 0;
        images8[k] = (dgx_image_t){.width = sizes[k].w, .height = sizes[k].h, .origin_x = origin, .origin_y = (int16_t)(2 * origin),
                                   .format = DGX_IMAGE_8BPP, .data = data8[k]};
        images1[k] = (dgx_image_t){.width = sizes[k].w, .height = sizes[k].h, .origin_x = origin, .origin_y = origin, .format = DGX_IMAGE_1BPP,
                                   .data = data1[k]};
        pieces8[k] = (dgx_ui_picture_t){.image = &images8[k], .map = maps[k], .scale = (uint8_t)sizes[k].scale};
        pieces1[k] = (dgx_ui_picture_t){.image = &images1[k], .scale = (uint8_t)sizes[k].scale};
    }
}

/* the model: the index of the table a pixel of the box takes, by DGX-GE's rule, the cut of a small box added */
static int model(const dgx_ui_picture_t *pieces, int unit, int bx, int by, int bw, int bh, int x, int y)
{
    int w[9] = {0}, h[9] = {0}, k[9] = {0};
    for (int i = 0; i < 9 && pieces; ++i) {
        const dgx_image_t *im = pieces[i].image;
        if (!im) continue;
        k[i] = pieces[i].scale ? pieces[i].scale * unit : 1;
        w[i] = im->width * k[i], h[i] = im->height * k[i];
    }
    const int r = w[2] < bw / 2 ? w[2] : bw / 2, l = w[0] < bw - r ? w[0] : bw - r;
    const int b = h[6] < bh / 2 ? h[6] : bh / 2, t = h[0] < bh - b ? h[0] : bh - b;
    const int col = x < bx + l ? 0 : x < bx + bw - r ? 1 : 2, row = y < by + t ? 0 : y < by + bh - b ? 1 : 2, i = row * 3 + col;
    if (!w[i] || !h[i]) return FILL;
    const int sx = col == 0 ? bx : col == 1 ? bx + l : bx + bw - w[2], sy = row == 0 ? by : row == 1 ? by + t : by + bh - h[6];
    int       lx = x - sx, ly = y - sy;
    const bool corner = col != 1 && row != 1;
    if (corner && (lx < 0 || ly < 0 || lx >= w[i] || ly >= h[i])) return FILL;
    if (!corner) lx = ((lx % w[i]) + w[i]) % w[i], ly = ((ly % h[i]) + h[i]) % h[i];
    const int px = lx / k[i], py = ly / k[i];
    if (pieces[i].image->format == DGX_IMAGE_1BPP) return value1(i, px, py) ? INK : FILL;
    const int v = value8(i, px, py);
    return v ? pieces[i].map[v - 1] : FILL;
}

static dgx_ui_nine_look_t params = {.fill = FILL, .ink = INK};
static const dgx_ui_look_t nine = {.draw = dgx_ui_draw_nine, .params = &params, .fill = FILL};
static dgx_ui_widget_t     root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 4};
static dgx_ui_widget_t     frame_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 1, .want_w = 40, .want_h = 24, .look = &nine};
static const dgx_ui_widget_t child_w = {.kind = DGX_UI_FRAME, .want_w = 6, .want_h = 5, .look = &dgx_ui_look_frame};
static dgx_ui_node_t       nodes[3];
static dgx_ui_t            ui;
static int                 frame_i, child_i;
static uint8_t             buffer[4096];

/* the tree drawn whole at a unit, the paint's buffer of so many bytes; with a child or not */
static void draw(panel_t *panel, int unit, size_t room, bool child)
{
    dgx_ui_paint_t paint = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 8, .buffer = room ? buffer : NULL, .buffer_size = room};
    dgx_ui_init(&ui, nodes, 3, unit);
    dgx_ui_add(&ui, -1, &root_w);
    frame_i = dgx_ui_add(&ui, 0, &frame_w);
    child_i = child ? dgx_ui_add(&ui, frame_i, &child_w) : -1;
    memset(panel->mem, 0x5a, sizeof panel->mem); /* a sentinel: what the frame does not write stays it */
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &paint, 0);
    dgx_ui_paint_free(&paint);
}

/* the pixels of the frame's box against the model; those under the child left out */
__attribute__((unused)) static int apart(const panel_t *panel, const dgx_ui_picture_t *pieces, int unit)
{
    const dgx_ui_node_t *f = dgx_ui_node(&ui, frame_i);
    const dgx_ui_node_t *c = child_i >= 0 ? dgx_ui_node(&ui, child_i) : NULL;
    int                  bad = 0;
    for (int y = f->y; y < f->y + f->h; ++y)
        for (int x = f->x; x < f->x + f->w; ++x) {
            if (c && x >= c->x && y >= c->y && x < c->x + c->w && y < c->y + c->h) continue;
            const uint32_t want = colors[model(pieces, unit, f->x, f->y, f->w, f->h, x, y)];
            if (panel->mem[y][x] != want && !bad++) fprintf(stderr, "  first apart at %d,%d: %x, not %x\n", x - f->x, y - f->y, panel->mem[y][x], want);
        }
    return bad;
}

__attribute__((unused)) static void copy_box(const panel_t *panel, uint32_t *out)
{
    const dgx_ui_node_t *f = dgx_ui_node(&ui, frame_i);
    for (int y = 0; y < f->h; ++y)
        for (int x = 0; x < f->w; ++x) out[y * f->w + x] = panel->mem[f->y + y][f->x + x];
}

int main(void)
{
    panel_t *panel = panel_make();
    make_pieces();
#if PANEL_BITS == 18
    /* at 18 bits the fill alone, every pixel of the box once, nothing outside the screen's box of the tree */
    params.pieces = pieces8;
    draw(panel, 1, sizeof buffer, false);
    const dgx_ui_node_t *f = dgx_ui_node(&ui, frame_i);
    int                  not_fill = 0;
    for (int y = f->y; y < f->y + f->h; ++y)
        for (int x = f->x; x < f->x + f->w; ++x) not_fill += panel->mem[y][x] != colors[FILL];
    CHECK(not_fill == 0 && panel_twice(panel) == 0);
#else
    /* the nine of 8 bits at a unit of 1, a child over the top-left corner and the edges beside it: the model, every
       pixel once, the child over the frame */
    params.pieces = pieces8;
    draw(panel, 1, sizeof buffer, true);
    CHECK(apart(panel, pieces8, 1) == 0);
    CHECK(panel_twice(panel) == 0 && panel_written(panel) == PANEL_W * PANEL_H);
    const dgx_ui_node_t *c = dgx_ui_node(&ui, child_i);
    CHECK(c->x < dgx_ui_node(&ui, frame_i)->x + 3 && c->x + c->w > dgx_ui_node(&ui, frame_i)->x + 3); /* across the corner's edge */
    CHECK(panel->mem[c->y][c->x] == colors[3]);                                                      /* the child's own border, over it */

    /* the same pixels in strips of 1, 2, 3 and 7 rows of the box, and in one: tiles across the strips' edges */
    static uint32_t whole[40 * 24], strips[40 * 24];
    draw(panel, 1, 40 * 24, true);
    copy_box(panel, whole);
    for (int rows = 1; rows <= 7; rows += rows == 3 ? 4 : 1) {
        draw(panel, 1, (size_t)(40 * rows), true);
        copy_box(panel, strips);
        CHECK(memcmp(whole, strips, sizeof whole) == 0 && panel_twice(panel) == 0);
        if (memcmp(whole, strips, sizeof whole)) fprintf(stderr, "  strips of %d rows apart\n", rows);
    }
    /* a buffer of less than a row: the fill alone, every pixel once */
    draw(panel, 1, 39, true);
    const dgx_ui_node_t *f = dgx_ui_node(&ui, frame_i);
    int                  off = 0;
    for (int y = f->y; y < f->y + f->h; ++y)
        for (int x = f->x; x < f->x + f->w; ++x)
            if (!(x >= c->x && y >= c->y && x < c->x + c->w && y < c->y + c->h)) off += panel->mem[y][x] != colors[FILL];
    CHECK(off == 0 && panel_twice(panel) == 0);
    draw(panel, 1, 0, true);
    CHECK(panel->mem[f->y][f->x] == colors[FILL] && panel_twice(panel) == 0);

    /* at a unit of 2: every piece twice as large, the right edge's of scale 2 four times, cut by the right column */
    frame_w.want_w = 30, frame_w.want_h = 16;
    draw(panel, 2, sizeof buffer, true);
    CHECK(apart(panel, pieces8, 2) == 0 && panel_twice(panel) == 0);
    CHECK(sizes[5].w * 2 * 2 > sizes[2].w * 2); /* the case: wider than its column */

    /* boxes smaller than their corners: cut, the far corners at the far edges, nothing written outside the box */
    static const struct {
        int w, h;
    } small[] = {{1, 1}, {6, 4}, {7, 3}, {3, 6}, {2, 2}, {5, 5}};
    for (size_t s = 0; s < sizeof small / sizeof *small; ++s) {
        frame_w.want_w = (int16_t)small[s].w, frame_w.want_h = (int16_t)small[s].h;
        draw(panel, 1, sizeof buffer, false);
        const dgx_ui_node_t *g = dgx_ui_node(&ui, frame_i);
        CHECK(apart(panel, pieces8, 1) == 0);
        CHECK(panel_twice(panel) == 0);
        /* the sentinel kept wherever the tree is not: the root's paper around the frame is written by the root */
        CHECK(g->w == small[s].w && g->h == small[s].h);
    }

    /* pieces of 1 bit, their ones in ink, their zeros the fill */
    frame_w.want_w = 40, frame_w.want_h = 24;
    params.pieces = pieces1;
    draw(panel, 1, sizeof buffer, false);
    CHECK(apart(panel, pieces1, 1) == 0 && panel_twice(panel) == 0);

    /* no pieces: the fill alone; a middle of RGB565: its cell the fill, the rest as the model */
    params.pieces = NULL;
    draw(panel, 1, sizeof buffer, false);
    CHECK(apart(panel, NULL, 1) == 0);
    static dgx_ui_picture_t mixed[9];
    static const uint16_t   rgb[4] = {0xffff, 0xffff, 0xffff, 0xffff};
    memcpy(mixed, pieces8, sizeof mixed);
    mixed[4] = (dgx_ui_picture_t){.rgb565 = rgb, .width = 2, .height = 2};
    params.pieces = mixed;
    draw(panel, 1, sizeof buffer, false);
    CHECK(apart(panel, mixed, 1) == 0 && panel_twice(panel) == 0);

    /* a part drawn again, the painter's clip on a band of the frame: the same pixels as whole */
    params.pieces = pieces8;
    draw(panel, 1, sizeof buffer, true);
    copy_box(panel, whole);
    {
        /* a buffer of a row of the part and not of the box: the part alone is built */
        dgx_ui_paint_t paint = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 8, .buffer = buffer, .buffer_size = 20};
        const dgx_ui_node_t *g = dgx_ui_node(&ui, frame_i);
        for (int y = g->y; y < g->y + g->h; ++y)
            for (int x = g->x; x < g->x + g->w; ++x) panel->mem[y][x] = 0x1234;
        dgx_ui_paint_area(&ui, &paint, g->x + 5, g->y + 3, 17, 9);
        dgx_ui_paint_free(&paint);
        int changed = 0;
        for (int y = g->y + 3; y < g->y + 12; ++y)
            for (int x = g->x + 5; x < g->x + 22; ++x) changed += panel->mem[y][x] != whole[(y - g->y) * g->w + (x - g->x)];
        CHECK(changed == 0);
        CHECK(panel->mem[g->y][g->x] == 0x1234); /* outside the part: not written */
    }
#endif
    free(panel);
    CHECK_DONE();
}
