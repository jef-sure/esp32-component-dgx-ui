/*
 * dgx_ui_paint_changes.c
 *
 *  What changed, told and not drawn: the pass dgx_ui_paint() makes, over the same marks, with a teller lent to the
 *  paint in place of its strips. The painter offers every part of a widget it would draw to the strips first; the
 *  teller tells the part's rectangle to the program's function and calls the look under an empty clip, so that it
 *  writes nothing and keeps what it keeps (its time, its canvas, its fitted variant). The places a band or popups left
 *  are told too, and not composed. The tree is then a layer in another compositor's strips, which composes each
 *  rectangle told by dgx_ui_paint_area_to(). A program that does not call it carries none of it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <stdint.h>

#include "dgx_ui_paint_priv.h"

/* lent to the paint for the call: the strips' hook first, so the paint's pointer to its strips is the teller */
typedef struct {
    dgx_ui_strips_t   hook;
    dgx_ui_paint_changes_t tell;
    void             *ctx;
    dgx_ui_rect_t     last; /* the rectangle told last: one inside it is composed with it */
} teller_t;

static bool inside(const dgx_ui_rect_t *inner, const dgx_ui_rect_t *outer)
{
    return !dgx_ui_rect_empty(outer) && inner->x >= outer->x && inner->y >= outer->y && inner->x + inner->w <= outer->x + outer->w &&
           inner->y + inner->h <= outer->y + outer->h;
}

/* told unless nobody is to be, or it is empty or lies inside the last told: the pass goes parents first, so a widget
   drawn whole is told once and its children not. The function may compose the rectangle at once: what that composing
   changes in the paint in the middle of the pass is given back after it (a composing names no moment) */
static void tell_once(teller_t *teller, dgx_ui_paint_t *paint, dgx_ui_rect_t rect)
{
    if (!teller->tell || dgx_ui_rect_empty(&rect) || inside(&rect, &teller->last)) return;
    teller->last              = rect;
    const dgx_ui_paint_t kept = *paint;
    teller->tell(teller->ctx, rect);
    paint->paper = kept.paper, paint->on_background = kept.on_background, paint->whole = kept.whole;
    paint->part = kept.part, paint->parts = kept.parts, paint->popup_over = kept.popup_over;
    paint->wake_ms = kept.wake_ms, paint->wake_named = kept.wake_named, paint->buffer_used = kept.buffer_used;
}

/* a part of a widget the pass would draw: told, then its look called under an empty clip; taken, never drawn. A
   part of a composing made meanwhile, by the function told, declined: drawn on that composing's own screen */
static bool tell_part(dgx_ui_paint_t *paint, const dgx_ui_look_t *look, dgx_ui_t *ui, int i, uint32_t now_ms, dgx_ui_rect_t part, bool *more)
{
    if (paint->composing) return false;
    tell_once((teller_t *)(uintptr_t)paint->strips, paint, part);
    dgx_screen_clip(paint->scr, 0, 0, 0, 0);
    *more = look->draw(look, paint, ui, i, now_ms);
    return true;
}

bool dgx_ui_paint_changes(dgx_ui_t *ui, dgx_ui_paint_t *paint, uint32_t now_ms, dgx_ui_paint_changes_t tell, void *ctx)
{
    teller_t       teller  = {.hook = {.draw = tell_part}, .tell = tell, .ctx = ctx};
    const uint32_t last_ms = paint->now_ms;
    paint->now_ms          = now_ms; /* the time of a composing the function makes of what it is told */
    /* the band gone or moved, popups closed: their places told and cleared, so that the pass composes none */
    const dgx_ui_rect_t band_was = paint->cover_was, band = paint->cover;
    if (band_was.x != band.x || band_was.y != band.y || band_was.w != band.w || band_was.h != band.h) {
        paint->cover_was = band;
        tell_once(&teller, paint, band_was);
    }
    for (dgx_ui_t *tree = ui; tree; tree = tree->popup) tell_once(&teller, paint, tree->popup_was), tree->popup_was = dgx_ui_rect(0, 0, 0, 0);
    const dgx_ui_strips_t *strips = paint->strips;
    paint->strips                 = &teller.hook;
    paint->now_ms                 = last_ms; /* the pass sees whether it is another tree of the same turn */
    const bool more               = dgx_ui_paint(ui, paint, now_ms);
    paint->strips                 = strips;
    return more;
}
