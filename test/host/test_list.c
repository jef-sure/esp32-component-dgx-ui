/*
 * The look of a list on the counting panel: its rows the lines of its text box, the row picked on a bar; drawn
 * whole, every pixel once; another pick writes the row picked before and the row picked now, once, and nothing else;
 * the focus the bar of the row picked; another text all rows; rows past the text paper; the rows numbered from min;
 * the screen composed in strips as drawn. The look of a dropdown: the option chosen on its face, an arrow; every
 * pixel of its box once in every state and for every option.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800}; /* paper, ink, a fill, the accent: the bar; focused, the ink */

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;
static char                  fruit_buf[40];
static dgx_ui_textbox_t      fruit_box;

static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};
/* three rows of 14 pixels, numbered 5, 6, 7 */
static int32_t list_w_place; /* its value */
static const dgx_ui_widget_t list_w = {.kind = DGX_UI_LIST, .value = &list_w_place, .text = &fruit_box, .min = 5, .max = 7, .want_h = 42, .look = &dgx_ui_look_list};

/* a dropdown of the same options, numbered 1, 2, 3 */
static char                  dd_buf[40];
static dgx_ui_textbox_t      dd_box;
static int32_t dd_w_place = 1; /* its value */
static const dgx_ui_widget_t dd_w = {.kind = DGX_UI_DROPDOWN, .value = &dd_w_place, .text = &dd_box, .min = 1, .max = 3, .want_h = 16, .look = &dgx_ui_look_dropdown};

static dgx_ui_node_t nodes[4];
static dgx_ui_t      ui;
static int           li, dd;

/* the change drawn: every pixel of the widget's box once, nothing else */
static bool once_in_box(panel_t *panel, dgx_ui_paint_t *pp, int i, uint32_t now)
{
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, pp, now);
    const dgx_ui_node_t *n = dgx_ui_node(&ui, i);
    return panel_twice(panel) == 0 && panel_written_outside(panel, n->x, n->y, n->x + n->w, n->y + n->h) == 0 &&
           panel_written(panel) == n->w * n->h;
}

/* pixels written in rows y0..y1 of the list's box, and outside them */
static void written(const panel_t *p, const dgx_ui_node_t *n, int y0, int y1, int *in, int *out)
{
    *in = *out = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x)
            if (p->writes[y][x]) *(x >= n->x && x < n->x + n->w && y >= y0 && y < y1 ? in : out) += 1;
}

/* the colour at the right end of a row: no letter reaches there */
static uint16_t row_end(const panel_t *p, const dgx_ui_node_t *n, int r)
{
    return p->mem[n->y + r * 14 + 7][n->x + n->w - 1];
}

static int differ(const panel_t *p, dgx_screen_t *v)
{
    int n = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) n += p->mem[y][x] != dgx_get_pixel(v, x, y);
    return n;
}

