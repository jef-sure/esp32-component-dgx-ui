/*
 * The level bar, dgx_ui_look_level: the value of the widget's place within min..max as the filled part of its box. From
 * each of the four edges; a value past max and below min, a place written with any int32; max equal to min and below
 * it, drawn empty; a box of no width; the widest box with the widest range. When only the value moves, only the part
 * between the filled length drawn and the new one is written, to the far edge's side too; a band of colour changed, all
 * of the filled part; by place, each part in its own band, the span alone written. Its second users: a slider of the
 * same place; a part under a popup and under the keyboard's band while the value changes, the cover gone in the same
 * pass; a composing between two passes; the strips of 18 bits; the rest of the paper over a drawn background, a bar
 * that shrinks showing the picture. After each pass on a panel behind a bus (16 bits straight, 18 through the strips:
 * PANEL_BITS, by the Makefile): no pixel written twice, and the panel as a fresh paint composes the tree.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_vscreen.h"
#include "panel.h"

static uint32_t       colors[4];
static panel_t       *panel;
static dgx_screen_t  *memory, *map;
static uint8_t        buffer[PANEL_W * 3 * 16];
static dgx_ui_paint_t paint;
static bool           with_map;

static void draw_map(dgx_ui_paint_t *p, int x, int y, int w, int h, void *ctx)
{
    dgx_vscreen_region_to_screen(p->scr, x - p->ox, y - p->oy, (dgx_screen_t *)ctx, x, y, w, h, 0);
}

static void paint_anew(void)
{
    paint = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
#if PANEL_BITS == 18
    paint.strips = &dgx_ui_strips;
#endif
    if (with_map) dgx_ui_background_set(&paint, draw_map, map);
}

/* the panel against the tree as a fresh paint composes it, the band's rows left out; and no pixel twice */
static int twice_or_off(dgx_ui_t *ui, uint32_t now_ms)
{
    const dgx_ui_rect_t band  = paint.cover;
    dgx_ui_paint_t      fresh = {.scr = memory, .colors = colors, .color_count = 4, .now_ms = now_ms};
    if (with_map) dgx_ui_background_set(&fresh, draw_map, map);
    dgx_fill_rectangle(memory, 0, 0, PANEL_W, PANEL_H, colors[3]);
    dgx_ui_paint_area_to(ui, &fresh, memory, NULL, 0, 0, PANEL_W, PANEL_H);
    dgx_ui_paint_free(&fresh);
    int off = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) {
            const bool in_band = x >= band.x && x < band.x + band.w && y >= band.y && y < band.y + band.h;
            off += !in_band && panel->mem[y][x] != dgx_get_pixel(memory, x, y);
        }
    return panel_twice(panel) + off;
}

static int pass(dgx_ui_t *ui, uint32_t now_ms)
{
    panel_count_from_now(panel);
    dgx_ui_paint(ui, &paint, now_ms);
    return twice_or_off(ui, now_ms);
}

/* the pixels the last pass wrote are `count`: straight on 16 bits; through the strips of 18 bits a widget is drawn whole
   into the buffer and sent once, so there the count is not asked */
static bool wrote(int count)
{
    return PANEL_BITS != 16 || panel_written(panel) == count;
}

static uint32_t panel_color(int k)
{
    static dgx_screen_t *probe;
    if (!probe) probe = dgx_vscreen_init(1, 1, PANEL_BITS, DgxScreenRGB);
    dgx_set_pixel(probe, 0, 0, colors[k]);
    return dgx_get_pixel(probe, 0, 0);
}

/* the pixels of a colour in a widget's box */
static int count_in(const dgx_ui_node_t *n, uint32_t color)
{
    int count = 0;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x) count += y < PANEL_H && x < PANEL_W && panel->mem[y][x] == color;
    return count;
}

/* the program's memory */
static int32_t level;

static const dgx_ui_widget_t column_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};

