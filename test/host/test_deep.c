/*
 * Drawing through the buffer in strips on a display behind a bus of 24 or 18 bits (PANEL_BITS, given by the
 * Makefile), where a field of text fills before it writes and blinks: the strips not named, or no buffer, a label
 * changed writes its field twice; named with a buffer, every widget without children is drawn whole in a strip over
 * the buffer and sent once, every pixel once, as a virtual screen of the same depth draws the same tree. A frame
 * holding a label (the frame straight, the label through the buffer); the look of lines with its letters appearing
 * through a buffer of one row, pass after pass, and still asking for time; one window a strip; a field with its caret;
 * a dropdown's list; the keyboard's band left alone; a look that remembers, over parts of several widths.
 */
#include <string.h>

#define PANEL_H 128
#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_dropdown.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

/* a place for the value of a widget made at run time, a new one each time, 0 in it */
static int32_t  place_pool[256];
static int      place_next;
static int32_t *new_place(void)
{
    int32_t *place = &place_pool[place_next++ % 256];
    *place         = 0;
    return place;
}

_Static_assert(PANEL_BITS == 18 || PANEL_BITS == 24, "built with -DPANEL_BITS=18 or 24");

static uint32_t colors[4] = {0x101820, 0xf0f0e8, 0x2c3c54, 0xf0a03c}; /* paper, ink, a fill, the accent: of 18 bits too, its bytes' low two bits clear */

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;

/* a tree: a column of a label, a frame holding a label, a field, a dropdown, the lines; two of it, for the panel and
   for the virtual screen it is compared with */
typedef struct {
    dgx_ui_node_t    nodes[10];
    dgx_ui_t         ui;
    char             b[5][48];
    dgx_ui_textbox_t t[5];
    dgx_ui_widget_t  label, inner, field, drop, lines;
    int              l, f, in, d, ln;
} tree_t;

static const dgx_ui_widget_t root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};
static const dgx_ui_widget_t frame_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2, .want_h = 18};

static void make(tree_t *t)
{
    memset(t, 0, sizeof *t);
    for (int k = 0; k < 5; ++k) dgx_ui_textbox_init(&t->t[k], t->b[k], sizeof t->b[k], k == 4 ? 2 : 1, &style);
    t->label = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &t->t[0], .want_h = 12};
    t->inner = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &t->t[1], .want_h = 12};
    t->field = (dgx_ui_widget_t){.kind = DGX_UI_INPUT, .text = &t->t[2], .want_h = 12, .look = &dgx_ui_look_input};
    t->drop  = (dgx_ui_widget_t){.kind = DGX_UI_DROPDOWN, .value = new_place(), .text = &t->t[3], .max = 2, .want_w = 40, .want_h = 10, .look = &dgx_ui_look_dropdown};
    t->lines = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &t->t[4], .want_h = 22, .look = &dgx_ui_look_lines};
    dgx_ui_init(&t->ui, t->nodes, 10, 1);
    dgx_ui_add(&t->ui, -1, &root_w);
    t->l = dgx_ui_add(&t->ui, 0, &t->label);
    t->f = dgx_ui_add(&t->ui, 0, &frame_w);
    t->in = dgx_ui_add(&t->ui, t->f, &t->inner);
    t->d  = dgx_ui_add(&t->ui, 0, &t->drop);
    t->ln = dgx_ui_add(&t->ui, 0, &t->lines);
    dgx_ui_text_set(&t->ui, t->l, "21.5 C");
    dgx_ui_text_set(&t->ui, t->in, "framed");
    strcpy(t->b[3], "one\ntwo\nthree");
    dgx_ui_text_set(&t->ui, t->ln, "a text of words that wraps");
}

static int differ(const panel_t *p, dgx_screen_t *v)
{
    int n = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) n += p->mem[y][x] != dgx_get_pixel(v, x, y);
    return n;
}

static int written_in(const panel_t *p, const dgx_ui_node_t *n, int *out)
{
    int in = 0;
    *out   = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x)
            if (p->writes[y][x]) (x >= n->x && x < n->x + n->w && y >= n->y && y < n->y + n->h) ? ++in : ++*out;
    return in;
}

