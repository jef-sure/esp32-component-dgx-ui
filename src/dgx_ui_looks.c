/*
 * dgx_ui_looks.c
 *
 *  The looks of the kinds that hold: a box (paper) and a frame (a border and a fill). Drawn whole, each writes the
 *  pixels of its box its children do not cover, once: a child is drawn over nothing of it, and a child that went
 *  leaves no trace. The text's is dgx_ui_text_look.c. And the paper every look lays, with the letters of a field over
 *  it: a colour here; the program's drawn background through dgx_ui_background.c, which a program names by
 *  dgx_ui_background_set().
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_ui_paint_priv.h"

void dgx_ui_lay_paper(dgx_ui_paint_t *paint, uint8_t paper, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    if (dgx_ui_paper_drawn(paint, paper)) paint->drawn->lay(paint, x, y, w, h); /* the program's picture */
    else dgx_fill_rectangle(paint->scr, x - paint->ox, y - paint->oy, w, h, dgx_ui_color(paint, paper == DGX_UI_PAPER ? paint->paper : paper));
}

bool dgx_ui_frame_begin(dgx_ui_paint_t *paint, const dgx_ui_t *ui, int i, dgx_ui_rect_t rect, uint8_t color, dgx_ui_frame_t *frame)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    frame->rect = dgx_ui_rect(0, 0, 0, 0), frame->color = color;
    if (!n || !(dgx_ui_bits_shown(ui, n) & DGX_UI_FOCUSED) || color == DGX_UI_PAPER || dgx_ui_rect_empty(&rect)) return true;
    frame->rect = rect;
    int *was    = frame->clip;
    dgx_screen_clip_get(paint->scr, &was[0], &was[1], &was[2], &was[3]);
    /* the inside within the clip, in pixels of the paint's screen */
    const int point = ui->unit, left = rect.x + point - paint->ox, top = rect.y + point - paint->oy;
    const int right = left + rect.w - 2 * point, bottom = top + rect.h - 2 * point;
    const int x0 = left > was[0] ? left : was[0], y0 = top > was[1] ? top : was[1];
    const int x1 = right < was[0] + was[2] ? right : was[0] + was[2], y1 = bottom < was[1] + was[3] ? bottom : was[1] + was[3];
    if (x1 <= x0 || y1 <= y0) return false;
    dgx_screen_clip(paint->scr, x0, y0, x1 - x0, y1 - y0);
    return true;
}

void dgx_ui_frame_end(dgx_ui_paint_t *paint, const dgx_ui_t *ui, const dgx_ui_frame_t *frame)
{
    const uint8_t color = frame->color;
    const dgx_ui_rect_t r = frame->rect;
    if (dgx_ui_rect_empty(&r)) return;
    dgx_screen_clip(paint->scr, frame->clip[0], frame->clip[1], frame->clip[2], frame->clip[3]);
    const int point = ui->unit;
    if (r.w <= 2 * point || r.h <= 2 * point) { /* no inside: the frame is all of it */
        dgx_ui_lay_paper(paint, color, r.x, r.y, r.w, r.h);
        return;
    }
    dgx_ui_lay_paper(paint, color, r.x, r.y, r.w, point);                               /* above */
    dgx_ui_lay_paper(paint, color, r.x, r.y + r.h - point, r.w, point);                 /* below */
    dgx_ui_lay_paper(paint, color, r.x, r.y + point, point, r.h - 2 * point);           /* left */
    dgx_ui_lay_paper(paint, color, r.x + r.w - point, r.y + point, point, r.h - 2 * point); /* right */
}

void dgx_ui_field_text(dgx_ui_paint_t *paint, const dgx_font_context_t *ctx, int x, int y, const char *text, uint8_t paper)
{
    if (dgx_ui_paper_drawn(paint, paper)) paint->drawn->text(paint, ctx, x, y, text, paper); /* over the picture */
    else dgx_font_draw(ctx, x, y, text);
}

/* a rectangle of the area: of the colour, or the paper laid (a box: what lies under it) */
static void fill(dgx_ui_paint_t *paint, int x, int y, int w, int h, uint32_t color, bool lay)
{
    if (lay) dgx_ui_lay_paper(paint, DGX_UI_PAPER, x, y, w, h);
    else if (w > 0 && h > 0) dgx_fill_rectangle(paint->scr, x - paint->ox, y - paint->oy, w, h, color);
}

