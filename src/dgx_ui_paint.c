/*
 * dgx_ui_paint.c
 *
 *  The painter: the tree walked from the root, parents before children, each widget that changed drawn by its look
 *  under a clip of each part of its box the covers leave (the keyboard's band, every popup over its tree), then each
 *  popup the same way, one over another; the screen's own clip given back at the end, all in one batch.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <stdlib.h>

#include "dgx_draw.h"
#include "dgx_ui_paint_priv.h"
#include "dgx_ui_text.h"
#include "esp_log.h"

static const char *TAG = "dgx_ui";

const dgx_ui_look_t *dgx_ui_look_of(const dgx_ui_widget_t *w)
{
    if (w->look) return w->look;
    switch (w->kind) {
    case DGX_UI_BOX: return &dgx_ui_look_box;
    case DGX_UI_FRAME: return &dgx_ui_look_frame;
    case DGX_UI_LABEL: return &dgx_ui_look_text;
    default: return NULL;
    }
}

/* the look of a shown widget; none: told once a tree, as it is never drawn */
static const dgx_ui_look_t *look_told(dgx_ui_t *ui, int i)
{
    const dgx_ui_widget_t *w    = dgx_ui_at(ui, i)->widget;
    const dgx_ui_look_t   *look = dgx_ui_look_of(w);
    if (!look && !ui->told_look) {
        ESP_LOGW(TAG, "widget %d of kind %d has no look, not drawn: give it .look = &dgx_ui_look_...", i, w->kind);
        ui->told_look = true;
    }
    return look;
}

/* filled around its children by its look: a box or a frame of a column or a row, in its own colours too; not when a
   child drawn by no look holds children of its own, which lie on the paper filled for it and are drawn again */
static bool fills_around(const dgx_ui_t *ui, const dgx_ui_node_t *n, const dgx_ui_look_t *look)
{
    if (!look || !(n->widget->flags & (DGX_UI_COLUMN | DGX_UI_ROW)) || (look->draw != dgx_ui_draw_box && look->draw != dgx_ui_draw_frame)) return false;
    for (int c = n->child; c >= 0; c = dgx_ui_at(ui, c)->next) {
        const dgx_ui_node_t *k = dgx_ui_at(ui, c);
        if (!(k->state.bits & DGX_UI_HIDDEN) && k->child >= 0 && !dgx_ui_look_of(k->widget)) return false;
    }
    return true;
}

/* the bits it shows (dgx_ui_bits_shown()), or what it shows of its place (dgx_ui_shown()), are not what was drawn */
static bool differs(const dgx_ui_t *ui, const dgx_ui_node_t *n)
{
    return dgx_ui_bits_shown(ui, n) != n->state_drawn.bits || dgx_ui_shown(n) != n->state_drawn.value;
}

/* drawn as it is now: the bits it shows, and what it shows of its place */
static void drawn_now(const dgx_ui_t *ui, dgx_ui_node_t *n)
{
    n->state_drawn.bits = dgx_ui_bits_shown(ui, n), n->state_drawn.value = dgx_ui_shown(n);
}

/* a rectangle of the tree's screen composed (below); left_to_walk: a place a cover left, composed by the painter before
   its walk */
static void compose(dgx_ui_t *ui, dgx_ui_paint_t *paint, dgx_ui_rect_t area, bool background, bool left_to_walk);

/* a rectangle cut by the covers: its parts counted, or drawn by a look, or laid with the paper, under a clip of each */
typedef struct {
    dgx_ui_paint_t      *paint;
    dgx_ui_t            *ui;
    int                  i, count;
    const dgx_ui_look_t *look; /* NULL: the paint's paper laid */
    uint32_t             now_ms;
    bool                 draw, more;
} cut_t;

/* the parts of r outside a cover and the covers after it: the rectangle of the popup `over`, of the one over that,
   and so on. A cover takes at most four parts of each part: no array of them, one level of the calls a cover */
static void cut(cut_t *c, dgx_ui_rect_t r, const dgx_ui_rect_t *cover, const dgx_ui_t *over)
{
    if (cover) {
        dgx_ui_rect_t parts[4];
        const int     n = dgx_ui_rect_subtract(&r, cover, parts);
        for (int k = 0; k < n; ++k) cut(c, parts[k], over ? &over->root_rect : NULL, over ? over->popup : NULL);
        return;
    }
    if (c->draw) {
        dgx_ui_paint_t *p = c->paint;
        dgx_screen_clip(p->scr, r.x - p->ox, r.y - p->oy, r.w, r.h);
        if (!c->look) dgx_ui_lay_paper(p, DGX_UI_PAPER, r.x, r.y, r.w, r.h);
        else {
            p->part = (uint8_t)(c->count < p->parts ? c->count : p->parts - 1);
            /* through the buffer when the program named the strips and they take this part; else straight. A look
               that asked for time and named no moment: the next pass */
            bool more     = false;
            p->wake_named = false;
            if (!p->strips || !p->strips->draw(p, c->look, c->ui, c->i, c->now_ms, r, &more)) more = c->look->draw(c->look, p, c->ui, c->i, c->now_ms);
            if (more && !p->wake_named) p->wake_ms = 0;
            c->more |= more;
        }
    }
    ++c->count;
}