/* a tree of one widget of the tick, the width of the panel; the passes it counted, whether its last part was drawn
   whole; a popup of a box */
typedef struct {
    dgx_ui_node_t   nodes[2], pop_nodes[1];
    dgx_ui_t        ui, pop;
    dgx_ui_look_t   look;
    dgx_ui_widget_t tick;
    int             count;
    bool            whole_last;
} ticks_t;

/* a look that counts the passes it remembers, in the last part not composing, and fills its box with the colour of
   the count: a strip that remembered when it should not is of another colour than the rest of the box */
static bool draw_tick(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    ticks_t             *t = (ticks_t *)look->params;
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    dgx_fill_rectangle(paint->scr, n->x - paint->ox, n->y - paint->oy, n->w, n->h, colors[2 + t->count % 2]);
    if (paint->part == paint->parts - 1) t->whole_last = paint->whole;
    if (paint->part == paint->parts - 1 && !paint->composing) ++t->count;
    return false;
}

static const dgx_ui_widget_t box_w = {.kind = DGX_UI_BOX, .look = &dgx_ui_look_box}, tick_root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};

static void make_ticks(ticks_t *t)
{
    memset(t, 0, sizeof *t);
    t->look = (dgx_ui_look_t){.draw = draw_tick, .params = t, .fill = -1};
    t->tick = (dgx_ui_widget_t){.kind = DGX_UI_BOX, .value = new_place(), .max = 1000, .want_h = 16, .look = &t->look};
    dgx_ui_init(&t->ui, t->nodes, 2, 1);
    dgx_ui_add(&t->ui, -1, &tick_root_w);
    dgx_ui_add(&t->ui, 0, &t->tick);
    dgx_ui_init(&t->pop, t->pop_nodes, 1, 1);
    dgx_ui_add(&t->pop, -1, &box_w);
}

/* passes of the tick's value changed: each pass the count one more on both screens, and the panel as the virtual
   screen */
static bool ticked(panel_t *panel, dgx_screen_t *v, ticks_t *a, dgx_ui_paint_t *pa, ticks_t *b, dgx_ui_paint_t *pv, uint32_t t)
{
    bool ok = true;
    for (int k = 0; k < 3; ++k, ++t) {
        const int was = a->count;
        dgx_ui_set_value(&a->ui, 1, was), dgx_ui_set_value(&b->ui, 1, was);
        dgx_ui_paint(&a->ui, pa, t), dgx_ui_paint(&b->ui, pv, t);
        ok = ok && a->count == was + 1 && b->count == a->count && differ(panel, v) == 0;
    }
    return ok;
}