/* the area less the children: of the colour, or the paper laid */
static void around(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, dgx_ui_rect_t a, uint32_t color, bool lay)
{
    const dgx_ui_node_t *n      = dgx_ui_at(ui, i);
    const bool           column = n->widget->flags & DGX_UI_COLUMN, row = !column && (n->widget->flags & DGX_UI_ROW);
    if (!column && !row) {
        fill(paint, a.x, a.y, a.w, a.h, color, lay);
        return;
    }
    /* a cursor along the stacking: the strip up to each child across the area, beside the child on both sides */
    int at = column ? a.y : a.x;
    for (int c = n->child; c >= 0; c = dgx_ui_at(ui, c)->next) {
        const dgx_ui_node_t *k = dgx_ui_at(ui, c);
        dgx_ui_rect_t        r;
        if ((k->state.bits & DGX_UI_HIDDEN) || !dgx_ui_look_of(k->widget)) continue; /* covers nothing */
        if (!dgx_ui_rect_intersect(&(dgx_ui_rect_t){k->x, k->y, k->w, k->h}, &a, &r)) continue;
        if (column) {
            if (r.y < at) r.h -= at - r.y, r.y = at; /* over the one before: what is left of it */
            if (r.h <= 0) continue;
            fill(paint, a.x, at, a.w, r.y - at, color, lay);
            fill(paint, a.x, r.y, r.x - a.x, r.h, color, lay);
            fill(paint, r.x + r.w, r.y, a.x + a.w - r.x - r.w, r.h, color, lay);
            at = r.y + r.h;
        } else {
            if (r.x < at) r.w -= at - r.x, r.x = at;
            if (r.w <= 0) continue;
            fill(paint, at, a.y, r.x - at, a.h, color, lay);
            fill(paint, r.x, a.y, r.w, r.y - a.y, color, lay);
            fill(paint, r.x, r.y + r.h, r.w, a.y + a.h - r.y - r.h, color, lay);
            at = r.x + r.w;
        }
    }
    if (column) fill(paint, a.x, at, a.w, a.y + a.h - at, color, lay);
    else fill(paint, at, a.y, a.x + a.w - at, a.h, color, lay);
}

void dgx_ui_fill_around(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, dgx_ui_rect_t a, uint32_t color)
{
    around(paint, ui, i, a, color, false);
}

/* ---- a box: its paper laid around its children when it is drawn whole ---- */

bool dgx_ui_draw_box(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t *w = dgx_ui_node(ui, i);
    if (paint->whole) around(paint, ui, i, dgx_ui_rect(w->x, w->y, w->w, w->h), 0, true);
    return false;
}

const dgx_ui_look_t dgx_ui_look_box = {.draw = dgx_ui_draw_box, .fill = DGX_UI_PAPER};

/* ---- a frame: four strips of the border and the fill inside, all around its children (a child laid over the
   border is drawn over it, as when the frame is composed) ---- */

bool dgx_ui_draw_frame(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    if (!paint->whole) return false;
    const dgx_ui_node_t       *w = dgx_ui_node(ui, i);
    const dgx_ui_frame_look_t *p = look->params;
    const uint32_t             border = dgx_ui_color(paint, p->border);
    int                        bw     = p->width * ui->unit;
    if (2 * bw > w->w) bw = w->w / 2;
    if (2 * bw > w->h) bw = w->h / 2;
    /* the four strips of the border, then the inside */
    const dgx_ui_rect_t parts[5] = {
        dgx_ui_rect(w->x, w->y, w->w, bw),
        dgx_ui_rect(w->x, w->y + w->h - bw, w->w, bw),
        dgx_ui_rect(w->x, w->y + bw, bw, w->h - 2 * bw),
        dgx_ui_rect(w->x + w->w - bw, w->y + bw, bw, w->h - 2 * bw),
        dgx_ui_rect(w->x + bw, w->y + bw, w->w - 2 * bw, w->h - 2 * bw),
    };
    for (int k = 0; k < 5; ++k) dgx_ui_fill_around(paint, ui, i, parts[k], k < 4 ? border : dgx_ui_color(paint, look->fill));
    return false;
}

static const dgx_ui_frame_look_t frame_default = {.border = 3, .width = 1};
const dgx_ui_look_t              dgx_ui_look_frame = {.draw = dgx_ui_draw_frame, .params = &frame_default, .fill = 2};
