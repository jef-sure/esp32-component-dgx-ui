/*
 * dgx_ui_indexed_look.c
 *
 *  The look of pictures of 4 and 8 bits (dgx_image_t): their values, through the picture's map, are indices of the
 *  painter's table; they are blitted into an 8-bit virtual screen laid over the program's buffer, a strip of as many
 *  rows as the buffer takes at a time, and each strip sent through a table of RGB565 made from the painter's colours.
 *  A pixel 0 is the paper, as the rest of the box around the picture: every pixel once, nothing blinks. On the
 *  program's drawn background the box is laid and the picture sent over it, its index 0 leaving the background. A picture of 1
 *  bit goes to the look of a picture. It needs the virtual screens of DGX (CONFIG_DGX_ENABLE_VSCREEN), a buffer in
 *  the paint and a 16-bit screen, or on 18 or 24 bits the strips of pictures (dgx_ui_strips_pictures), each row of
 *  indices put through a row of the screen's depth; without them the box is paper, told once in the log.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <stdlib.h>

#include "dgx_draw.h"
#include "dgx_image.h"
#include "dgx_ui_paint_priv.h"
#include "dgx_vscreen.h"
#include "esp_log.h"

static const char *TAG = "dgx_ui";

/* the picture a widget shows now: its own, or the one of an icon's value; NULL none */
static const dgx_ui_picture_t *shown(const dgx_ui_node_t *n)
{
    const dgx_ui_widget_t *w = n->widget;
    if (!w->picture) return NULL;
    if (w->kind != DGX_UI_ICON) return w->picture;
    const int v = dgx_ui_value_of(n);
    return v >= w->min && v <= w->max ? &w->picture[v - w->min] : NULL;
}

/* the box less a hole, its paper (an index or DGX_UI_PAPER) laid */
static void paper_around(dgx_ui_paint_t *paint, const dgx_ui_node_t *n, const dgx_ui_rect_t *hole, uint8_t paper)
{
    const dgx_ui_rect_t box = dgx_ui_rect(n->x, n->y, n->w, n->h);
    dgx_ui_rect_t       parts[4];
    const int           k = dgx_ui_rect_subtract(&box, hole, parts);
    for (int j = 0; j < k; ++j) dgx_ui_lay_paper(paint, paper, parts[j].x, parts[j].y, parts[j].w, parts[j].h);
}

