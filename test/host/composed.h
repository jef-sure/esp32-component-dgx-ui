#pragma once
/*
 * Two oracles of a look that draws less than its whole, after a pass: the screen as the passes left it against the
 * tree composed anew by dgx_ui_paint_area_to() into another screen of the same size and depth, at the paint's moment.
 *
 *  differ_from_composed()  composed by the same paint: sees a row or strokes the passes left out of the screen; cannot
 *                          see what a canvas the look keeps lacks or holds too much of, since the same paint puts its
 *                          own canvas again.
 *  differ_from_fresh()     composed by a fresh paint, whose canvases are new and written from nothing: sees that too,
 *                          and what a kept canvas lacks or holds too much of.
 *
 * Neither sees a fault the look makes as much when it draws from nothing: the tests' references see that.
 */
#include "dgx_draw.h"
#include "dgx_ui_paint.h"

/* the pixels of the widget's box that differ between the screen painted and the tree composed anew into composed */
static inline int differ_from_composed(dgx_ui_t *ui, dgx_ui_paint_t *paint, dgx_screen_t *composed, int i)
{
    const dgx_ui_node_t *node = dgx_ui_node(ui, i);
    dgx_fill_rectangle(composed, 0, 0, composed->width, composed->height, 0);
    dgx_ui_paint_area_to(ui, paint, composed, NULL, 0, 0, composed->width, composed->height);
    int differ = 0;
    for (int y = node->y; y < node->y + node->h; ++y)
        for (int x = node->x; x < node->x + node->w; ++x) differ += dgx_get_pixel(paint->scr, x, y) != dgx_get_pixel(composed, x, y);
    return differ;
}

/* the pixels of the widget's box that differ between the screen painted and the tree composed at the paint's moment
   by a fresh paint of the same colours into composed */
static inline int differ_from_fresh(dgx_ui_t *ui, const dgx_ui_paint_t *paint, dgx_screen_t *composed, int i)
{
    const dgx_ui_node_t *node  = dgx_ui_node(ui, i);
    dgx_ui_paint_t       fresh = {.scr = composed, .colors = paint->colors, .color_count = paint->color_count, .now_ms = paint->now_ms};
    dgx_fill_rectangle(composed, 0, 0, composed->width, composed->height, 0);
    dgx_ui_paint_area_to(ui, &fresh, composed, NULL, 0, 0, composed->width, composed->height);
    dgx_ui_paint_free(&fresh);
    int differ = 0;
    for (int y = node->y; y < node->y + node->h; ++y)
        for (int x = node->x; x < node->x + node->w; ++x) differ += dgx_get_pixel(paint->scr, x, y) != dgx_get_pixel(composed, x, y);
    return differ;
}
