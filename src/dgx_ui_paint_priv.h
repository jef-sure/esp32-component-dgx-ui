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

