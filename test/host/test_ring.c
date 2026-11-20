/*
 * The ring scale, dgx_ui_look_ring. Every value from min to max by one, up and then down: the pixels a step writes are
 * exactly those whose colour changes, the panel the same value drawn whole, and going up no pixel of the arc turns off.
 * An oracle of its own: each pixel's class by atan2 and hypot in doubles against the panel, only the pixels within a
 * hundredth of a pixel of a ray or a circle let off. A full circle at max: its four quarters of one count, for an odd
 * side and an even one, at points of 1, 2 and 3. A disc, a width of 0, a box of one pixel, a box wider than high; a
 * sweep of 360, of 10 and counter-clockwise, a start of 0 and past 360; max at min and below it; a value out of the
 * range; bands by place and by the value shown, a value at a band's edge. Children inside, one lying over the ring:
 * every pixel of the box written once by the ring and its children together; a child hidden and shown. Its second users:
 * two rings of one place and a slider of it; a part under a popup and under the keyboard's band while the value
 * changes, the cover gone in the same pass; a composing between two passes; a drawn background. On a panel behind a bus,
 * 16 bits straight or 18 through the strips (PANEL_BITS, by the Makefile).
 */
#include <math.h>
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t       colors[4];
static panel_t       *panel;
static dgx_screen_t  *memory, *map;
static uint8_t        buffer[PANEL_W * 3 * 16];
static dgx_ui_paint_t paint;
static bool           with_map;
static uint32_t       paper_c, ink_c, fill_c, accent_c;

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

/* no pixel twice, and the panel as a fresh paint composes the tree; the band's rows left out */
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

/* a pass over a tree with a label: over a drawn background straight its letters are drawn over the picture laid for
   their field, twice as the painter means them, so there only the panel is compared */
static int pass_letters(dgx_ui_t *ui, uint32_t now_ms)
{
    const int off = pass(ui, now_ms);
    return with_map && PANEL_BITS == 16 ? off - panel_twice(panel) : off;
}

static uint32_t panel_color(int k)
{
    static dgx_screen_t *probe;
    if (!probe) probe = dgx_vscreen_init(1, 1, PANEL_BITS, DgxScreenRGB);
    dgx_set_pixel(probe, 0, 0, colors[k]);
    return dgx_get_pixel(probe, 0, 0);
}

static int32_t level, other;

static dgx_ui_node_t nodes[8];
static dgx_ui_t      tree;
static const dgx_ui_widget_t free_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN}; /* a column: it fills around its child, as a box of neither fills under it too */

/* a tree of one ring at x, y of w by h pixels, unit `point` */
static int make_ring(const dgx_ui_widget_t *ring_w, int point)
{
    dgx_ui_init(&tree, nodes, 8, point);
    dgx_ui_add(&tree, -1, &free_w);
    paint_anew();
    return dgx_ui_add(&tree, 0, ring_w);
}
static void done(void)
{
    dgx_ui_paint_free(&paint), dgx_ui_free(&tree);
}

/* the class an exact geometry gives a pixel: -1 too near a ray or a circle to say; else the colour index, or 0xff the
   paper. The angles are the look's, a 64th of a degree as it takes them */
