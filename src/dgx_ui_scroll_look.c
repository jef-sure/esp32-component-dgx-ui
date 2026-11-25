/*
 * dgx_ui_scroll_look.c
 *
 *  The look of a scrolling box (dgx_ui_look_scroll): its ground around the children and its bar. A change of its
 *  offset moves the picture by the difference with dgx_scroll_no_fill() where the screen can and draws the rows that
 *  came in, else it is drawn whole. A panel that moves by its own scroll holds one area at a time: the area is kept
 *  here, behind the paint's pointer, and let go when its box goes or another needs it. A program that names it not
 *  carries none of it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <stdlib.h>

#include "dgx_draw.h"
#include "dgx_ui_paint_priv.h"
#include "dgx_ui_scroll_priv.h"


/* the area of a panel a box holds: moved by the panel's scroll, let go by a move as large as it */
typedef struct {
    dgx_ui_t              *tree;
    const dgx_ui_widget_t *widget;
    int16_t                index;
    bool                   wanted; /* another box needed the area: let go before the next pass */
    dgx_ui_rect_t          rect;   /* of the tree's screen */
    int16_t                ox, oy; /* the paint's then */
} held_t;

/* the area let go: a move as large as it under a clip of it */
static void let_go(dgx_ui_paint_t *paint, held_t *h)
{
    int cx, cy, cw, ch;
    dgx_screen_clip_get(paint->scr, &cx, &cy, &cw, &ch);
    const dgx_ui_rect_t r = h->rect;
    dgx_screen_clip(paint->scr, r.x - h->ox, r.y - h->oy, r.w, r.h);
    dgx_scroll_no_fill(paint->scr, r.x - h->ox, r.y - h->oy, r.w, r.h, 0, r.h);
    dgx_screen_clip(paint->scr, cx, cy, cw, ch);
    h->tree = NULL, h->wanted = false;
}

/* before a pass: the area held let go when its box is no longer seen as it was, or another needs it */
static void before(dgx_ui_paint_t *paint, dgx_ui_t *ui)
{
    held_t *h = paint->scroll_held;
    if (!h || !h->tree) return;
    bool gone = h->wanted, found = false;
    for (dgx_ui_t *t = ui; t; t = t->popup) found |= t == h->tree;
    if (!found || dgx_ui_widget(h->tree, h->index) != h->widget || dgx_ui_hidden(h->tree, h->index)) gone = true;
    if (!gone) {
        dgx_ui_scroll_box_t b;
        dgx_ui_rect_t   in;
        dgx_ui_scroll_measure(h->tree, h->index, &b);
        if (!dgx_ui_rect_intersect(&b.rows, &h->rect, &in) || in.x != h->rect.x || in.y != h->rect.y || in.w != h->rect.w || in.h != h->rect.h) gone = true;
        for (const dgx_ui_t *t = h->tree->popup; t && !gone; t = t->popup) gone = dgx_ui_rect_intersect(&t->root_rect, &h->rect, &in);
    }
    if (!gone) return;
    const dgx_ui_rect_t r = h->rect;
    let_go(paint, h);
    dgx_ui_compose_left(ui, paint, r);
}

static void free_held(dgx_ui_paint_t *paint)
{
    held_t *h = paint->scroll_held;
    if (h && h->tree) let_go(paint, h);
    free(h);
    paint->scroll_held = NULL;
}

static const dgx_ui_scroller_t scroller = {.before = before, .free = free_held};

/* the ground of the box over a rectangle: its fill, or what lies under it */
static void ground(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_rect_t r)
{
    if (r.w <= 0 || r.h <= 0) return;
    if (look->fill != DGX_UI_PAPER) dgx_fill_rectangle(paint->scr, r.x - paint->ox, r.y - paint->oy, r.w, r.h, dgx_ui_color(paint, look->fill));
    else dgx_ui_lay_paper(paint, DGX_UI_PAPER, r.x, r.y, r.w, r.h);
}

