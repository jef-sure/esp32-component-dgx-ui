/*
 * dgx_ui_picture_look.c
 *
 *  The looks of a picture and of an icon. A dgx_image_t of 1 bit is written whole by DGX_IMAGE_OPAQUE, its zeros
 *  the paper; the rest of the box around it is paper too, so every pixel is written once and an icon drawn again
 *  does not blink. An icon shows the picture of its value, none outside min..max. Pictures of RGB565 are the
 *  second look, which needs the virtual screens of DGX.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_image.h"
#include "dgx_ui_paint_priv.h"

/* the picture a widget shows now: its own, or the one of an icon's value; NULL none */
static const dgx_ui_picture_t *shown(const dgx_ui_node_t *n)
{
    const dgx_ui_widget_t *w = n->widget;
    if (!w->picture) return NULL;
    if (w->kind != DGX_UI_ICON) return w->picture;
    const int v = dgx_ui_value_of(n);
    return v >= w->min && v <= w->max ? &w->picture[v - w->min] : NULL;
}

/* the box of the widget less a rectangle, its paper (an index or DGX_UI_PAPER) laid */
static void paper_around(dgx_ui_paint_t *paint, const dgx_ui_node_t *n, const dgx_ui_rect_t *hole, uint8_t paper)
{
    const dgx_ui_rect_t box = dgx_ui_rect(n->x, n->y, n->w, n->h);
    dgx_ui_rect_t       parts[4];
    const int           k = dgx_ui_rect_subtract(&box, hole, parts);
    for (int j = 0; j < k; ++j) dgx_ui_lay_paper(paint, paper, parts[j].x, parts[j].y, parts[j].w, parts[j].h);
}

bool dgx_ui_draw_picture(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t         *n   = dgx_ui_node(ui, i);
    const dgx_ui_picture_look_t *p   = look->params;
    const dgx_ui_picture_t      *pic = shown(n);
    const dgx_ui_rect_t          none = dgx_ui_rect(0, 0, 0, 0);
    if (!pic || !pic->image || pic->image->format != DGX_IMAGE_1BPP) {
        /* none, or of 4 or 8 bits: the look of indexed pictures draws those, through the paint's buffer */
        paper_around(paint, n, &none, p->paper);
        return false;
    }
    /* its zeros the paper, written with the ones; on the drawn background, the whole box laid and the ones over it */
    const bool          drawn = dgx_ui_paper_drawn(paint, p->paper);
    const dgx_image_t  *im    = pic->image;
    const int           k     = dgx_ui_picture_scale(ui, pic); /* its pixel so many of the screen's, across and down */
    const dgx_ui_rect_t hole  = dgx_ui_rect(n->x + (n->w - im->width * k) / 2, n->y + (n->h - im->height * k) / 2, im->width * k, im->height * k);
    paper_around(paint, n, drawn ? &none : &hole, p->paper);
    const uint32_t         paper = dgx_ui_color(paint, p->paper == DGX_UI_PAPER ? paint->paper : p->paper);
    const dgx_image_blit_t how   = {.flags = drawn ? 0 : DGX_IMAGE_OPAQUE, .color = dgx_ui_color(paint, p->ink), .bg = paper, .scale = (uint8_t)k};
    dgx_image_blit(paint->scr, hole.x - paint->ox + im->origin_x * k, hole.y - paint->oy + im->origin_y * k, im, &how); /* cut to its box by the painter's clip */
    return false;
}

static const dgx_ui_picture_look_t picture_default = {.ink = 1, .paper = DGX_UI_PAPER};
const dgx_ui_look_t                dgx_ui_look_picture = {.draw = dgx_ui_draw_picture, .params = &picture_default, .fill = -1};