static int exact_class(const dgx_ui_node_t *n, const dgx_ui_ring_look_t *p, int point, int32_t value, int x, int y)
{
    const dgx_ui_widget_t *w    = n->widget;
    const double           side = n->w < n->h ? n->w : n->h, outer = side / 2, inner = outer - p->width * point;
    const double           dx = x + 0.5 - (n->x + n->w / 2.0), dy = y + 0.5 - (n->y + n->h / 2.0), far = hypot(dx, dy);
    const double           near = 0.01;
    if (fabs(far - outer) < near || (inner > 0 && fabs(far - inner) < near)) return -1;
    if (far > outer || (inner > 0 && far <= inner)) return 0xff;
    int start = p->start, sweep = p->sweep;
    const bool mirrored = sweep < 0;
    if (mirrored) start = -start, sweep = -sweep;
    if (sweep > 360) sweep = 360;
    /* the pixel's turn clockwise from the start, in degrees, mirrored for a sweep counter-clockwise */
    double angle = atan2(mirrored ? -dx : dx, -dy) * 180.0 / M_PI;
    double turn  = fmod(angle - start + 720.0, 360.0);
    /* a ray of the look at a turn from the start, and the pixel's distance from it on its side */
    double rays[40];
    int    ray_count = 0;
    rays[ray_count++] = 0, rays[ray_count++] = sweep;
    const int range = w->max - w->min;
    const double value_turn = range > 0 ? floor((double)sweep * 64 * (value - w->min) / range) / 64 : 0;
    if (value > w->min && value < w->max) rays[ray_count++] = value_turn;
    for (int k = 0; k < p->band_count && ray_count < 40; ++k)
        if (p->bands_by == DGX_UI_BANDS_BY_PLACE && p->bands[k].below > w->min && p->bands[k].below < w->max)
            rays[ray_count++] = floor((double)sweep * 64 * (p->bands[k].below - w->min) / range) / 64;
    for (int k = 0; k < ray_count; ++k) {
        const double a = (start + rays[k]) * M_PI / 180.0, ux = mirrored ? -sin(a) : sin(a), uy = -cos(a);
        if (dx * ux + dy * uy >= 0 && fabs(dx * uy - dy * ux) < near) return -1;
    }
    if (far < near) return -1; /* the middle */
    if (sweep < 360 && turn > sweep) return 0xff;
    const bool on = range > 0 && (value >= w->max || (value > w->min && turn < value_turn));
    if (!on) return p->off;
    if (p->bands_by != DGX_UI_BANDS_BY_PLACE || !p->band_count) {
        for (int k = 0; k < p->band_count; ++k)
            if (value < p->bands[k].below) return p->bands[k].color;
        return p->on;
    }
    for (int k = 0; k < p->band_count; ++k) {
        if (p->bands[k].below <= w->min) continue;
        if (p->bands[k].below >= w->max) return p->bands[k].color;
        if (turn < floor((double)sweep * 64 * (p->bands[k].below - w->min) / range) / 64) return p->bands[k].color;
    }
    return p->on;
}

/* the pixels of the ring's box whose colour is not what the exact geometry says, those it cannot say left out */
static int off_the_exact(const dgx_ui_node_t *n, const dgx_ui_ring_look_t *p, int point, int32_t value)
{
    const uint32_t table[4] = {paper_c, ink_c, fill_c, accent_c};
    int            off      = 0;
    for (int y = n->y; y < n->y + n->h && y < PANEL_H; ++y)
        for (int x = n->x; x < n->x + n->w && x < PANEL_W; ++x) {
            const int cls = exact_class(n, p, point, value, x, y);
            if (cls < 0) continue;
            off += panel->mem[y][x] != table[cls == 0xff ? 0 : cls];
        }
    return off;
}

/* every value from min to max by one, up then down: a step writes exactly the pixels whose colour changes, the panel
   is the value drawn whole, and going up no pixel of the arc turns to the rest's colour */
static int steps_off(const dgx_ui_node_t *n, int i, const dgx_ui_ring_look_t *p, int point)
{
    static uint32_t before[PANEL_H][PANEL_W];
    int             faults = 0;
    uint32_t        now    = 100;
    for (int round = 0; round < 2; ++round)
        for (int k = 0; k <= 100; ++k) {
            const int32_t value = round ? 100 - k : k;
            memcpy(before, panel->mem, sizeof before);
            level = value;
            faults += pass(&tree, now += 10) != 0;
            int changed = 0, darkened = 0;
            for (int y = 0; y < PANEL_H; ++y)
                for (int x = 0; x < PANEL_W; ++x) {
                    changed += panel->mem[y][x] != before[y][x];
                    darkened += !round && before[y][x] != panel->mem[y][x] && before[y][x] != fill_c && panel->mem[y][x] == fill_c;
                }
            faults += darkened;
            if (PANEL_BITS == 16) faults += panel_written(panel) != changed;
            faults += off_the_exact(n, p, point, value) != 0;
        }
    (void)i;
    return faults;
}

