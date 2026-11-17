/*
 * dgx_ui_paint.c
 *
 *  The painter: the tree walked from the root, parents before children, each widget that changed drawn by its look
 *  under a clip of its box (less the keyboard's band), the screen's own clip given back at the end, all in one batch.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_ui_paint.h"

/* the look of a widget: its own, else its kind's */
static const dgx_ui_look_t *look_of(const dgx_ui_widget_t *w)
{
    if (w->look) return w->look;
    switch (w->kind) {
    case DGX_UI_BOX: return &dgx_ui_look_box;
    case DGX_UI_FRAME: return &dgx_ui_look_frame;
    case DGX_UI_LABEL: return &dgx_ui_look_text;
    default: return NULL;
    }
}

static bool differs(const dgx_ui_state_t *a, const dgx_ui_state_t *b)
{
    return a->bits != b->bits || a->value != b->value;
}

/* the look called for each part of the box the keyboard's band leaves, the clip set to it */
static bool draw_parts(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, const dgx_ui_look_t *look, const dgx_ui_rect_t *within, uint32_t now_ms)
{
    const dgx_ui_node_t *w = dgx_ui_node(ui, i);
    dgx_ui_rect_t          box = dgx_ui_rect(w->x, w->y, w->w, w->h);
    if (within && !dgx_ui_rect_intersect(&box, within, &box)) return paint->parts = 0, false;
    /* the box less the keyboard's band, each part less the popup over the tree: at most 16 pieces */
    dgx_ui_rect_t first[4], parts[16];
    const int     m = dgx_ui_rect_subtract(&box, &paint->cover, first);
    int           n = 0;
    for (int j = 0; j < m; ++j) n += dgx_ui_rect_subtract(&first[j], &paint->popup_cover, parts + n);
    bool more = false;
    paint->parts                = (uint8_t)n;
    for (int k = 0; k < n; ++k) {
        paint->part = (uint8_t)k;
        dgx_screen_clip(paint->scr, parts[k].x - paint->ox, parts[k].y - paint->oy, parts[k].w, parts[k].h);
        more |= look->draw(look, paint, ui, i, now_ms);
    }
    return more;
}

static bool walk(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint8_t paper, bool whole, uint32_t now_ms)
{
    dgx_ui_node_t *w = dgx_ui_node(ui, i);
    if (w->state_value.bits & DGX_UI_HIDDEN) return false;
    whole |= (w->marks & DGX_UI_MARK_WHOLE) != 0;
    const dgx_ui_look_t *look = look_of(w->widget);
    bool                 more = false, unseen = false;
    if (look && (whole || (w->marks & (DGX_UI_MARK_CHANGED | DGX_UI_MARK_FIT | DGX_UI_MARK_TIME)) || differs(&w->state_value, &w->state_drawn))) {
        paint->paper = paper, paint->whole = whole;
        more         = draw_parts(paint, ui, i, look, NULL, now_ms);
        whole        = paint->whole; /* a look may find it has to be drawn whole: its children too then */
        unseen       = paint->parts == 0;
    }
    if (!unseen) {
        w->marks &= (uint8_t)~(DGX_UI_MARK_WHOLE | DGX_UI_MARK_CHANGED | DGX_UI_MARK_FIT | DGX_UI_MARK_TIME);
        if (more) w->marks |= DGX_UI_MARK_TIME; /* the look is called again, and it alone knows whether time changed it */
        w->state_drawn = w->state_value;
    }
    /* else wholly under a cover: no part drawn, so its marks and its state as drawn stay, for the pass that sees it */
    const uint8_t in = look && look->fill >= 0 ? (uint8_t)look->fill : paper;
    for (int c = w->child; c >= 0; c = dgx_ui_node(ui, c)->next) more |= walk(paint, ui, c, in, whole, now_ms);
    return more;
}