bool dgx_ui_draw_indexed(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t         *n   = dgx_ui_node(ui, i);
    const dgx_ui_picture_look_t *p   = look->params;
    const dgx_ui_picture_t      *pic = shown(n);
    const dgx_image_t           *im  = pic ? pic->image : NULL;
    if (im && im->format == DGX_IMAGE_1BPP) return dgx_ui_draw_picture(look, paint, ui, i, now_ms);
    const int           paper_index = p->paper == DGX_UI_PAPER ? paint->paper : p->paper;
    const dgx_ui_rect_t none        = dgx_ui_rect(0, 0, 0, 0);
    if (!im || (im->format != DGX_IMAGE_4BPP && im->format != DGX_IMAGE_8BPP) || pic->mask) {
        static bool told; /* a mask, which dgx_ui_c refuses here when it sees it: one of a description loaded or an icon's value */
        if (im && pic->mask && !told) {
            ESP_LOGW(TAG, "widget %d: a mask is drawn by dgx_ui_look_soft_picture: its box is paper", i);
            told = true;
        }
        paper_around(paint, n, &none, p->paper); /* none */
        return false;
    }
    /* on 18 or 24 bits through the strips of pictures (dgx_ui_strips_pictures): a row of indices and its row of the
       screen's depth in the buffer */
    const int                k      = dgx_ui_picture_scale(ui, pic); /* its pixel so many of the screen's, across and down */
    const bool               deep   = paint->scr->color_bits != 16;
    const dgx_ui_strips_t   *strips = paint->strips;
    size_t                   room;
    uint8_t                 *buffer = dgx_ui_buffer_free(paint, &room);
    const size_t             row    = (size_t)im->width + (deep ? dgx_ui_deep_row(im->width, k) : 0);
    const int                rows   = row ? (int)(room / row) : 0;
    if (!buffer || rows < 1 || (deep && !(strips && strips->put_indexed))) {
        static bool told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
        if (!told) {
            if (deep) ESP_LOGW(TAG, "widget %d: a picture of %d bits on %d bits: name dgx_ui_strips_pictures for the paint's strips, its buffer %u bytes a row at least",
                               i, im->format, paint->scr->color_bits, (unsigned)row);
            else ESP_LOGW(TAG, "widget %d: a picture of %d bits needs a buffer of a row at least (%d bytes) and a 16-bit screen", i, im->format, im->width);
        }
        told = true;
        paper_around(paint, n, &none, p->paper);
        return false;
    }
    /* the painter's own: an 8-bit screen over the buffer and, on 16 bits, the table of its indices, made at their first need */
    if (!paint->indexed) paint->indexed = dgx_vscreen_from_array(buffer, im->width, 1, 8, DgxScreenRGB);
    if (!deep && !paint->lut) paint->lut = malloc(256 * sizeof *paint->lut);
    if (!paint->indexed || (!deep && !paint->lut)) {
        paper_around(paint, n, &none, p->paper);
        return false;
    }
    for (int c = 0; !deep && c < 256; ++c) paint->lut[c] = (uint16_t)(c < paint->color_count && paint->colors ? paint->colors[c] : 0);
    /* in the middle of the box, the rest of it paper; then the picture strip by strip. On the drawn background the
       whole box laid, and the picture's index 0 not sent: the background stays there */
    const bool          drawn = dgx_ui_paper_drawn(paint, p->paper);
    const dgx_ui_rect_t hole  = dgx_ui_rect(n->x + (n->w - im->width * k) / 2, n->y + (n->h - im->height * k) / 2, im->width * k, im->height * k);
    paper_around(paint, n, drawn ? &none : &hole, p->paper);
    /* only the picture's rows in the clip, which the painter keeps within the box: a scaled row cut at the clip's edge
       drawn whole and cut by it; a part beside a cover, a band of a composing, costs its own rows */
    int cx, cy, cw, ch;
    dgx_screen_clip_get(paint->scr, &cx, &cy, &cw, &ch);
    const int top = cy + paint->oy > hole.y ? (cy + paint->oy - hole.y) / k : 0;
    const int end = cy + paint->oy + ch < hole.y + hole.h ? (cy + paint->oy + ch - hole.y + k - 1) / k : im->height;
    /* the table exact: one that runs straight from its first colour to its last is not mixed by a panel's hardware */
    const dgx_image_blit_t how = {.flags = DGX_IMAGE_OPAQUE, .map = pic->map, .bg = pic->solid ? pic->zero : drawn ? 0u : (uint32_t)paper_index};
    for (int y0 = top; y0 < end; y0 += rows) { /* y0 a row of the picture, from its own top */
        const int h = end - y0 < rows ? end - y0 : rows;
        dgx_vscreen_array_set(paint->indexed, buffer, im->width, h);
        dgx_image_blit(paint->indexed, im->origin_x, im->origin_y - y0, im, &how);
        if (deep) strips->put_indexed(paint, hole.x, hole.y + y0 * k, buffer, im->width, im->width, h, k, drawn, (size_t)im->width * (size_t)rows);
        else dgx_vscreen8_to_screen16(paint->scr, hole.x - paint->ox, hole.y + y0 * k - paint->oy, paint->indexed, paint->lut,
                                      DGX_VSCREEN_EXACT | DGX_VSCREEN_SCALE(k) | (drawn ? DGX_VSCREEN_TRANSPARENT : 0u));
    }
    return false;
}

static const dgx_ui_picture_look_t indexed_default = {.ink = 1, .paper = DGX_UI_PAPER};
const dgx_ui_look_t                dgx_ui_look_indexed = {.draw = dgx_ui_draw_indexed, .params = &indexed_default, .fill = DGX_UI_PAPER};