int main(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    style      = (dgx_ui_text_style_t){.variants = variant, .count = 1};
    dgx_ui_textbox_init(&fruit_box, fruit_buf, sizeof fruit_buf, 1, &style);
    panel_t       *panel = panel_make();
    dgx_ui_paint_t pp    = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
    dgx_ui_init(&ui, nodes, 4, 1);
    dgx_ui_add(&ui, -1, &root_w);
    li = dgx_ui_add(&ui, 0, &list_w);
    dgx_ui_text_set(&ui, li, "Apple\nBanana\nCherry");
    dgx_ui_textbox_init(&dd_box, dd_buf, sizeof dd_buf, 1, &style);
    dd = dgx_ui_add(&ui, 0, &dd_w);
    dgx_ui_text_set(&ui, dd, "Apple\nBanana\nCherry");
    dgx_ui_set_value(&ui, li, 6);

    /* drawn whole: every pixel once; the bar under Banana, the other rows paper */
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &pp, 0);
    const dgx_ui_node_t *n = dgx_ui_node(&ui, li);
    CHECK(n->h == 42 && panel_twice(panel) == 0 && panel_written(panel) == PANEL_W * PANEL_H);
    CHECK(row_end(panel, n, 0) == colors[0] && row_end(panel, n, 1) == colors[3] && row_end(panel, n, 2) == colors[0]);
    /* Banana on the bar in colour 0: some pixel of its row is that */
    {
        int ink = 0;
        for (int y = n->y + 14; y < n->y + 28; ++y)
            for (int x = n->x; x < n->x + 40; ++x) ink += panel->mem[y][x] == colors[0];
        CHECK(ink > 0);
    }

    /* Apple in row 0, in the middle of the row, as the font draws it there */
    {
        dgx_screen_t *e    = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
        dgx_font_t   *t12  = TerminusTTFMedium12();
        const int     cell = t12->yBottomMax - t12->yOffsetLowest, base = n->y + (14 - cell) / 2 - t12->yOffsetLowest, pad = cell / 4;
        dgx_fill_rectangle(e, 0, 0, PANEL_W, PANEL_H, colors[0]);
        const dgx_font_context_t ec = {.scr = e, .font = t12, .color = colors[1], .bg = colors[0], .field = n->w - 2 * pad};
        dgx_font_draw(&ec, n->x + pad, base, "Apple"); /* a quarter of a line in, as the dropdown's face */
        int off = 0;
        for (int y = n->y; y < n->y + 14; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) off += panel->mem[y][x] != dgx_get_pixel(e, x, y);
        CHECK(off == 0);
        e->destroy(&e);
    }

    /* Cherry picked: rows 1 and 2 written once, row 0 and the rest not at all */
    int in, out;
    panel_count_from_now(panel);
    dgx_ui_set_value(&ui, li, 7);
    dgx_ui_paint(&ui, &pp, 1);
    written(panel, n, n->y + 14, n->y + 42, &in, &out);
    CHECK(panel_twice(panel) == 0 && out == 0 && in == n->w * 28);
    CHECK(row_end(panel, n, 1) == colors[0] && row_end(panel, n, 2) == colors[3]);

    /* focused: the row picked alone, its bar of the focus */
    panel_count_from_now(panel);
    dgx_ui_focus(&ui, li);
    dgx_ui_paint(&ui, &pp, 2);
    written(panel, n, n->y + 28, n->y + 42, &in, &out);
    CHECK(panel_twice(panel) == 0 && out == 0 && in == n->w * 14 && row_end(panel, n, 2) == colors[1]);

    /* the keys move the pick: PREV to Banana, the bar of the focus with it */
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_PREV) && dgx_ui_value_of(dgx_ui_node(&ui, li)) == 6);
    dgx_ui_paint(&ui, &pp, 3);
    CHECK(row_end(panel, n, 1) == colors[1] && row_end(panel, n, 2) == colors[0]);

    /* disabled: all the rows written again, once, their text in colour 2 */
    panel_count_from_now(panel);
    dgx_ui_set_state(&ui, li, DGX_UI_DISABLED, true);
    dgx_ui_paint(&ui, &pp, 31);
    written(panel, n, n->y, n->y + 42, &in, &out);
    CHECK(panel_twice(panel) == 0 && out == 0 && in == n->w * 42);
    {
        int grey = 0;
        for (int y = n->y; y < n->y + 14; ++y)
            for (int x = n->x; x < n->x + 40; ++x) grey += panel->mem[y][x] == colors[2];
        CHECK(grey > 0);
    }
    dgx_ui_set_state(&ui, li, DGX_UI_DISABLED, false);
    dgx_ui_paint(&ui, &pp, 32);

    /* another text: all the rows, once; a text of two lines leaves the third row paper */
    panel_count_from_now(panel);
    dgx_ui_text_set(&ui, li, "Red\nGreen");
    dgx_ui_paint(&ui, &pp, 4);
    written(panel, n, n->y, n->y + 42, &in, &out);
    CHECK(panel_twice(panel) == 0 && out == 0 && in == n->w * 42);
    {
        int ink = 0;
        for (int y = n->y + 28; y < n->y + 42; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) ink += panel->mem[y][x] != colors[0];
        CHECK(ink == 0);
    }

    /* a finger on the third row picks 7 */
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(n->x + 5), .y = (int16_t)(n->y + 35), .down = true, .pressed = true});
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(n->x + 5), .y = (int16_t)(n->y + 35), .released = true});
    CHECK(dgx_ui_value_of(dgx_ui_node(&ui, li)) == 7);
    dgx_ui_paint(&ui, &pp, 5);

    /* the dropdown: Apple on its face, the arrow at the right; every state and every option written once in its box */
    {
        dgx_ui_focus(&ui, -1); /* the list's focus gone first: its bar drawn now, not with the dropdown's */
        dgx_ui_paint(&ui, &pp, 5);
        const dgx_ui_node_t *d = dgx_ui_node(&ui, dd);
        const int            ax = d->x + d->w - d->h / 2, ay = d->y + d->h / 2; /* the middle of the arrow's square */
        CHECK(panel->mem[ay][ax] == colors[1] && panel->mem[d->y][d->x + d->w - 1] == colors[2]);
        dgx_ui_set_value(&ui, dd, 3);
        CHECK(once_in_box(panel, &pp, dd, 6));
        dgx_ui_set_state(&ui, dd, DGX_UI_PRESSED, true);
        CHECK(once_in_box(panel, &pp, dd, 7) && panel->mem[d->y][d->x] == colors[3]);
        dgx_ui_set_state(&ui, dd, DGX_UI_PRESSED, false);
        dgx_ui_focus(&ui, dd);
        /* the focus: a frame of the ink round the box, the face inside it as it was */
        CHECK(once_in_box(panel, &pp, dd, 8) && panel->mem[d->y][d->x] == colors[1] && panel->mem[d->y + 1][d->x + 1] == colors[2]);
        dgx_ui_set_state(&ui, dd, DGX_UI_DISABLED, true);
        CHECK(once_in_box(panel, &pp, dd, 9) && panel->mem[d->y][d->x] == colors[0] && panel->mem[ay][ax] == colors[2]);
        dgx_ui_set_state(&ui, dd, DGX_UI_DISABLED, false);
        dgx_ui_paint(&ui, &pp, 10);
        /* Cherry on the face: the row of the text as the font draws it there */
        dgx_screen_t *e = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
        dgx_font_t   *t12 = TerminusTTFMedium12();
        const int     cell = t12->yBottomMax - t12->yOffsetLowest, base = d->y + (d->h - cell) / 2 - t12->yOffsetLowest, pad = cell / 4;
        dgx_fill_rectangle(e, 0, 0, PANEL_W, PANEL_H, colors[2]);
        const dgx_font_context_t ec = {.scr = e, .font = t12, .color = colors[1], .bg = colors[2]};
        dgx_font_draw(&ec, d->x + pad, base, "Cherry");
        const dgx_font_context_t m  = {.font = t12};
        const int                cw = dgx_font_bounds(&m, "Cherry", NULL, NULL);
        int                      off = 0;
        for (int y = base + t12->yOffsetLowest; y < base + t12->yBottomMax; ++y)
            for (int x = d->x + pad; x < d->x + pad + cw; ++x) off += panel->mem[y][x] != dgx_get_pixel(e, x, y);
        CHECK(off == 0);
        e->destroy(&e);
    }

    /* the screen composed in strips of 5 rows equals the screen drawn */
    {
        dgx_screen_t  *band = dgx_vscreen_init(PANEL_W, 5, 16, DgxScreenRGB);
        dgx_ui_paint_t pb   = {.scr = band, .colors = colors, .color_count = 4};
        int            off  = 0;
        for (int y0 = 0; y0 < PANEL_H; y0 += 5) {
            const int hh = PANEL_H - y0 < 5 ? PANEL_H - y0 : 5;
            pb.oy        = (int16_t)y0;
            dgx_ui_paint_area(&ui, &pb, 0, y0, PANEL_W, hh);
            for (int y = 0; y < hh; ++y)
                for (int x = 0; x < PANEL_W; ++x) off += dgx_get_pixel(band, x, y) != panel->mem[y0 + y][x];
        }
        CHECK(off == 0);
        band->destroy(&band);
    }
    /* and the whole as a fresh tree draws it */
    {
        dgx_screen_t  *v  = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
        dgx_ui_paint_t pv = {.scr = v, .colors = colors, .color_count = 4};
        dgx_ui_paint_area(&ui, &pv, 0, 0, PANEL_W, PANEL_H);
        CHECK(differ(panel, v) == 0);
        v->destroy(&v);
    }
    dgx_ui_free(&ui);
    free(panel);
    CHECK_DONE();
}
