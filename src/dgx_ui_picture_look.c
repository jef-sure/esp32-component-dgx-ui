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
#include "dgx_ui_paint.h"

/* the picture a widget shows now: its own, or the one of an icon's value; NULL none */
static const dgx_ui_picture_t *shown(const dgx_ui_node_t *n)
{
    const dgx_ui_widget_t *w = n->widget;
    if (!w->picture) return NULL;
    if (w->kind != DGX_UI_ICON) return w->picture;
    const int v = n->state_value.value;
    return v >= w->min && v <= w->max ? &w->picture[v - w->min] : NULL;
}

/* the box of the widget less a rectangle, paper */
static void paper_around(dgx_ui_paint_t *paint, const dgx_ui_node_t *n, const dgx_ui_rect_t *hole, uint32_t paper)
{
    const dgx_ui_rect_t box = dgx_ui_rect(n->x, n->y, n->w, n->h);
    dgx_ui_rect_t       parts[4];
    const int           k = dgx_ui_rect_subtract(&box, hole, parts);
    for (int j = 0; j < k; ++j) dgx_fill_rectangle(paint->scr, parts[j].x - paint->ox, parts[j].y - paint->oy, parts[j].w, parts[j].h, paper);
}

static bool picture_draw(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t         *n   = dgx_ui_node(ui, i);
    const dgx_ui_picture_look_t *p   = look->params;
    const dgx_ui_picture_t      *pic = shown(n);
    const uint32_t paper = dgx_ui_color(paint, p->paper == DGX_UI_PAPER ? paint->paper : p->paper);
    if (!pic || !pic->image || pic->image->format != DGX_IMAGE_1BPP) {
        /* none, or of 4 or 8 bits: through the buffer, not yet */
        paper_around(paint, n, &(dgx_ui_rect_t){0, 0, 0, 0}, paper);
        return false;
    }
    const dgx_image_t  *im   = pic->image;
    const dgx_ui_rect_t hole = dgx_ui_rect(n->x + (n->w - im->width) / 2, n->y + (n->h - im->height) / 2, im->width, im->height);
    paper_around(paint, n, &hole, paper);
    const dgx_image_blit_t how = {.flags = DGX_IMAGE_OPAQUE, .color = dgx_ui_color(paint, p->ink), .bg = paper};
    dgx_image_blit(paint->scr, hole.x - paint->ox + im->origin_x, hole.y - paint->oy + im->origin_y, im, &how);
    return false;
}

static const dgx_ui_picture_look_t picture_default = {.ink = 1, .paper = DGX_UI_PAPER};
const dgx_ui_look_t                dgx_ui_look_picture = {.draw = picture_draw, .params = &picture_default, .fill = -1};
