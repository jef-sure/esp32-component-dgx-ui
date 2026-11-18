/*
 * dgx_ui_indexed_look.c
 *
 *  The look of pictures of 4 and 8 bits (dgx_image_t): their values, through the picture's map, are indices of the
 *  painter's table; they are blitted into an 8-bit virtual screen laid over the program's buffer, a strip of as many
 *  rows as the buffer takes at a time, and each strip sent through a table of RGB565 made from the painter's colours.
 *  A pixel 0 is the paper, as the rest of the box around the picture: every pixel once, nothing blinks. A picture of 1
 *  bit goes to the look of a picture. It needs the virtual screens of DGX (CONFIG_DGX_ENABLE_VSCREEN), a buffer in
 *  the paint and a 16-bit screen; without them the box is paper, told once in the log.
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
    const int v = n->state_value.value;
    return v >= w->min && v <= w->max ? &w->picture[v - w->min] : NULL;
}

static void fill(dgx_ui_paint_t *paint, int x, int y, int w, int h, uint32_t c)
{
    if (w > 0 && h > 0) dgx_fill_rectangle(paint->scr, x - paint->ox, y - paint->oy, w, h, c);
}

/* the box less a hole, paper */
static void paper_around(dgx_ui_paint_t *paint, const dgx_ui_node_t *n, const dgx_ui_rect_t *hole, uint32_t paper)
{
    const dgx_ui_rect_t box = dgx_ui_rect(n->x, n->y, n->w, n->h);
    dgx_ui_rect_t       parts[4];
    const int           k = dgx_ui_rect_subtract(&box, hole, parts);
    for (int j = 0; j < k; ++j) fill(paint, parts[j].x, parts[j].y, parts[j].w, parts[j].h, paper);
}

bool dgx_ui_draw_indexed(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t         *n   = dgx_ui_node(ui, i);
    const dgx_ui_picture_look_t *p   = look->params;
    const dgx_ui_picture_t      *pic = shown(n);
    const dgx_image_t           *im  = pic ? pic->image : NULL;
    if (im && im->format == DGX_IMAGE_1BPP) return dgx_ui_draw_picture(look, paint, ui, i, now_ms);
    const int      paper_index = p->paper == DGX_UI_PAPER ? paint->paper : p->paper;
    const uint32_t paper       = dgx_ui_color(paint, paper_index);
    size_t         room;
    uint8_t       *buffer = dgx_ui_buffer_free(paint, &room);
    const int      rows   = im && im->width ? (int)(room / im->width) : 0;
    if (!im || (im->format != DGX_IMAGE_4BPP && im->format != DGX_IMAGE_8BPP)) {
        fill(paint, n->x, n->y, n->w, n->h, paper); /* none */
        return false;
    }
    if (!buffer || rows < 1 || paint->scr->color_bits != 16) {
        static bool told;
        if (!told) ESP_LOGW(TAG, "widget %d: a picture of %d bits needs a buffer of a row at least (%d bytes) and a 16-bit screen", i, im->format, im->width);
        told = true;
        fill(paint, n->x, n->y, n->w, n->h, paper);
        return false;
    }
    /* the painter's own: an 8-bit screen over the buffer and the table of its indices, made at their first need */
    if (!paint->indexed) paint->indexed = dgx_vscreen_from_array(buffer, im->width, 1, 8, DgxScreenRGB);
    if (!paint->lut) paint->lut = malloc(256 * sizeof *paint->lut);
    if (!paint->indexed || !paint->lut) {
        fill(paint, n->x, n->y, n->w, n->h, paper);
        return false;
    }
    for (int k = 0; k < 256; ++k) paint->lut[k] = (uint16_t)(k < paint->color_count && paint->colors ? paint->colors[k] : 0);
    /* in the middle of the box, the rest of it paper; then the picture strip by strip */
    const dgx_ui_rect_t hole = dgx_ui_rect(n->x + (n->w - im->width) / 2, n->y + (n->h - im->height) / 2, im->width, im->height);
    paper_around(paint, n, &hole, paper);
    /* only the rows of the picture in the clip: a part beside a cover, a band of a composing, costs its own rows */
    int cx, cy, cw, ch;
    dgx_screen_clip_get(paint->scr, &cx, &cy, &cw, &ch);
    const int top = cy + paint->oy > hole.y ? cy + paint->oy - hole.y : 0;
    const int end = cy + paint->oy + ch < hole.y + im->height ? cy + paint->oy + ch - hole.y : im->height;
    /* the table exact: one that runs straight from its first colour to its last is not mixed by a panel's hardware */
    const dgx_image_blit_t how = {.flags = DGX_IMAGE_OPAQUE, .map = pic->map, .bg = (uint32_t)paper_index};
    for (int y0 = top; y0 < end; y0 += rows) {
        const int h = end - y0 < rows ? end - y0 : rows;
        dgx_vscreen_array_set(paint->indexed, buffer, im->width, h);
        dgx_image_blit(paint->indexed, im->origin_x, im->origin_y - y0, im, &how);
        dgx_vscreen8_to_screen16(paint->scr, hole.x - paint->ox, hole.y + y0 - paint->oy, paint->indexed, paint->lut, DGX_VSCREEN_EXACT);
    }
    return false;
}

static const dgx_ui_picture_look_t indexed_default = {.ink = 1, .paper = DGX_UI_PAPER};
const dgx_ui_look_t                dgx_ui_look_indexed = {.draw = dgx_ui_draw_indexed, .params = &indexed_default, .fill = -1};