int main(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    style      = (dgx_ui_text_style_t){.variants = variant, .count = 1, .flags = DGX_UI_TEXT_MIDDLE};
    panel_t       *panel = panel_make();
    dgx_screen_t  *v     = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    static uint8_t buffer[96 * 3 * 4]; /* four rows of the panel's width */
    static tree_t  a, b;

    /* a buffer given, the strips not named: drawn straight, the label changed writes its field twice, the blink; the
       strips named without a buffer: the same */
    make(&a);
    dgx_ui_paint_t pa = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
    dgx_ui_paint(&a.ui, &pa, 0);
    panel_count_from_now(panel);
    dgx_ui_text_set(&a.ui, a.l, "22.0 C");
    dgx_ui_paint(&a.ui, &pa, 1);
    CHECK(panel_twice(panel) > 0 && pa.strip == NULL);
    pa = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .strips = &dgx_ui_strips};
    panel_count_from_now(panel);
    dgx_ui_text_set(&a.ui, a.l, "21.5 C");
    dgx_ui_paint(&a.ui, &pa, 2);
    CHECK(panel_twice(panel) > 0 && pa.strip == NULL);
    dgx_ui_free(&a.ui);

    /* named with a buffer: drawn whole, every pixel once, as the virtual screen draws it */
    make(&a), make(&b);
    pa = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer, .strips = &dgx_ui_strips};
    /* the virtual screen it is compared with has them named too, and a buffer: a virtual screen does not blink, the
       strips draw nothing there */
    static uint8_t vbuffer[sizeof buffer];
    dgx_ui_paint_t pv = {.scr = v, .colors = colors, .color_count = 4, .buffer = vbuffer, .buffer_size = sizeof vbuffer, .strips = &dgx_ui_strips};
    panel_count_from_now(panel);
    CHECK(dgx_ui_paint(&a.ui, &pa, 0) == dgx_ui_paint(&b.ui, &pv, 0));
    CHECK(panel_twice(panel) == 0 && panel_written(panel) == PANEL_W * PANEL_H && differ(panel, v) == 0 && pa.buffer_used == 0);
    CHECK(pa.strip != NULL && pv.strip == NULL);
    /* the label changed: its box once, nothing else; one window a strip, four rows a strip */
    panel_count_from_now(panel);
    dgx_ui_text_set(&a.ui, a.l, "22.0 C"), dgx_ui_text_set(&b.ui, b.l, "22.0 C");
    dgx_ui_paint(&a.ui, &pa, 1), dgx_ui_paint(&b.ui, &pv, 1);
    {
        const dgx_ui_node_t *n = dgx_ui_node(&a.ui, a.l);
        int                  out;
        CHECK(written_in(panel, n, &out) == n->w * n->h && out == 0 && panel_twice(panel) == 0 && differ(panel, v) == 0);
        CHECK(panel->windows == (n->h + 3) / 4);
    }
    /* the label in the frame changed: through the buffer, the frame not drawn */
    panel_count_from_now(panel);
    dgx_ui_text_set(&a.ui, a.in, "inside"), dgx_ui_text_set(&b.ui, b.in, "inside");
    dgx_ui_paint(&a.ui, &pa, 2), dgx_ui_paint(&b.ui, &pv, 2);
    {
        const dgx_ui_node_t *n = dgx_ui_node(&a.ui, a.in);
        int                  out;
        CHECK(written_in(panel, n, &out) == n->w * n->h && out == 0 && differ(panel, v) == 0);
    }
    /* a field focused: its caret, once */
    panel_count_from_now(panel);
    dgx_ui_focus(&a.ui, -1);
    dgx_ui_add(&a.ui, 0, &a.field), dgx_ui_add(&b.ui, 0, &b.field);
    const int fa = a.ui.count - 1, fb = b.ui.count - 1;
    dgx_ui_paint(&a.ui, &pa, 3), dgx_ui_paint(&b.ui, &pv, 3);
    dgx_ui_focus(&a.ui, fa), dgx_ui_focus(&b.ui, fb);
    panel_count_from_now(panel);
    dgx_ui_paint(&a.ui, &pa, 4), dgx_ui_paint(&b.ui, &pv, 4);
    CHECK(panel_twice(panel) == 0 && differ(panel, v) == 0);
    {
        const dgx_ui_node_t *n = dgx_ui_node(&a.ui, fa);
        int                  caret = 0;
        for (int y = n->y; y < n->y + n->h; ++y) caret += panel->mem[y][n->x] == colors[3];
        CHECK(caret > 0);
    }

    /* the lines with letters appearing, through a buffer of one row of the panel: pass after pass as the virtual screen
       draws them, every pixel once, time asked while they come */
    pa.buffer_size = 96 * 3;
    dgx_ui_text_appear(&a.ui, a.ln, 20), dgx_ui_text_appear(&b.ui, b.ln, 20);
    {
        int  passes = 0, off = 0, twice = 0;
        bool more_a = true, more_b = true;
        for (uint32_t t = 10; t < 2000 && (more_a || more_b); t += 15, ++passes) {
            panel_count_from_now(panel);
            more_a = dgx_ui_paint(&a.ui, &pa, t);
            more_b = dgx_ui_paint(&b.ui, &pv, t);
            off += differ(panel, v), twice += panel_twice(panel);
            if (more_a != more_b) break;
        }
        CHECK(passes > 3 && off == 0 && twice == 0 && more_a == more_b);
    }

    /* a dropdown's list open: the list through the buffer, its frame straight; every pixel once, as drawn on the
       virtual screen */
    {
        static dgx_ui_dropdown_t da, db;
        dgx_ui_paint(&a.ui, &pa, 3000), dgx_ui_paint(&b.ui, &pv, 3000);
        CHECK(dgx_ui_dropdown_open(&a.ui, a.d, &da, NULL, NULL) && dgx_ui_dropdown_open(&b.ui, b.d, &db, NULL, NULL));
        panel_count_from_now(panel);
        dgx_ui_paint(&a.ui, &pa, 3001), dgx_ui_paint(&b.ui, &pv, 3001);
        CHECK(panel_twice(panel) == 0 && differ(panel, v) == 0);
        dgx_ui_popup_close(&a.ui), dgx_ui_popup_close(&b.ui);
        dgx_ui_paint(&a.ui, &pa, 3002), dgx_ui_paint(&b.ui, &pv, 3002);
        CHECK(differ(panel, v) == 0);
    }

    /* the keyboard's band over the lower half: the label above changed, nothing under the band */
    {
        pa.cover = dgx_ui_rect(0, 30, PANEL_W, PANEL_H - 30), pv.cover = pa.cover;
        dgx_ui_paint(&a.ui, &pa, 4000), dgx_ui_paint(&b.ui, &pv, 4000);
        panel_count_from_now(panel);
        dgx_ui_redraw(&a.ui, 0), dgx_ui_redraw(&b.ui, 0);
        dgx_ui_paint(&a.ui, &pa, 4001), dgx_ui_paint(&b.ui, &pv, 4001);
        int under = 0;
        for (int y = 30; y < PANEL_H; ++y)
            for (int x = 0; x < PANEL_W; ++x) under += panel->writes[y][x] != 0;
        CHECK(under == 0 && panel_twice(panel) == 0);
    }

    /* the look remembers in the last strip of the last part only, and a part after a buffered one is drawn as told.
       The tick at the top of the panel, the keyboard's band over its right half below row 8: its parts the band of
       rows 0..7 across the panel, then the rows 8..15 left of the band, 50 points wide. Through a buffer of 480
       bytes both go in strips (one row, then three); the tick counts once a pass, in the last strip. */
    {
        static ticks_t ta, tb;
        make_ticks(&ta), make_ticks(&tb);
        dgx_ui_paint_free(&pa);
        pa = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = 480, .strips = &dgx_ui_strips};
        pv = (dgx_ui_paint_t){.scr = v, .colors = colors, .color_count = 4};
        pa.cover = pv.cover = dgx_ui_rect(50, 8, PANEL_W - 50, PANEL_H - 8);
        dgx_ui_paint(&ta.ui, &pa, 5000), dgx_ui_paint(&tb.ui, &pv, 5000);
        CHECK(dgx_ui_node(&ta.ui, 1)->w == PANEL_W && dgx_ui_node(&ta.ui, 1)->y == 0);
        CHECK(ta.count == 1 && tb.count == 1 && differ(panel, v) == 0);
        CHECK(ticked(panel, v, &ta, &pa, &tb, &pv, 5001));
        /* a popup over the band of rows 0..7 right of point 20, a buffer of 120 bytes: the part left of the popup, 20
           points wide, goes in strips of two rows; the last, 50 points wide, straight, remembers, and is drawn as
           told, not whole as the strips were */
        CHECK(dgx_ui_popup_open(&ta.ui, &ta.pop, 20, 0, PANEL_W - 20, 8, 0) && dgx_ui_popup_open(&tb.ui, &tb.pop, 20, 0, PANEL_W - 20, 8, 0));
        pa.buffer_size = 120;
        dgx_ui_paint(&ta.ui, &pa, 5010), dgx_ui_paint(&tb.ui, &pv, 5010);
        CHECK(ticked(panel, v, &ta, &pa, &tb, &pv, 5011) && !ta.whole_last && !tb.whole_last);
        dgx_ui_paint_free(&pa);
        dgx_ui_free(&ta.ui), dgx_ui_free(&tb.ui), dgx_ui_free(&ta.pop), dgx_ui_free(&tb.pop);
    }

    /* a list that scrolls, its window moved by a key: through the strips, every pixel once, as the virtual screen */
    {
        static char                  ba[60], bb[60];
        static dgx_ui_textbox_t      ta, tb;
        static dgx_ui_widget_t       wa, wb;
        static dgx_ui_node_t         na[2], nb[2];
        static dgx_ui_t              ua, ub;
        static const dgx_ui_widget_t col = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2};
        dgx_ui_textbox_init(&ta, ba, sizeof ba, 1, &style), dgx_ui_textbox_init(&tb, bb, sizeof bb, 1, &style);
        const int line = dgx_ui_variant_line(&variant[0]);
        wa = (dgx_ui_widget_t){.kind = DGX_UI_LIST, .value = new_place(), .text = &ta, .max = 5, .want_h = (int16_t)(2 * line), .look = &dgx_ui_look_list};
        wb = wa, wb.text = &tb, wb.value = new_place(); /* a copy shares the place otherwise */
        dgx_ui_init(&ua, na, 2, 1), dgx_ui_init(&ub, nb, 2, 1);
        dgx_ui_add(&ua, -1, &col), dgx_ui_add(&ub, -1, &col);
        dgx_ui_add(&ua, 0, &wa), dgx_ui_add(&ub, 0, &wb);
        dgx_ui_text_set(&ua, 1, "one\ntwo\nthree\nfour\nfive\nsix"), dgx_ui_text_set(&ub, 1, "one\ntwo\nthree\nfour\nfive\nsix");
        pa = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer, .strips = &dgx_ui_strips};
        pv = (dgx_ui_paint_t){.scr = v, .colors = colors, .color_count = 4};
        dgx_ui_paint(&ua, &pa, 7000), dgx_ui_paint(&ub, &pv, 7000);
        dgx_ui_focus(&ua, 1), dgx_ui_focus(&ub, 1);
        dgx_ui_key(&ua, DGX_UI_KEY_NEXT), dgx_ui_key(&ub, DGX_UI_KEY_NEXT); /* the second row: the window stands */
        dgx_ui_paint(&ua, &pa, 7001), dgx_ui_paint(&ub, &pv, 7001);
        panel_count_from_now(panel);
        dgx_ui_key(&ua, DGX_UI_KEY_NEXT), dgx_ui_key(&ub, DGX_UI_KEY_NEXT); /* the third: the window a row down */
        dgx_ui_paint(&ua, &pa, 7002), dgx_ui_paint(&ub, &pv, 7002);
        const dgx_ui_node_t *n = dgx_ui_node(&ua, 1);
        CHECK(ta.top == 1 && panel_twice(panel) == 0 && panel_written(panel) == n->w * n->h && differ(panel, v) == 0);
        dgx_ui_paint_free(&pa), dgx_ui_paint_free(&pv);
        dgx_ui_free(&ua), dgx_ui_free(&ub);
    }

    /* the paint given a display of the other depth (three bytes a point both): the strip made again, the label drawn */
    {
        pa = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer, .strips = &dgx_ui_strips};
        dgx_ui_redraw(&a.ui, a.l);
        dgx_ui_paint(&a.ui, &pa, 6000);
        CHECK(pa.strip && pa.strip->color_bits == PANEL_BITS);
        panel->sbus.scr.color_bits = PANEL_BITS == 24 ? 18 : 24;
        panel_count_from_now(panel);
        dgx_ui_redraw(&a.ui, a.l);
        dgx_ui_paint(&a.ui, &pa, 6001);
        const dgx_ui_node_t *n = dgx_ui_node(&a.ui, a.l);
        int                  out;
        CHECK(written_in(panel, n, &out) == n->w * n->h && out == 0 && pa.strip->color_bits == panel->sbus.scr.color_bits);
        panel->sbus.scr.color_bits = PANEL_BITS;
    }

    dgx_ui_paint_free(&pa);
    CHECK(pa.strip == NULL);
    dgx_ui_free(&a.ui), dgx_ui_free(&b.ui);
    v->destroy(&v);
    free(panel);
    CHECK_DONE();
}