/* the ground of an area of the children's column less the children in it: strips between them, beside them */
static void ground_around(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, dgx_ui_rect_t a)
{
    int at = a.y;
    for (int c = dgx_ui_at(ui, i)->child; c >= 0; c = dgx_ui_at(ui, c)->next) {
        const dgx_ui_node_t *k = dgx_ui_at(ui, c);
        dgx_ui_rect_t        r;
        if ((k->state.bits & DGX_UI_HIDDEN) || !dgx_ui_look_of(k->widget)) continue; /* covers nothing */
        if (!dgx_ui_rect_intersect(&(dgx_ui_rect_t){k->x, k->y, k->w, k->h}, &a, &r)) continue;
        if (r.y < at) r.h -= at - r.y, r.y = at;
        if (r.h <= 0) continue;
        ground(look, paint, dgx_ui_rect(a.x, at, a.w, r.y - at));
        ground(look, paint, dgx_ui_rect(a.x, r.y, r.x - a.x, r.h));
        ground(look, paint, dgx_ui_rect(r.x + r.w, r.y, a.x + a.w - r.x - r.w, r.h));
        at = r.y + r.h;
    }
    ground(look, paint, dgx_ui_rect(a.x, at, a.w, a.y + a.h - at));
}

/* the bar: the arrows' cells row by row, the track above and below the knob, the knob; every pixel once */
static void draw_bar(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, const dgx_ui_scroll_box_t *b)
{
    if (!b->bar) return;
    const dgx_ui_scroll_look_t *p = look->params ? look->params : &dgx_ui_scroll_default;
    const dgx_ui_node_t        *n = dgx_ui_at(ui, i);
    int                         ah, ky, kh;
    dgx_ui_scroll_bar(p, b, &ah, &ky, &kh);
    const bool     focused = (dgx_ui_bits_shown(ui, n) & DGX_UI_FOCUSED) && p->focused != DGX_UI_PAPER;
    const uint32_t track = dgx_ui_color(paint, p->track), knob = dgx_ui_color(paint, focused ? p->focused : p->knob);
    const int      x = b->bar_x - paint->ox, y = b->rows.y - paint->oy, w = b->bar;
    for (int r = 0; r < ah; ++r) { /* the arrow's row r from its point: as wide as the bar at its base */
        const int      span = (w * (r + 1) + ah - 1) / ah, side = (w - span) / 2;
        const uint32_t ink  = dgx_ui_color(paint, p->arrows);
        for (int end = 0; end < 2; ++end) {
            const int row = end ? y + b->rows.h - 1 - r : y + r;
            dgx_fill_rectangle(paint->scr, x, row, side, 1, track);
            dgx_fill_rectangle(paint->scr, x + side, row, span, 1, ink);
            dgx_fill_rectangle(paint->scr, x + side + span, row, w - side - span, 1, track);
        }
    }
    dgx_fill_rectangle(paint->scr, x, y + ah, w, ky - ah, track);
    dgx_fill_rectangle(paint->scr, x, y + ky, w, kh, knob);
    dgx_fill_rectangle(paint->scr, x, y + ky + kh, w, b->rows.h - ah - ky - kh, track);
}

/* the picture moved by d rows up, the rows that came in drawn: false when it cannot be, the box drawn whole then. Not
   under a composing, nor a clip of nothing (a teller's pass, dgx_ui_paint_changes()), nor over a drawn background the box
   does not fill, nor by a whole view; on a panel that moves by its own scroll, not while another box holds its area */
