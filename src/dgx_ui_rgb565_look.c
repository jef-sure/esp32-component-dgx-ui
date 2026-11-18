/*
 * dgx_ui_rgb565_look.c
 *
 *  The look of a picture or an icon of RGB565: a virtual screen of DGX, kept by the paint and pointed at each
 *  picture's pixels in turn (dgx_vscreen_array_set(): a few assignments), transferred every pixel once in the
 *  middle of the box, the rest of it paper. The screen is made at the first need and given back by
 *  dgx_ui_paint_free(). It needs CONFIG_DGX_ENABLE_VSCREEN and a screen of 16 bits; on another the box is paper. A
 *  program that does not name this look does not carry it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_vscreen.h"
#include "esp_log.h"

static const char *TAG = "dgx_ui";

static const dgx_ui_picture_t *shown(const dgx_ui_node_t *n)
{
    const dgx_ui_widget_t *w = n->widget;
    if (!w->picture) return NULL;
    if (w->kind != DGX_UI_ICON) return w->picture;
    const int v = n->state_value.value;
    return v >= w->min && v <= w->max ? &w->picture[v - w->min] : NULL;
}

/* the paint's screen over a picture's pixels: made at the first need, pointed after; NULL when it cannot be had */
static dgx_screen_t *source(dgx_ui_paint_t *paint, const dgx_ui_picture_t *pic)
{
    void *pixels = (void *)pic->rgb565; /* read only: the transfer reads the source, never writes it */
    if (paint->rgb565) {
        dgx_vscreen_array_set(paint->rgb565, pixels, pic->width, pic->height);
        return paint->rgb565;
    }
    paint->rgb565 = dgx_vscreen_from_array(pixels, pic->width, pic->height, 16, DgxScreenRGB);
    if (!paint->rgb565) ESP_LOGE(TAG, "no memory for the screen of a picture of RGB565: its box is drawn as paper");
    return paint->rgb565;
}

bool dgx_ui_draw_rgb565(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t         *n     = dgx_ui_node(ui, i);
    const dgx_ui_picture_look_t *p     = look->params;
    const dgx_ui_picture_t      *pic   = shown(n);
    const dgx_ui_rect_t          box   = dgx_ui_rect(n->x, n->y, n->w, n->h);
    dgx_ui_rect_t                hole  = dgx_ui_rect(0, 0, 0, 0);
    const bool    fits = pic && !pic->image && pic->rgb565 && pic->width > 0 && pic->height > 0 && paint->scr->color_bits == 16;
    dgx_screen_t *src  = fits ? source(paint, pic) : NULL;
    if (src) hole = dgx_ui_rect(n->x + (n->w - pic->width) / 2, n->y + (n->h - pic->height) / 2, pic->width, pic->height);
    dgx_ui_rect_t parts[4];
    const int     k = dgx_ui_rect_subtract(&box, &hole, parts);
    for (int j = 0; j < k; ++j) dgx_ui_lay_paper(paint, p->paper, parts[j].x, parts[j].y, parts[j].w, parts[j].h);
    if (src) dgx_vscreen_to_screen(paint->scr, hole.x - paint->ox, hole.y - paint->oy, src, 0);
    return false;
}

static const dgx_ui_picture_look_t rgb565_default = {.ink = 1, .paper = DGX_UI_PAPER};
const dgx_ui_look_t                dgx_ui_look_rgb565 = {.draw = dgx_ui_draw_rgb565, .params = &rgb565_default, .fill = -1};
