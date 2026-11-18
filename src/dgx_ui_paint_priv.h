#pragma once
/*
 * dgx_ui_paint_priv.h
 *
 *  What the painter and the looks of a box and a frame share: the look a widget is drawn by.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_paint.h"
#include "dgx_ui_priv.h"

/* the look of a widget: its own, else its kind's; NULL: nothing draws it */
const dgx_ui_look_t *dgx_ui_look_of(const dgx_ui_widget_t *w);

/* the part of the paint's buffer a look may use now, its bytes in *size: past the painter's strip while the widget is
   drawn in strips over the buffer (a display of 18 or 24 bits), else all of it; NULL none */
void *dgx_ui_buffer_free(const dgx_ui_paint_t *paint, size_t *size);

/* the painter's strips, named by the program (dgx_ui_strips, dgx_ui_strips.c): a part r of widget i drawn through the
   buffer, *more set when the look asks for time; false, not drawn: the painter draws the part straight */
struct dgx_ui_strips_t {
    bool (*draw)(dgx_ui_paint_t *paint, const dgx_ui_look_t *look, dgx_ui_t *ui, int i, uint32_t now_ms, dgx_ui_rect_t r, bool *more);
};
