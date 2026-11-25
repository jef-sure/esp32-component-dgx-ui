/*
 * dgx_ui_rgb565_look.c
 *
 *  The look of a picture or an icon of RGB565: a virtual screen of DGX, kept by the paint and pointed at each
 *  picture's pixels in turn (dgx_vscreen_array_set(): a few assignments), transferred every pixel once in the
 *  middle of the box, the rest of it paper. The screen is made at the first need and given back by
 *  dgx_ui_paint_free(). It needs CONFIG_DGX_ENABLE_VSCREEN and a screen of 16 bits, or on 18 or 24 bits the strips
 *  of pictures (dgx_ui_strips_pictures), its rows in the clip spread to the screen's depth; else the box is paper. A
 *  program that does not name this look does not carry it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_ui_paint_priv.h"
#include "dgx_vscreen.h"
#include "esp_log.h"

static const char *TAG = "dgx_ui";

static const dgx_ui_picture_t *shown(const dgx_ui_node_t *n)
{
    const dgx_ui_widget_t *w = n->widget;
    if (!w->picture) return NULL;
    if (w->kind != DGX_UI_ICON) return w->picture;
    const int v = dgx_ui_value_of(n);
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
    const bool             ours   = pic && !pic->image && pic->rgb565 && pic->width > 0 && pic->height > 0;
    const dgx_ui_strips_t *strips = paint->strips;
    /* on 18 or 24 bits through the strips of pictures (dgx_ui_strips_pictures); on 16 the picture's own rows */
    const bool    deep = ours && paint->scr->color_bits != 16 && strips && strips->put_rgb565;
    dgx_screen_t *src  = ours && paint->scr->color_bits == 16 ? source(paint, pic) : NULL;
    const int     by   = pic ? dgx_ui_picture_scale(ui, pic) : 1; /* its pixel so many of the screen's, across and down */
    if (ours && paint->scr->color_bits != 16 && !deep) {
        static bool told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
        if (!told) ESP_LOGW(TAG, "widget %d: a picture of RGB565 on %d bits: name dgx_ui_strips_pictures for the paint's strips", i, paint->scr->color_bits);
        told = true;
    }
    if (src || deep) hole = dgx_ui_rect(n->x + (n->w - pic->width * by) / 2, n->y + (n->h - pic->height * by) / 2, pic->width * by, pic->height * by);
    dgx_ui_rect_t parts[4];
    const int     count = dgx_ui_rect_subtract(&box, &hole, parts);
    for (int j = 0; j < count; ++j) dgx_ui_lay_paper(paint, p->paper, parts[j].x, parts[j].y, parts[j].w, parts[j].h);
    if (src) dgx_vscreen_to_screen(paint->scr, hole.x - paint->ox, hole.y - paint->oy, src, DGX_VSCREEN_SCALE(by)); /* cut to its box by the painter's clip */
    /* only the picture's rows in the clip, which the painter keeps within the box */
    int cx, cy, cw, ch;
    dgx_screen_clip_get(paint->scr, &cx, &cy, &cw, &ch);
    const int top = deep && cy + paint->oy > hole.y ? (cy + paint->oy - hole.y) / by : 0;
    const int end = deep && cy + paint->oy + ch < hole.y + hole.h ? (cy + paint->oy + ch - hole.y + by - 1) / by : pic ? pic->height : 0;
    if (deep && top < end && !strips->put_rgb565(paint, hole.x, hole.y + top * by, pic->rgb565 + (size_t)top * pic->width, pic->width, pic->width, end - top, by)) {
        dgx_ui_lay_paper(paint, p->paper, hole.x, hole.y, hole.w, hole.h); /* no room for a row of the screen's depth */
        static bool told;
        if (!told) ESP_LOGW(TAG, "widget %d: a picture of RGB565 on %d bits needs a buffer of %u bytes a row", i, paint->scr->color_bits, (unsigned)dgx_ui_deep_row(pic->width, by));
        told = true;
    }
    return false;
}

static const dgx_ui_picture_look_t rgb565_default = {.ink = 1, .paper = DGX_UI_PAPER};
const dgx_ui_look_t                dgx_ui_look_rgb565 = {.draw = dgx_ui_draw_rgb565, .params = &rgb565_default, .fill = -1};
