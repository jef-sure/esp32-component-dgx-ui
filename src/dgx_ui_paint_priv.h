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

/* the part of the paint's buffer a look may use now, its bytes in *size: today all of it; NULL none. When the painter
   comes to keep a strip of its own there (a picture for the background), the rest */
void *dgx_ui_buffer_free(const dgx_ui_paint_t *paint, size_t *size);

