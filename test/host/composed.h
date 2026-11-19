#pragma once
/*
 * The oracle of a look that draws less than its whole (a row it skips, strokes it adds to those on the screen): after
 * a pass, the screen as the passes left it against the tree composed anew by dgx_ui_paint_area_to() into another
 * screen of the same size and depth, all of it drawn as it stands at the paint's last moment.
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