static bool move(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, const dgx_ui_scroll_box_t *b, int d)
{
    const int ad = d < 0 ? -d : d;
    if (paint->composing || paint->parts != 1 || ad >= b->rows.h || (look->fill == DGX_UI_PAPER && paint->on_background)) return false;
    const dgx_ui_node_t *n = dgx_ui_at(ui, i);
    if (n->parent >= 0 && !(dgx_ui_at(ui, n->parent)->widget->flags & (DGX_UI_COLUMN | DGX_UI_ROW))) /* a box of neither: one over it moves too */
        for (int c = dgx_ui_at(ui, n->parent)->child; c >= 0; c = dgx_ui_at(ui, c)->next) {
            const dgx_ui_node_t *k = dgx_ui_at(ui, c);
            dgx_ui_rect_t        over;
            if (c != i && !(k->state.bits & DGX_UI_HIDDEN) && dgx_ui_rect_intersect(&(dgx_ui_rect_t){k->x, k->y, k->w, k->h}, &b->rows, &over)) return false;
        }
    dgx_screen_t *scr = paint->scr;
    int           cx, cy, cw, ch;
    dgx_screen_clip_get(scr, &cx, &cy, &cw, &ch);
    dgx_ui_rect_t r;
    if (!dgx_ui_rect_intersect(&b->rows, &(dgx_ui_rect_t){cx + paint->ox, cy + paint->oy, cw, ch}, &r) || r.h <= ad) return false;
    const bool panel = scr->screen_subtype == DgxPhysicalScreenWithBus;
    held_t    *h     = paint->scroll_held;
    if (panel) {
        if (h && h->tree && (h->tree != ui || h->index != i || h->widget != n->widget || h->rect.x != r.x || h->rect.y != r.y || h->rect.w != r.w ||
                             h->rect.h != r.h)) {
            h->wanted = true; /* let go before the next pass; drawn whole now */
            return false;
        }
        if (!h && !(h = paint->scroll_held = calloc(1, sizeof *h))) return false; /* no area held that could not be let go */
    }
    dgx_screen_clip(scr, r.x - paint->ox, r.y - paint->oy, r.w, r.h);
    const bool done = dgx_scroll_no_fill(scr, r.x - paint->ox, r.y - paint->oy, r.w, r.h, 0, -d) == ESP_OK;
    dgx_screen_clip(scr, cx, cy, cw, ch);
    if (!done) return false;
    if (panel) *h = (held_t){.tree = ui, .widget = n->widget, .index = (int16_t)i, .rect = r, .ox = paint->ox, .oy = paint->oy};
    /* the rows that came in: at the bottom when the children went up; the ground, the children, the bar after */
    const dgx_ui_rect_t strip = d > 0 ? dgx_ui_rect(r.x, r.y + r.h - ad, r.w, ad) : dgx_ui_rect(r.x, r.y, r.w, ad);
    dgx_ui_rect_t       part;
    dgx_screen_clip(scr, strip.x - paint->ox, strip.y - paint->oy, strip.w, strip.h);
    if (dgx_ui_rect_intersect(&strip, &b->column, &part)) {
        ground_around(look, paint, ui, i, part);
        const bool composing = paint->composing;
        paint->composing     = true; /* a child drawn in part: nothing of it remembered */
        dgx_ui_compose_in(paint, ui, i, part);
        paint->composing = composing;
    }
    const int right = b->bar ? b->bar_x + b->bar : b->column.x + b->column.w; /* the pad beside the bar */
    ground(look, paint, dgx_ui_rect(right, strip.y, strip.x + strip.w - right, strip.h));
    dgx_screen_clip(scr, cx, cy, cw, ch);
    return true;
}

bool dgx_ui_draw_scroll(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    (void)now_ms;
    paint->scroller = &scroller;
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    dgx_ui_scroll_box_t                b;
    dgx_ui_scroll_measure(ui, i, &b);
    /* an offset the program wrote into the place since the layout: the children laid at it first */
    if (n->widget->value && *n->widget->value != b.offset) dgx_ui_relayout(ui), dgx_ui_layout(ui), dgx_ui_scroll_measure(ui, i, &b);
    const int d = (int)(dgx_ui_value_of(n) - n->state_drawn.value);
    if (!paint->whole && d && !move(look, paint, ui, i, &b, d)) paint->whole = true; /* its children too */
    if (paint->whole) {
        /* the children's column around them, the pad beside the bar */
        ground_around(look, paint, ui, i, b.column);
        const int right = b.bar ? b.bar_x + b.bar : b.column.x + b.column.w;
        ground(look, paint, dgx_ui_rect(right, b.rows.y, n->x + n->w - right, b.rows.h));
    }
    draw_bar(look, paint, ui, i, &b);
    return false;
}

const dgx_ui_look_t dgx_ui_look_scroll = {.draw = dgx_ui_draw_scroll, .params = &dgx_ui_scroll_default, .fill = DGX_UI_PAPER, .behavior = &dgx_ui_scroll_behavior};
