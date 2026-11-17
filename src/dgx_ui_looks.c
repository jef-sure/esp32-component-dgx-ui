/*
 * dgx_ui_looks.c
 *
 *  The looks of the kinds that hold: a box (paper) and a frame (a border and a fill). Each writes every pixel of its
 *  box once when drawn whole. The text's is dgx_ui_text_look.c.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_ui_paint.h"

/* ---- a box: its paper when it is drawn whole ---- */

static bool box_draw(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t *w = dgx_ui_node(ui, i);
    if (paint->whole) dgx_fill_rectangle(paint->scr, w->x - paint->ox, w->y - paint->oy, w->w, w->h, dgx_ui_color(paint, paint->paper));
    return false;
}

const dgx_ui_look_t dgx_ui_look_box = {.draw = box_draw, .fill = -1};

/* ---- a frame: four strips of the border and the fill inside ---- */

static bool frame_draw(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    if (!paint->whole) return false;
    const dgx_ui_node_t       *w = dgx_ui_node(ui, i);
    const dgx_ui_frame_look_t *p = look->params;
    const uint32_t             border = dgx_ui_color(paint, p->border), fill = dgx_ui_color(paint, look->fill);
    int                        bw     = p->width * ui->unit;
    if (2 * bw > w->w) bw = w->w / 2;
    if (2 * bw > w->h) bw = w->h / 2;
    dgx_screen_t *s = paint->scr;
    const int     x = w->x - paint->ox, y = w->y - paint->oy;
    if (bw > 0) {
        dgx_fill_rectangle(s, x, y, w->w, bw, border);
        dgx_fill_rectangle(s, x, y + w->h - bw, w->w, bw, border);
        dgx_fill_rectangle(s, x, y + bw, bw, w->h - 2 * bw, border);
        dgx_fill_rectangle(s, x + w->w - bw, y + bw, bw, w->h - 2 * bw, border);
    }
    if (w->w > 2 * bw && w->h > 2 * bw) dgx_fill_rectangle(s, x + bw, y + bw, w->w - 2 * bw, w->h - 2 * bw, fill);
    return false;
}

static const dgx_ui_frame_look_t frame_default = {.border = 3, .width = 1};
const dgx_ui_look_t              dgx_ui_look_frame = {.draw = frame_draw, .params = &frame_default, .fill = 2};