int main(void)
{
    panel  = panel_make();
    memory = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    map    = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    colors[0] = dgx_screen_rgb(&panel->sbus.scr, 16, 20, 28), colors[1] = dgx_screen_rgb(&panel->sbus.scr, 232, 232, 222);
    colors[2] = dgx_screen_rgb(&panel->sbus.scr, 44, 60, 86), colors[3] = dgx_screen_rgb(&panel->sbus.scr, 240, 160, 60);
    paper_c = panel_color(0), ink_c = panel_color(1), fill_c = panel_color(2), accent_c = panel_color(3);
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) dgx_set_pixel(map, x, y, (x / 4 + y / 4) % 2 ? colors[2] : colors[0]);

    /* the steps and the oracle: the default look, bands by place, by the value shown, counter-clockwise, a full
       circle, a narrow sweep, a disc; boxes of an even side and an odd one */
    {
        static const dgx_ui_band_t      bands[] = {{.below = 30, .color = 1}, {.below = 70, .color = 0}};
        static const dgx_ui_ring_look_t looks[] = {
            {.on = 3, .off = 2, .paper = DGX_UI_PAPER, .width = 3, .start = -135, .sweep = 270, .bands_by = DGX_UI_BANDS_BY_PLACE},
            {.on = 3, .off = 2, .paper = DGX_UI_PAPER, .width = 5, .start = -135, .sweep = 270, .band_count = 2, .bands_by = DGX_UI_BANDS_BY_PLACE, .bands = bands},
            {.on = 3, .off = 2, .paper = DGX_UI_PAPER, .width = 5, .start = -135, .sweep = 270, .band_count = 2, .bands_by = DGX_UI_BANDS_BY_VALUE, .bands = bands},
            {.on = 3, .off = 2, .paper = DGX_UI_PAPER, .width = 4, .start = 45, .sweep = -200, .bands_by = DGX_UI_BANDS_BY_PLACE},
            {.on = 3, .off = 2, .paper = DGX_UI_PAPER, .width = 6, .start = 0, .sweep = 360, .bands_by = DGX_UI_BANDS_BY_PLACE},
            {.on = 3, .off = 2, .paper = DGX_UI_PAPER, .width = 6, .start = 400, .sweep = 10, .bands_by = DGX_UI_BANDS_BY_PLACE},
            {.on = 3, .off = 2, .paper = DGX_UI_PAPER, .width = 40, .start = -90, .sweep = 180, .bands_by = DGX_UI_BANDS_BY_PLACE},
        };
        for (size_t k = 0; k < sizeof looks / sizeof *looks; ++k)
            for (int odd = 0; odd < 2; ++odd) {
                const dgx_ui_look_t   look   = {.draw = dgx_ui_draw_ring, .params = &looks[k], .fill = -1};
                const dgx_ui_widget_t ring_w = {.kind = DGX_UI_OWN, .value = &level, .max = 100, .at_x = 10, .at_y = 2, .want_w = (int16_t)(58 + odd), .want_h = (int16_t)(58 + odd), .look = &look};
                level                        = 0;
                const int i                  = make_ring(&ring_w, 1);
                CHECK(pass(&tree, 0) == 0);
                const int faults = steps_off(dgx_ui_node(&tree, i), i, &looks[k], 1);
                CHECK(faults == 0);
                if (faults) fprintf(stderr, "  look %zu, side %d: %d faults over the steps\n", k, 58 + odd, faults);
                done();
            }
    }

    /* the ends exact: a pixel whose centre lies on the end ray is of the arc at max, no sliver left; on the start ray
       of the arc as soon as the value is past min */
    {
        static const dgx_ui_widget_t ring_w = {.kind = DGX_UI_OWN, .value = &level, .max = 100, .want_w = 60, .want_h = 60, .look = &dgx_ui_look_ring};
        level                               = 100;
        const int            i              = make_ring(&ring_w, 1);
        const dgx_ui_node_t *n              = dgx_ui_node(&tree, i);
        CHECK(pass(&tree, 0) == 0);
        /* 41 halves of a pixel right and down of the middle, at 135 degrees clockwise from the top: in the ring */
        const int cx2 = 2 * n->x + n->w, cy2 = 2 * n->y + n->h;
        const int ex = (cx2 + 41 - 1) / 2, ey = (cy2 + 41 - 1) / 2, sx = (cx2 - 41 - 1) / 2;
        CHECK(2 * ex + 1 - cx2 == 41 && 2 * ey + 1 - cy2 == 41 && panel->mem[ey][ex] == accent_c);
        level = 99;
        CHECK(pass(&tree, 10) == 0 && panel->mem[ey][ex] == fill_c);
        level = 0;
        CHECK(pass(&tree, 20) == 0 && panel->mem[ey][sx] == fill_c);
        level = 1;
        CHECK(pass(&tree, 30) == 0 && panel->mem[ey][sx] == accent_c);
        done();
    }

    /* a sweep of 0 is no scale: all of it paper, whatever the start; twenty bands by place, all of them read */
    {
        const int starts[3] = {0, 45, 90};
        for (int k = 0; k < 3; ++k)
            for (int odd = 0; odd < 2; ++odd) {
                const dgx_ui_ring_look_t none = {.on = 3, .off = 2, .paper = DGX_UI_PAPER, .width = 8, .start = (int16_t)starts[k], .sweep = 0};
                const dgx_ui_look_t      look = {.draw = dgx_ui_draw_ring, .params = &none, .fill = -1};
                const dgx_ui_widget_t ring_w = {.kind = DGX_UI_OWN, .value = &level, .max = 100, .want_w = (int16_t)(58 + odd), .want_h = (int16_t)(58 + odd), .look = &look};
                level                        = 100;
                const int i                  = make_ring(&ring_w, 1);
                CHECK(pass(&tree, 0) == 0);
                const dgx_ui_node_t *n = dgx_ui_node(&tree, i);
                int                  marked = 0;
                for (int y = n->y; y < n->y + n->h; ++y)
                    for (int x = n->x; x < n->x + n->w; ++x) marked += panel->mem[y][x] != paper_c;
                CHECK(marked == 0);
                done();
            }
        static dgx_ui_band_t many[20];
        for (int k = 0; k < 20; ++k) many[k] = (dgx_ui_band_t){.below = 5 * (k + 1), .color = (uint8_t)(k % 4)};
        const dgx_ui_ring_look_t twenty = {.on = 3, .off = 2, .paper = DGX_UI_PAPER, .width = 10, .start = 0, .sweep = 360, .band_count = 20, .bands_by = DGX_UI_BANDS_BY_PLACE, .bands = many};
        const dgx_ui_look_t      look   = {.draw = dgx_ui_draw_ring, .params = &twenty, .fill = -1};
        const dgx_ui_widget_t    ring_w = {.kind = DGX_UI_OWN, .value = &level, .max = 99, .want_w = 60, .want_h = 60, .look = &look};
        level                           = 99;
        const int i                     = make_ring(&ring_w, 1);
        CHECK(pass(&tree, 0) == 0 && off_the_exact(dgx_ui_node(&tree, i), &twenty, 1, level) == 0);
        level = 47;
        CHECK(pass(&tree, 10) == 0 && off_the_exact(dgx_ui_node(&tree, i), &twenty, 1, level) == 0);
        done();
    }

    /* a full circle at max: four quarters of one count, an odd side and an even one, at points of 1, 2 and 3 */
    for (int point = 1; point <= 3; ++point)
        for (int odd = 0; odd < 2; ++odd) {
            static const dgx_ui_ring_look_t full = {.on = 3, .off = 2, .paper = DGX_UI_PAPER, .width = 4, .start = 0, .sweep = 360};
            const dgx_ui_look_t             look = {.draw = dgx_ui_draw_ring, .params = &full, .fill = -1};
            const int                       points = 60 / point + odd; /* a side odd at points of 1 and 3; at 2 it cannot be */
            const dgx_ui_widget_t ring_w = {.kind = DGX_UI_OWN, .value = &level, .max = 100, .want_w = (int16_t)points, .want_h = (int16_t)points, .look = &look};
            level                        = 100;
            const int i                  = make_ring(&ring_w, point);
            CHECK(pass(&tree, 0) == 0);
            const dgx_ui_node_t *n = dgx_ui_node(&tree, i);
            int                  quarter[4] = {0};
            for (int y = n->y; y < n->y + n->h; ++y)
                for (int x = n->x; x < n->x + n->w; ++x)
                    if (panel->mem[y][x] == accent_c && 2 * x + 1 != 2 * n->x + n->w && 2 * y + 1 != 2 * n->y + n->h) /* the middle row and column apart */
                        quarter[(2 * x + 1 > 2 * n->x + n->w) + 2 * (2 * y + 1 > 2 * n->y + n->h)] += 1;
            CHECK(quarter[0] > 0 && quarter[0] == quarter[1] && quarter[1] == quarter[2] && quarter[2] == quarter[3]);
            done();
        }

    /* a width of 0, a box of one pixel, a box wider than high; max at min and below it; values out of the range */
    {
        static const dgx_ui_ring_look_t thin = {.on = 3, .off = 2, .paper = DGX_UI_PAPER, .width = 0, .start = -135, .sweep = 270};
        const dgx_ui_look_t             look = {.draw = dgx_ui_draw_ring, .params = &thin, .fill = -1};
        const dgx_ui_widget_t widths[] = {
            {.kind = DGX_UI_OWN, .value = &level, .max = 100, .want_w = 40, .want_h = 40, .look = &look},
            {.kind = DGX_UI_OWN, .value = &level, .max = 100, .want_w = 1, .want_h = 1, .look = &dgx_ui_look_ring},
            {.kind = DGX_UI_OWN, .value = &level, .max = 100, .want_w = 90, .want_h = 30, .look = &dgx_ui_look_ring},
            {.kind = DGX_UI_OWN, .value = &level, .min = 5, .max = 5, .want_w = 40, .want_h = 40, .look = &dgx_ui_look_ring},
            {.kind = DGX_UI_OWN, .value = &level, .min = 9, .max = 1, .want_w = 40, .want_h = 40, .look = &dgx_ui_look_ring},
        };
        for (size_t k = 0; k < sizeof widths / sizeof *widths; ++k) {
            level       = 50;
            const int i = make_ring(&widths[k], 1);
            CHECK(pass(&tree, 0) == 0);
            level = 2147483647;
            CHECK(pass(&tree, 10) == 0);
            level = -2147483647 - 1;
            CHECK(pass(&tree, 20) == 0);
            if (k == 0) CHECK(off_the_exact(dgx_ui_node(&tree, i), &thin, 1, level) == 0);
            done();
        }
    }

    /* children inside, one lying over the ring: every pixel of the box once by the ring and its children together; a
       child hidden and shown; the value moved while a child's text changes */
    {
        static dgx_ui_font_variant_t variant[1];
        static dgx_ui_text_style_t   style = {.variants = variant, .count = 1, .align = DGX_FONT_ALIGN_CENTER};
        variant[0]                         = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
        static char                  number[8] = "42";
        static dgx_ui_textbox_t      box;
        dgx_ui_textbox_init(&box, number, sizeof number, 1, &style);
        strcpy(number, "42");
        static const dgx_ui_widget_t ring_w   = {.kind = DGX_UI_OWN, .flags = DGX_UI_COLUMN, .pad = 12, .value = &level, .max = 100, .at_x = 15, .at_y = 2, .want_w = 60, .want_h = 60, .look = &dgx_ui_look_ring};
        static const dgx_ui_widget_t label_w  = {.kind = DGX_UI_LABEL, .text = &box};
        static const dgx_ui_widget_t over_w   = {.kind = DGX_UI_BOX, .at_x = 0, .want_w = 8, .want_h = 10, .look = &dgx_ui_look_frame};
        for (int pictured = 0; pictured < 2; ++pictured) {
            with_map    = pictured;
            level       = 30;
            const int i = make_ring(&ring_w, 1);
            const int t = dgx_ui_add(&tree, i, &label_w), o = dgx_ui_add(&tree, i, &over_w);
            CHECK(pass_letters(&tree, 0) == 0);
            const dgx_ui_node_t *n = dgx_ui_node(&tree, i);
            int                  unwritten = 0;
            for (int y = n->y; y < n->y + n->h; ++y)
                for (int x = n->x; x < n->x + n->w; ++x) unwritten += panel->writes[y][x] == 0;
            CHECK(unwritten == 0);
            level = 60;
            dgx_ui_text_set(&tree, t, "61");
            CHECK(pass_letters(&tree, 10) == 0);
            dgx_ui_show(&tree, o, false);
            CHECK(pass_letters(&tree, 20) == 0);
            dgx_ui_show(&tree, o, true);
            level = 10;
            CHECK(pass_letters(&tree, 30) == 0);
            done();
        }
        with_map = false;
    }

    /* the second users, on the paper and over a drawn background: two rings of one place and a slider of it; a part
       under a popup and under the band while the value changes, the cover gone in the same pass; a composing between
       two passes */
    for (int pictured = 0; pictured < 2; ++pictured) {
        with_map = pictured;
        static const dgx_ui_widget_t column_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .pad = 1, .gap = 2};
        static const dgx_ui_widget_t ring_w   = {.kind = DGX_UI_OWN, .value = &level, .max = 100, .want_w = 40, .want_h = 40, .look = &dgx_ui_look_ring};
        static const dgx_ui_widget_t slider_w = {.kind = DGX_UI_SLIDER, .value = &level, .max = 100, .want_w = 10, .want_h = 40, .look = &dgx_ui_look_slider};
        dgx_ui_init(&tree, nodes, 8, 1);
        dgx_ui_add(&tree, -1, &column_w);
        const int a = dgx_ui_add(&tree, 0, &ring_w), s = dgx_ui_add(&tree, 0, &slider_w);
        dgx_ui_add(&tree, 0, &ring_w);
        paint_anew();
        level = 50;
        CHECK(pass(&tree, 0) == 0);
        dgx_ui_focus(&tree, s);
        CHECK(dgx_ui_key(&tree, DGX_UI_KEY_LESS) && level == 49 && pass(&tree, 10) == 0);
        const dgx_ui_node_t         *n = dgx_ui_node(&tree, a);
        static dgx_ui_node_t         popup_nodes[1];
        static const dgx_ui_widget_t frame_w = {.kind = DGX_UI_FRAME, .look = &dgx_ui_look_frame};
        dgx_ui_t                     popup;
        dgx_ui_init(&popup, popup_nodes, 1, 1), dgx_ui_add(&popup, -1, &frame_w);
        CHECK(dgx_ui_popup_open(&tree, &popup, n->x + n->w / 2, n->y + 5, 40, 20, 0) && pass(&tree, 20) == 0);
        level = 80;
        CHECK(pass(&tree, 30) == 0);
        level = 20;
        dgx_ui_popup_close(&tree);
        CHECK(pass(&tree, 40) == 0);
        dgx_ui_free(&popup);
        paint.cover = dgx_ui_rect(0, 0, PANEL_W, n->y + n->h / 2);
        CHECK(pass(&tree, 50) == 0);
        level = 70;
        CHECK(pass(&tree, 60) == 0);
        level       = 35;
        paint.cover = dgx_ui_rect(0, 0, 0, 0);
        CHECK(pass(&tree, 70) == 0);
        dgx_ui_paint_area_to(&tree, &paint, memory, NULL, 0, 0, PANEL_W, PANEL_H);
        level = 65;
        CHECK(pass(&tree, 80) == 0);
        done();
    }
    (void)other;
    CHECK_DONE();
}