static dgx_ui_node_t nodes[6];
static dgx_ui_t      tree;
static int           make_with(const dgx_ui_widget_t *a, const dgx_ui_widget_t *b, int *second)
{
    dgx_ui_init(&tree, nodes, 6, 1);
    dgx_ui_add(&tree, -1, &column_w);
    const int i = dgx_ui_add(&tree, 0, a);
    if (b) *second = dgx_ui_add(&tree, 0, b);
    paint_anew();
    return i;
}
static void done(void)
{
    dgx_ui_paint_free(&paint), dgx_ui_free(&tree);
}

int main(void)
{
    panel  = panel_make();
    memory = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    map    = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    colors[0] = dgx_screen_rgb(&panel->sbus.scr, 16, 20, 28), colors[1] = dgx_screen_rgb(&panel->sbus.scr, 232, 232, 222);
    colors[2] = dgx_screen_rgb(&panel->sbus.scr, 44, 60, 86), colors[3] = dgx_screen_rgb(&panel->sbus.scr, 240, 160, 60);
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) dgx_set_pixel(map, x, y, (x / 4 + y / 4) % 2 ? colors[2] : colors[0]);
    const uint32_t accent = panel_color(3), fill = panel_color(2), ink = panel_color(1);

    /* from each edge: the filled part where it should be, a fifth of the box; the part that moves alone written */
    for (int from = DGX_UI_FROM_LEFT; from <= DGX_UI_FROM_TOP; ++from) {
        const dgx_ui_level_look_t params = {.fill = 3, .track = 2, .from = (uint8_t)from};
        const dgx_ui_look_t       look   = {.draw = dgx_ui_draw_level, .params = &params, .fill = DGX_UI_PAPER};
        const bool                across = from == DGX_UI_FROM_LEFT || from == DGX_UI_FROM_RIGHT;
        const dgx_ui_widget_t     bar_w  = {.kind = DGX_UI_OWN, .value = &level, .max = 100, .want_w = across ? 0 : 20, .want_h = across ? 10 : 50, .look = &look};
        level                            = 20;
        const int i                      = make_with(&bar_w, NULL, NULL);
        CHECK(pass(&tree, 0) == 0);
        const dgx_ui_node_t *n      = dgx_ui_node(&tree, i);
        const int            length = across ? n->w : n->h, other = across ? n->h : n->w;
        CHECK(count_in(n, accent) == length * 20 / 100 * other && count_in(n, fill) == (length - length * 20 / 100) * other);
        /* at the edge it fills from */
        const int ex = from == DGX_UI_FROM_RIGHT ? n->x + n->w - 1 : n->x, ey = from == DGX_UI_FROM_BOTTOM ? n->y + n->h - 1 : n->y;
        CHECK(panel->mem[ey][ex] == accent);
        level = 60;
        CHECK(pass(&tree, 10) == 0 && wrote((length * 60 / 100 - length * 20 / 100) * other));
        level = 30;
        CHECK(pass(&tree, 20) == 0 && wrote((length * 60 / 100 - length * 30 / 100) * other));
        done();
    }

    /* a value past max and below min, any int32 in the place: full and empty, the memory left; max equal to min and
       below it: empty; a box of no width: nothing */
    {
        static const dgx_ui_widget_t bar_w   = {.kind = DGX_UI_OWN, .value = &level, .max = 100, .want_h = 10, .look = &dgx_ui_look_level};
        static const dgx_ui_widget_t same_w  = {.kind = DGX_UI_OWN, .value = &level, .min = 5, .max = 5, .want_h = 10, .look = &dgx_ui_look_level};
        static const dgx_ui_widget_t below_w = {.kind = DGX_UI_OWN, .value = &level, .min = 9, .max = 1, .want_h = 10, .look = &dgx_ui_look_level};
        level       = 2147483647;
        int       k = 0;
        const int i = make_with(&bar_w, &same_w, &k);
        CHECK(pass(&tree, 0) == 0 && count_in(dgx_ui_node(&tree, i), fill) == 0 && count_in(dgx_ui_node(&tree, k), accent) == 0);
        level = -2147483647 - 1;
        CHECK(pass(&tree, 10) == 0 && count_in(dgx_ui_node(&tree, i), accent) == 0 && level == -2147483647 - 1);
        done();
        level = 7;
        make_with(&below_w, NULL, NULL);
        CHECK(pass(&tree, 0) == 0 && count_in(dgx_ui_node(&tree, 1), accent) == 0);
        done();
        static const dgx_ui_widget_t row_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 10};
        static const dgx_ui_widget_t thin_w = {.kind = DGX_UI_OWN, .value = &level, .max = 100, .want_w = 0, .look = &dgx_ui_look_level};
        dgx_ui_init(&tree, nodes, 6, 1);
        dgx_ui_add(&tree, -1, &row_w), dgx_ui_add(&tree, 0, &thin_w);
        paint_anew();
        CHECK(pass(&tree, 0) == 0 && dgx_ui_node(&tree, 1)->w == 0);
        done();
    }

    /* the widest box with the widest range: the product fits, the boundary where the arithmetic puts it */
    {
        static const dgx_ui_widget_t wide_w = {.kind = DGX_UI_OWN, .value = &level, .min = -32768, .max = 32767, .want_w = 32767, .want_h = 10, .look = &dgx_ui_look_level};
        level       = -32768 + 100;
        const int i = make_with(&wide_w, NULL, NULL); /* 100 * 32767 / 65535: 49 pixels filled */
        CHECK(pass(&tree, 0) == 0);
        const dgx_ui_node_t *n = dgx_ui_node(&tree, i);
        CHECK(n->w == 32767 && panel->mem[n->y][n->x + 48] == accent && panel->mem[n->y][n->x + 49] == fill);
        level = 32766; /* all of the panel filled */
        CHECK(pass(&tree, 10) == 0 && panel->mem[n->y][PANEL_W - 1] == accent);
        done();
    }

    /* bands: the filled part in the colour of the value's band, all of it written when the band changes */
    {
        static const dgx_ui_band_t       bands[] = {{.below = 20, .color = 1}, {.below = 50, .color = 2}};
        static const dgx_ui_level_look_t params  = {.fill = 3, .track = 0, .from = DGX_UI_FROM_LEFT, .band_count = 2, .bands = bands};
        static const dgx_ui_look_t       look    = {.draw = dgx_ui_draw_level, .params = &params, .fill = DGX_UI_PAPER};
        static const dgx_ui_widget_t     bar_w   = {.kind = DGX_UI_OWN, .value = &level, .max = 100, .want_h = 10, .look = &look};
        level                                    = 10;
        const int            i                   = make_with(&bar_w, NULL, NULL);
        const dgx_ui_node_t *n                   = dgx_ui_node(&tree, i);
        CHECK(pass(&tree, 0) == 0 && count_in(n, ink) == n->w * 10 / 100 * n->h);
        level = 15; /* the same band: the part that grew alone */
        CHECK(pass(&tree, 10) == 0 && wrote((n->w * 15 / 100 - n->w * 10 / 100) * n->h));
        level = 30; /* another band: all of the filled part */
        CHECK(pass(&tree, 20) == 0 && count_in(n, fill) == n->w * 30 / 100 * n->h && wrote(n->w * 30 / 100 * n->h));
        level = 20; /* at a band's `below`: the next band */
        CHECK(pass(&tree, 25) == 0 && count_in(n, fill) == n->w * 20 / 100 * n->h);
        level = 90; /* past the last band: bar */
        CHECK(pass(&tree, 30) == 0 && count_in(n, accent) == n->w * 90 / 100 * n->h);
        done();
    }

    /* bands by place: each part in the band its place stands for; a value moved writes the span alone, no colour
       moves */
    {
        static const dgx_ui_band_t       bands[] = {{.below = 30, .color = 1}, {.below = 70, .color = 0}};
        static const dgx_ui_level_look_t params  = {.fill = 3, .track = 2, .from = DGX_UI_FROM_LEFT, .band_count = 2, .bands_by = DGX_UI_BANDS_BY_PLACE, .bands = bands};
        static const dgx_ui_look_t       look    = {.draw = dgx_ui_draw_level, .params = &params, .fill = DGX_UI_PAPER};
        static const dgx_ui_widget_t     bar_w   = {.kind = DGX_UI_OWN, .value = &level, .max = 100, .want_h = 10, .look = &look};
        level                                    = 100;
        const int            i                   = make_with(&bar_w, NULL, NULL);
        const dgx_ui_node_t *n                   = dgx_ui_node(&tree, i);
        const uint32_t       paper               = panel_color(0);
        CHECK(pass(&tree, 0) == 0 && count_in(n, ink) == n->w * 30 / 100 * n->h && count_in(n, paper) == (n->w * 70 / 100 - n->w * 30 / 100) * n->h);
        CHECK(count_in(n, accent) == (n->w - n->w * 70 / 100) * n->h);
        level = 50;
        CHECK(pass(&tree, 10) == 0 && wrote((n->w - n->w * 50 / 100) * n->h) && count_in(n, ink) == n->w * 30 / 100 * n->h);
        level = 60;
        CHECK(pass(&tree, 20) == 0 && wrote((n->w * 60 / 100 - n->w * 50 / 100) * n->h));
        done();
    }

    /* the second users, on the paper and over a drawn background */
    for (int pictured = 0; pictured < 2; ++pictured) {
        with_map = pictured;
        static const dgx_ui_level_look_t papered = {.fill = 3, .track = DGX_UI_PAPER, .from = DGX_UI_FROM_RIGHT};
        static const dgx_ui_look_t       look    = {.draw = dgx_ui_draw_level, .params = &papered, .fill = DGX_UI_PAPER};
        static const dgx_ui_widget_t     bar_w   = {.kind = DGX_UI_OWN, .value = &level, .max = 100, .want_h = 10, .look = &look};
        static const dgx_ui_widget_t slider_w = {.kind = DGX_UI_SLIDER, .value = &level, .max = 100, .want_h = 10, .look = &dgx_ui_look_slider};
        int                          s        = 0;
        level                                 = 50;
        const int            i                = make_with(&bar_w, &slider_w, &s);
        const dgx_ui_node_t *n                = dgx_ui_node(&tree, i);
        CHECK(pass(&tree, 0) == 0);
        /* a slider of the same place moved by the key: the bar follows; the bar shrinks over the picture */
        dgx_ui_focus(&tree, s);
        CHECK(dgx_ui_key(&tree, DGX_UI_KEY_LESS) && level == 49 && pass(&tree, 10) == 0);
        level = 10;
        CHECK(pass(&tree, 20) == 0);
        /* half under a popup while the value changes, the popup closed in the same pass */
        static dgx_ui_node_t         popup_nodes[1];
        static const dgx_ui_widget_t frame_w = {.kind = DGX_UI_FRAME, .look = &dgx_ui_look_frame};
        dgx_ui_t                     popup;
        dgx_ui_init(&popup, popup_nodes, 1, 1), dgx_ui_add(&popup, -1, &frame_w);
        CHECK(dgx_ui_popup_open(&tree, &popup, n->x + n->w / 2, 0, PANEL_W, n->y + n->h, 0));
        CHECK(pass(&tree, 30) == 0);
        level = 80;
        CHECK(pass(&tree, 40) == 0);
        level = 20;
        dgx_ui_popup_close(&tree);
        CHECK(pass(&tree, 50) == 0);
        dgx_ui_free(&popup);
        /* under the keyboard's band while it changes, the band gone in the same pass */
        paint.cover = dgx_ui_rect(0, 0, PANEL_W, n->y + n->h / 2);
        CHECK(pass(&tree, 60) == 0);
        level       = 70;
        CHECK(pass(&tree, 70) == 0);
        level       = 35;
        paint.cover = dgx_ui_rect(0, 0, 0, 0);
        CHECK(pass(&tree, 80) == 0);
        /* a composing between two passes, as a mirror does it, and the value moved after */
        dgx_ui_paint_area_to(&tree, &paint, memory, NULL, 0, 0, PANEL_W, PANEL_H);
        level = 65;
        CHECK(pass(&tree, 90) == 0);
        done();
    }
    CHECK_DONE();
}