/* the look called for each part of the box the keyboard's band and the popups over the tree leave, the clip set to
   it: the parts counted first, so a look knows its last */
static bool draw_parts(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, const dgx_ui_look_t *look, const dgx_ui_rect_t *within, uint32_t now_ms)
{
    const dgx_ui_node_t *w = dgx_ui_at(ui, i);
    dgx_ui_rect_t          box = dgx_ui_rect(w->x, w->y, w->w, w->h);
    if (within && !dgx_ui_rect_intersect(&box, within, &box)) return paint->parts = 0, false;
    cut_t c = {.paint = paint, .ui = ui, .i = i, .look = look, .now_ms = now_ms};
    cut(&c, box, &paint->cover, paint->popup_over);
    paint->parts = (uint8_t)(c.count < 255 ? c.count : 255);
    if (!c.count) return false;
    c.draw = true, c.count = 0;
    cut(&c, box, &paint->cover, paint->popup_over);
    return c.more;
}

/* the rectangle that holds two, an empty one none */
static dgx_ui_rect_t hull(dgx_ui_rect_t a, dgx_ui_rect_t b)
{
    if (dgx_ui_rect_empty(&a)) return b;
    if (dgx_ui_rect_empty(&b)) return a;
    const int x0 = a.x < b.x ? a.x : b.x, y0 = a.y < b.y ? a.y : b.y;
    const int x1 = a.x + a.w > b.x + b.w ? a.x + a.w : b.x + b.w, y1 = a.y + a.h > b.y + b.h ? a.y + a.h : b.y + b.h;
    return dgx_ui_rect(x0, y0, x1 - x0, y1 - y0);
}

/*
 * A widget drawn when it is to be, then its children: all when it was drawn whole, else those marked or with a mark
 * below them; a widget and all under it skipped when nothing is. Returns whether it, or one under it, is still to be
 * drawn: under a cover, or asking for time; the widgets above it keep DGX_UI_MARK_BELOW then. *time: time asked.
 * paper, on_bg: what lies under it, the drawn background when no box above it fills.
 */
static bool walk(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint8_t paper, bool on_bg, bool whole, uint32_t now_ms, bool *time, const dgx_ui_rect_t *clip)
{
    dgx_ui_node_t *w = dgx_ui_at(ui, i);
    if (w->state.bits & DGX_UI_HIDDEN) return false; /* drawn whole when shown: what is under it waits */
    whole |= (w->marks & DGX_UI_MARK_WHOLE) != 0;
    const bool itself = whole || (w->marks & DGX_UI_MARKS_TO_DRAW) || differs(ui, w);
    if (!itself && !(w->marks & DGX_UI_MARK_BELOW)) return false; /* nothing here, nor under it */
    /* what of it may be seen: its box within its parent's, and so on up; none, nothing of it or under it is drawn */
    dgx_ui_rect_t seen;
    if (!dgx_ui_rect_intersect(&(dgx_ui_rect_t){w->x, w->y, w->w, w->h}, clip, &seen)) {
        w->marks &= (uint8_t)~(DGX_UI_MARKS_TO_DRAW | DGX_UI_MARK_BELOW);
        drawn_now(ui, w);
        return false;
    }
    const dgx_ui_look_t *look = look_told(ui, i);
    /* a child moved, came or went: filled around the children, who are drawn when marked themselves; a look that
       cannot do so draws it whole, its children too */
    bool around = !whole && (w->marks & DGX_UI_MARK_AROUND);
    if (around && !fills_around(ui, w, look)) whole = true, around = false;
    bool more = false, unseen = false;
    if (look && itself) {
        paint->paper = paper, paint->on_background = on_bg, paint->whole = whole || around;
        more         = draw_parts(paint, ui, i, look, &seen, now_ms);
        if (!around) whole = paint->whole; /* a look may find it has to be drawn whole: its children too then */
        unseen = paint->parts == 0;
        /* its text drawn and no more time asked: the look showed all it will show of it (dgx_ui_text_state()) */
        if (!unseen && !more && w->widget->text) w->widget->text->flags |= DGX_UI_TEXTBOX_SEEN;
    }
    if (!unseen) {
        w->marks &= (uint8_t)~DGX_UI_MARKS_TO_DRAW;
        if (more) w->marks |= DGX_UI_MARK_TIME; /* the look is called again, and it alone knows whether time changed it */
        drawn_now(ui, w);
    }
    /* else wholly under a cover: no part drawn, so its marks and its state as drawn stay, for the pass that sees it */
    *time |= more;
    if (whole || (w->marks & DGX_UI_MARK_BELOW)) {
        const bool    fills = look && look->fill >= 0;
        const uint8_t in    = fills ? (uint8_t)look->fill : paper;
        bool          below = false;
        for (int c = w->child; c >= 0; c = dgx_ui_at(ui, c)->next) below |= walk(paint, ui, c, in, on_bg && !fills, whole, now_ms, time, &seen);
        w->marks = (uint8_t)(below ? w->marks | DGX_UI_MARK_BELOW : w->marks & ~DGX_UI_MARK_BELOW);
    }
    return (w->marks & (DGX_UI_MARKS_TO_DRAW | DGX_UI_MARK_BELOW)) || differs(ui, w);
}

