/*
 * dgx_ui_indexed_look.c
 *
 *  The look of pictures of 4 and 8 bits (dgx_image_t): their values, through the picture's map, are indices of the
 *  painter's table, drawn by DGX in those colours on a screen of any depth. Over a paper of a colour the paper is
 *  around the picture and its value 0 too: every pixel of the box once, nothing blinks. Over the program's drawn
 *  background the box is laid and the picture drawn over it, a pixel of the index 0 leaving the background; over a
 *  hatched paper on a screen of 1 bit the same, its value 0 leaving the hatch. A value whose colour is DGX_UI_HATCH is
 *  lit, as letters are. A picture of 1 bit goes to the look of a picture. It needs neither the virtual screens of DGX
 *  nor a buffer. The colours of a picture of 4 bits lie on the stack, those of one of 8 bits in a table the paint
 *  keeps, made at its first need.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <stdlib.h>

#include "dgx_draw.h"
#include "dgx_image.h"
#include "dgx_ui_paint_priv.h"
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
    const int           count = dgx_ui_rect_subtract(&box, hole, parts);
    for (int k = 0; k < count; ++k) dgx_ui_lay_paper(paint, paper, parts[k].x, parts[k].y, parts[k].w, parts[k].h);
}

bool dgx_ui_draw_indexed(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t         *n   = dgx_ui_node(ui, i);
    const dgx_ui_picture_look_t *p   = look->params;
    const dgx_ui_picture_t      *pic = shown(n);
    const dgx_image_t           *im  = pic ? pic->image : NULL;
    if (im && im->format == DGX_IMAGE_1BPP) return dgx_ui_draw_picture(look, paint, ui, i, now_ms);
    const dgx_ui_rect_t none = dgx_ui_rect(0, 0, 0, 0);
    if (!im || (im->format != DGX_IMAGE_4BPP && im->format != DGX_IMAGE_8BPP) || pic->mask) {
        static bool told; /* a mask, which dgx_ui_c refuses here when it sees it: one of a description loaded or an icon's value */
        if (im && pic->mask && !told) {
            ESP_LOGW(TAG, "widget %d: a mask is drawn by dgx_ui_look_soft_picture: its box is paper", i);
            told = true;
        }
        paper_around(paint, n, &none, p->paper);
        return false;
    }
    const bool drawn = dgx_ui_paper_drawn(paint, p->paper);
    /* the colours of the values: 16 on the stack, 256 in the paint's table, each followed by the opacities */
    const int values = im->format == DGX_IMAGE_4BPP ? 16 : 256;
    uint32_t  few[16 + 16 / 4];
    uint32_t *colors = few;
    if (values > 16) {
        if (!paint->values) paint->values = malloc(256 * sizeof *paint->values + 256);
        if (!paint->values) {
            ESP_LOGE(TAG, "no memory for the colours of a picture of 8 bits: its box is drawn as paper");
            paper_around(paint, n, &none, p->paper);
            return false;
        }
        colors = paint->values;
    }
    uint8_t *const alpha = (uint8_t *)(colors + values);
    /* over the drawn background, or a hatched paper, the box laid and the picture put over it: on the background a
       pixel of the index 0 shows it, on the hatch the value 0 */
    const int  paper   = p->paper == DGX_UI_PAPER ? paint->paper : p->paper;
#if DGX_UI_HATCHES
    const bool hatched = !drawn && paint->scr->color_bits == 1 && dgx_ui_color(paint, paper) == DGX_UI_HATCH;
#else
    const bool hatched = false;
#endif
    for (int v = 0; v < values; ++v) {
        const int index = v ? pic->map ? pic->map[v - 1] : v : pic->solid ? pic->zero : drawn ? 0 : paper;
        colors[v]       = dgx_ui_color(paint, index);
        alpha[v]        = drawn ? (index ? 255u : 0u) : v || pic->solid ? 255u : 0u;
    }
    const int           k    = dgx_ui_picture_scale(ui, pic); /* its pixel so many of the screen's, across and down */
    const dgx_ui_rect_t hole = dgx_ui_rect(n->x + (n->w - im->width * k) / 2, n->y + (n->h - im->height * k) / 2, im->width * k, im->height * k);
    paper_around(paint, n, drawn || hatched ? &none : &hole, p->paper);
    const dgx_image_blit_t how = {.flags  = drawn || hatched ? 0u : DGX_IMAGE_OPAQUE,
                                  .colors = colors,
                                  .alpha  = drawn || hatched ? alpha : NULL,
                                  .bg     = colors[0],
                                  .scale  = (uint8_t)k};
    dgx_image_blit(paint->scr, hole.x - paint->ox + im->origin_x * k, hole.y - paint->oy + im->origin_y * k, im, &how); /* cut to its box by the painter's clip */
    return false;
}

static const dgx_ui_picture_look_t indexed_default = {.ink = 1, .paper = DGX_UI_PAPER};
const dgx_ui_look_t                dgx_ui_look_indexed = {.draw = dgx_ui_draw_indexed, .params = &indexed_default, .fill = DGX_UI_PAPER};
