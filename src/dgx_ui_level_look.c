/*
 * dgx_ui_level_look.c
 *
 *  The look of a level bar: the value of the widget's place within min..max as the filled part of its box from one
 *  edge, in one colour, that of the value's band, or each part in the band of the value its place stands for. Its parts
 *  do not overlap, every pixel once; when only the value changed, only the part between the filled length drawn and
 *  the new one is written, or, by the value shown, all of the filled part and what emptied when its colour changed. A
 *  program that names it not carries none of it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_paint_priv.h"

/* the filled length of a value in a box `length` pixels long: the value bounded first, as a place holds any int32;
   max not above min empty, with no division. The product of the widest box, 32767, and the widest range, 65535,
   fits 32 bits */
static int filled(const dgx_ui_widget_t *w, int32_t value, int length)
{
    if (w->max <= w->min || length <= 0 || value <= w->min) return 0;
    if (value >= w->max) return length;
    return (int)((uint32_t)(value - w->min) * (uint32_t)length / (uint32_t)(w->max - w->min));
}

/* the colour of a value: the first band that holds it, else bar */
static uint8_t color_of(const dgx_ui_level_look_t *p, int32_t value)
{
    for (int k = 0; k < p->band_count; ++k)
        if (value < p->bands[k].below) return p->bands[k].color;
    return p->fill;
}

/* a part along the bar, from `a` to `b` pixels from the edge it fills from, laid in a colour or the paper */
static void lay_along(dgx_ui_paint_t *paint, const dgx_ui_node_t *n, int from, int a, int b, uint8_t color);

/* the filled part from `a` to `b` along: one colour, the value's; or by place, each band from where its values begin */
static void lay_filled(dgx_ui_paint_t *paint, const dgx_ui_node_t *n, const dgx_ui_level_look_t *p, int length, int a, int b, uint8_t color)
{
    if (p->bands_by != DGX_UI_BANDS_BY_PLACE) {
        lay_along(paint, n, p->from, a, b, color);
        return;
    }
    int at = a;
    for (int k = 0; k < p->band_count && at < b; ++k) {
        const int end = filled(n->widget, p->bands[k].below, length); /* the band's values end there */
        if (end > at) lay_along(paint, n, p->from, at, end < b ? end : b, p->bands[k].color), at = end;
    }
    lay_along(paint, n, p->from, at, b, p->fill);
}

static void lay_along(dgx_ui_paint_t *paint, const dgx_ui_node_t *n, int from, int a, int b, uint8_t color)
{
    if (b <= a) return;
    switch (from) {
    case DGX_UI_FROM_RIGHT: dgx_ui_lay_paper(paint, color, n->x + n->w - b, n->y, b - a, n->h); break;
    case DGX_UI_FROM_TOP: dgx_ui_lay_paper(paint, color, n->x, n->y + a, n->w, b - a); break;
    case DGX_UI_FROM_BOTTOM: dgx_ui_lay_paper(paint, color, n->x, n->y + n->h - b, n->w, b - a); break;
    default: dgx_ui_lay_paper(paint, color, n->x + a, n->y, b - a, n->h); break;
    }
}

bool dgx_ui_draw_level(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t       *n = dgx_ui_node(ui, i);
    const dgx_ui_widget_t     *w = n->widget;
    const dgx_ui_level_look_t *p = look->params;
    static bool                told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
    dgx_ui_tell_no_place(i, w, "a level bar", &told);
    const bool    across = p->from != DGX_UI_FROM_BOTTOM && p->from != DGX_UI_FROM_TOP; /* as lay_along() lays any other */
    const int     length = across ? n->w : n->h;
    const int32_t value = dgx_ui_value_of(n), drawn = n->state_drawn.value;
    const int     now_filled = filled(w, value, length);
    const uint8_t bar        = color_of(p, value);
    int           a = 0, b = length; /* the part written, along the bar */
    if (!paint->whole && dgx_ui_bits_shown(ui, n) == n->state_drawn.bits) {
        /* only the value moved: the part between the length drawn and the new one; its colour changed, all of the
           filled part, and what emptied */
        const int  was_filled = filled(w, drawn, length);
        const bool recolored  = p->bands_by != DGX_UI_BANDS_BY_PLACE && color_of(p, drawn) != bar; /* by place the colours stay */
        b                     = was_filled < now_filled ? now_filled : was_filled;
        a                     = recolored ? 0 : was_filled < now_filled ? was_filled : now_filled;
    }
    lay_filled(paint, n, p, length, a, b < now_filled ? b : now_filled, bar);
    lay_along(paint, n, p->from, a > now_filled ? a : now_filled, b, p->track);
    return false;
}

static const dgx_ui_level_look_t level_default = {.fill = 3, .track = 2, .from = DGX_UI_FROM_LEFT};
const dgx_ui_look_t              dgx_ui_look_level = {.draw = dgx_ui_draw_level, .params = &level_default, .fill = DGX_UI_PAPER};