bool dgx_ui_paint(dgx_ui_t *ui, dgx_ui_paint_t *paint, uint32_t now_ms)
{
    dgx_screen_t *scr       = paint->scr;
    const bool    same_turn = paint->painted && now_ms == paint->now_ms; /* another tree of the turn: the least moment of both */
    paint->now_ms           = now_ms; /* the time of the composings below too: a look that counts by it is drawn as it stands */
    /* the orientation, each direction of DGX's -1, 0 or +1 counted from 0: the eight turns told apart */
    const int     turn = (int)((unsigned)(scr->dir_x + 1) | (unsigned)(scr->dir_y + 1) << 2 | (unsigned)scr->swap_xy << 4);
    for (dgx_ui_t *t = ui; t; t = t->popup) {
        /* every tree, the popups' too, laid for the screen as it stands */
        dgx_ui_screen(t, scr->width, scr->height, turn);
        dgx_ui_layout(t);
        dgx_ui_places(t); /* what the program wrote into the places since the last pass */
    }
    /* the keyboard's band gone or moved: the place it leaves composed again, the new band left alone */
    const dgx_ui_rect_t was = paint->cover_was, now = paint->cover;
    dgx_ui_rect_t       band_left = dgx_ui_rect(0, 0, 0, 0);
    if (was.x != now.x || was.y != now.y || was.w != now.w || was.h != now.h) {
        paint->cover_was = now;
        if (!dgx_ui_rect_empty(&was)) compose(ui, paint, was, false, true), band_left = was;
    }
    /* popups closed at any level: where they lay composed again from every tree still open, in one rectangle, less the
       band's place composed just now: every pixel once. What of that place the new band covers is covered here too */
    dgx_ui_rect_t closed = dgx_ui_rect(0, 0, 0, 0);
    for (dgx_ui_t *t = ui; t; t = t->popup) closed = hull(closed, t->popup_was), t->popup_was = dgx_ui_rect(0, 0, 0, 0);
    if (!dgx_ui_rect_empty(&closed)) {
        dgx_ui_rect_t parts[4];
        const int     count = dgx_ui_rect_subtract(&closed, &band_left, parts); /* the place whole with no band left */
        for (int k = 0; k < count; ++k) compose(ui, paint, parts[k], false, true);
    }
    int cx, cy, cw, ch;
    dgx_screen_clip_get(scr, &cx, &cy, &cw, &ch);
    dgx_screen_batch_begin(scr);
    paint->composing = false;
    if (!same_turn) paint->wake_ms = DGX_UI_WAKE_NONE; /* the moments of this walk, and of the trees before it of its turn */
    bool more        = false;
    const dgx_ui_rect_t everywhere = dgx_ui_rect(-16384, -16384, 32767, 32767);
    for (dgx_ui_t *t = ui; t; t = t->popup) {
        /* each tree under the popups over it; a popup is a tree of its own in its rectangle */
        paint->popup_over = t->popup;
        if (dgx_ui_widget(t, 0)) walk(paint, t, 0, paint->background, paint->drawn != NULL, false, now_ms, &more, &everywhere);
    }
    paint->popup_over = NULL;
    paint->painted = true;
    dgx_screen_clip(scr, cx, cy, cw, ch);
    dgx_screen_batch_end(scr);
    if (paint->canvases) paint->canvases->end(paint, ui); /* the canvases of widgets gone freed, NO_MEMORY told */
    return more;
}