bool dgx_ui_paint(dgx_ui_t *ui, dgx_ui_paint_t *paint, uint32_t now_ms)
{
    dgx_screen_t *scr = paint->scr;
    dgx_ui_screen(ui, scr->width, scr->height, (int)scr->dir_x | (int)scr->dir_y << 2 | (int)scr->swap_xy << 4);
    dgx_ui_layout(ui);
    /* the keyboard's band gone or moved: the place it leaves composed again, the new band left alone */
    const dgx_ui_rect_t was = paint->cover_was, now = paint->cover;
    if (was.x != now.x || was.y != now.y || was.w != now.w || was.h != now.h) {
        paint->cover_was = now;
        if (!dgx_ui_rect_empty(&was)) dgx_ui_paint_area(ui, paint, was.x, was.y, was.w, was.h);
    }
    /* a popup closed: where it lay composed again from the tree under it */
    if (!dgx_ui_rect_empty(&ui->popup_was)) {
        const dgx_ui_rect_t r = ui->popup_was;
        ui->popup_was         = dgx_ui_rect(0, 0, 0, 0);
        paint->popup_cover    = dgx_ui_rect(0, 0, 0, 0);
        dgx_ui_paint_area(ui, paint, r.x, r.y, r.w, r.h);
    }
    int cx, cy, cw, ch;
    dgx_screen_clip_get(scr, &cx, &cy, &cw, &ch);
    dgx_screen_batch_begin(scr);
    paint->composing   = false;
    paint->popup_cover = ui->popup ? ui->popup->root_rect : dgx_ui_rect(0, 0, 0, 0);
    bool more          = dgx_ui_widget(ui, 0) ? walk(paint, ui, 0, paint->background, false, now_ms) : false;
    if (ui->popup && dgx_ui_widget(ui->popup, 0)) {
        /* the popup over it: a tree of its own in its rectangle, nothing over it but the keyboard's band */
        dgx_ui_t *pop      = ui->popup;
        paint->popup_cover = dgx_ui_rect(0, 0, 0, 0);
        dgx_ui_screen(pop, scr->width, scr->height, (int)scr->dir_x | (int)scr->dir_y << 2 | (int)scr->swap_xy << 4);
        dgx_ui_layout(pop);
        more |= walk(paint, pop, 0, paint->background, false, now_ms);
    }
    paint->popup_cover = dgx_ui_rect(0, 0, 0, 0);
    dgx_screen_clip(scr, cx, cy, cw, ch);
    dgx_screen_batch_end(scr);
    return more;
}

void dgx_ui_paint_free(dgx_ui_paint_t *paint)
{
    if (paint->rgb565) dgx_screen_destroy(&paint->rgb565);
}

/* every widget under i that meets the area drawn whole under a clip of its part of it, nothing marked */
static void walk_area(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint8_t paper, const dgx_ui_rect_t *area)
{
    const dgx_ui_node_t *w = dgx_ui_node(ui, i);
    if (w->state_value.bits & DGX_UI_HIDDEN) return;
    const dgx_ui_look_t *look = look_of(w->widget);
    if (look) {
        paint->paper = paper, paint->whole = true;
        draw_parts(paint, ui, i, look, area, 0);
    }
    const uint8_t in = look && look->fill >= 0 ? (uint8_t)look->fill : paper;
    for (int c = w->child; c >= 0; c = dgx_ui_node(ui, c)->next) walk_area(paint, ui, c, in, area);
}

void dgx_ui_paint_area(dgx_ui_t *ui, dgx_ui_paint_t *paint, int x, int y, int w, int h)
{
    dgx_screen_t *scr = paint->scr;
    dgx_ui_layout(ui);
    const dgx_ui_node_t *root = dgx_ui_widget(ui, 0) ? dgx_ui_node(ui, 0) : NULL;
    const dgx_ui_rect_t  area = dgx_ui_rect(x, y, w, h);
    int                    cx, cy, cw, ch;
    dgx_screen_clip_get(scr, &cx, &cy, &cw, &ch);
    dgx_screen_batch_begin(scr);
    /* the background where the root does not lie */
    dgx_ui_rect_t parts[4];
    const dgx_ui_rect_t rbox = root ? dgx_ui_rect(root->x, root->y, root->w, root->h) : dgx_ui_rect(0, 0, 0, 0);
    const int n = dgx_ui_rect_subtract(&area, &rbox, parts);
    for (int k = 0; k < n; ++k) {
        dgx_screen_clip(scr, parts[k].x - paint->ox, parts[k].y - paint->oy, parts[k].w, parts[k].h);
        dgx_fill_rectangle(scr, parts[k].x - paint->ox, parts[k].y - paint->oy, parts[k].w, parts[k].h, dgx_ui_color(paint, paint->background));
    }
    paint->composing   = true;
    paint->popup_cover = ui->popup ? ui->popup->root_rect : dgx_ui_rect(0, 0, 0, 0);
    if (root) walk_area(paint, ui, 0, paint->background, &area);
    if (ui->popup && dgx_ui_widget(ui->popup, 0)) {
        paint->popup_cover = dgx_ui_rect(0, 0, 0, 0);
        dgx_ui_layout(ui->popup);
        walk_area(paint, ui->popup, 0, paint->background, &area);
    }
    paint->popup_cover = dgx_ui_rect(0, 0, 0, 0);
    paint->composing   = false;
    dgx_screen_clip(scr, cx, cy, cw, ch);
    dgx_screen_batch_end(scr);
}