void *dgx_ui_buffer_free(const dgx_ui_paint_t *paint, size_t *size)
{
    if (!paint->buffer || paint->buffer_used >= paint->buffer_size) return *size = 0, NULL;
    *size = paint->buffer_size - paint->buffer_used;
    return (uint8_t *)paint->buffer + paint->buffer_used; /* past the painter's strip while it draws one */
}

void dgx_ui_paint_free(dgx_ui_paint_t *paint)
{
    if (paint->rgb565) dgx_screen_destroy(&paint->rgb565);
    if (paint->indexed) dgx_screen_destroy(&paint->indexed);
    if (paint->strip) dgx_screen_destroy(&paint->strip);
    if (paint->canvases) paint->canvases->free(paint);
    free(paint->lut);
    paint->lut = NULL;
}

/* every widget under i that meets the area drawn whole under a clip of its part of it, nothing marked; for the
   background only, nothing within a widget whose look fills: none of the background shows there. left_to_walk, the
   painter's own composing of a place a cover left before its walk: a widget the walk will draw is left to it, marked
   whole so that the walk draws all of it, and every pixel of the place is written once */
static void walk_area(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint8_t paper, bool on_bg, bool background, const dgx_ui_rect_t *area,
                      bool left_to_walk)
{
    dgx_ui_node_t *w = dgx_ui_at(ui, i);
    dgx_ui_rect_t  seen; /* its box within the area and its parent's: its children lie within it, or are cut */
    if ((w->state.bits & DGX_UI_HIDDEN) || !dgx_ui_rect_intersect(&(dgx_ui_rect_t){w->x, w->y, w->w, w->h}, area, &seen)) return;
    if (left_to_walk && ((w->marks & (DGX_UI_MARKS_TO_DRAW | DGX_UI_MARK_WHOLE)) || differs(ui, w))) {
        w->marks |= DGX_UI_MARK_WHOLE;
        return;
    }
    const dgx_ui_look_t *look  = look_told(ui, i);
    const bool           fills = look && look->fill >= 0;
    if (background && fills) return;
    if (look) {
        paint->paper = paper, paint->on_background = on_bg, paint->whole = true;
        draw_parts(paint, ui, i, look, &seen, paint->now_ms);
    }
    const uint8_t in = fills ? (uint8_t)look->fill : paper;
    for (int c = w->child; c >= 0; c = dgx_ui_at(ui, c)->next) walk_area(paint, ui, c, in, on_bg && !fills, background, &seen, left_to_walk);
}

void dgx_ui_paint_area(dgx_ui_t *ui, dgx_ui_paint_t *paint, int x, int y, int w, int h)
{
    dgx_ui_compose(ui, paint, dgx_ui_rect(x, y, w, h), false);
}

void dgx_ui_compose(dgx_ui_t *ui, dgx_ui_paint_t *paint, dgx_ui_rect_t area, bool background)
{
    compose(ui, paint, area, background, false);
}

static void compose(dgx_ui_t *ui, dgx_ui_paint_t *paint, dgx_ui_rect_t area, bool background, bool left_to_walk)
{
    dgx_screen_t *scr = paint->scr;
    for (dgx_ui_t *t = ui; t; t = t->popup) dgx_ui_layout(t);
    const dgx_ui_node_t *root  = dgx_ui_widget(ui, 0) ? dgx_ui_node(ui, 0) : NULL;
    const bool           drawn = paint->drawn != NULL;
    int                  cx, cy, cw, ch;
    dgx_screen_clip_get(scr, &cx, &cy, &cw, &ch);
    dgx_screen_batch_begin(scr);
    /* the background where the root does not lie, less the keyboard's band and the popups over it */
    dgx_ui_rect_t       parts[4];
    const dgx_ui_rect_t rbox = root ? dgx_ui_rect(root->x, root->y, root->w, root->h) : dgx_ui_rect(0, 0, 0, 0);
    const int           n    = dgx_ui_rect_subtract(&area, &rbox, parts);
    cut_t               c    = {.paint = paint, .draw = true};
    paint->paper = paint->background, paint->on_background = drawn;
    for (int k = 0; k < n; ++k) cut(&c, parts[k], &paint->cover, ui->popup);
    /* every tree in the area, each under the popups over it */
    paint->composing = true;
    for (dgx_ui_t *t = ui; t; t = t->popup) {
        paint->popup_over = t->popup;
        if (dgx_ui_widget(t, 0)) walk_area(paint, t, 0, paint->background, drawn, background, &area, left_to_walk);
    }
    paint->popup_over = NULL;
    paint->composing  = false;
    dgx_screen_clip(scr, cx, cy, cw, ch);
    dgx_screen_batch_end(scr);
}
