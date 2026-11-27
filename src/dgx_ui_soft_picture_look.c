/*
 * dgx_ui_soft_picture_look.c
 *
 *  The look of soft pictures: a mask, how much of the ink covers each pixel, and a picture of 16 bits with or without
 *  its plane of opacity, their edges mixed by DGX (DGX_IMAGE_SOFT). Over the paper of a colour the box is written
 *  whole, every pixel once: the paper around the picture, the picture by DGX_IMAGE_OPAQUE, its edges mixed with that
 *  colour on any screen. Over the program's drawn background the box is laid and the picture drawn over it: mixed with
 *  what lies there on a screen that can be read (a virtual screen, the strips, a DSI panel), the one case of a pixel
 *  written twice; on one that cannot, a pixel half covered or more takes the picture whole, less leaves the background.
 *  A picture of 1 bit goes to the look of a picture. It needs neither the virtual screens of DGX nor a buffer. A
 *  program that does not name this look does not carry it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

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

bool dgx_ui_draw_soft_picture(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t         *n   = dgx_ui_node(ui, i);
    const dgx_ui_picture_look_t *p   = look->params;
    const dgx_ui_picture_t      *pic = shown(n);
    const dgx_image_t           *im  = pic ? pic->image : NULL;
    if (im && im->format == DGX_IMAGE_1BPP) return dgx_ui_draw_picture(look, paint, ui, i, now_ms);
    /* what it draws: a mask of 4 or 8 bits, a dgx_image_t of 16 bits, or rows of RGB565 seen as one */
    dgx_image_t rows;
    const bool  mask = im && pic->mask && (im->format == DGX_IMAGE_4BPP || im->format == DGX_IMAGE_8BPP);
    if (!pic || (im && !mask && im->format != DGX_IMAGE_16BPP)) {
        if (im) {
            static bool told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
            if (!told) ESP_LOGW(TAG, "widget %d: a picture of %d bits that is not a mask: dgx_ui_look_indexed draws it, its box is paper", i, im->format);
            told = true;
        }
        im = NULL;
    } else if (!im) {
        rows = (dgx_image_t){.width = (uint16_t)pic->width, .height = (uint16_t)pic->height, .format = DGX_IMAGE_16BPP, .data = (const uint8_t *)pic->rgb565};
        im   = pic->rgb565 && pic->width > 0 && pic->height > 0 ? &rows : NULL;
    }
    const dgx_ui_rect_t none = dgx_ui_rect(0, 0, 0, 0);
    if (!im) {
        paper_around(paint, n, &none, p->paper); /* none */
        return false;
    }
    const int           k     = dgx_ui_picture_scale(ui, pic); /* its pixel so many of the screen's, across and down */
    const dgx_ui_rect_t hole  = dgx_ui_rect(n->x + (n->w - im->width * k) / 2, n->y + (n->h - im->height * k) / 2, im->width * k, im->height * k);
    const bool          drawn = dgx_ui_paper_drawn(paint, p->paper);
    /* over a colour the paper around it and the picture's own box written once, its edges mixed with that colour; over
       the drawn background the whole box laid, the picture mixed with it where the screen can be read */
    paper_around(paint, n, drawn ? &none : &hole, p->paper);
    const uint32_t         ink   = dgx_ui_color(paint, p->ink);
    const uint32_t         paper = dgx_ui_color(paint, p->paper == DGX_UI_PAPER ? paint->paper : p->paper);
    const dgx_image_blit_t how   = {.flags = DGX_IMAGE_SOFT | (mask ? DGX_IMAGE_SILHOUETTE : 0u) | (drawn ? 0u : DGX_IMAGE_OPAQUE),
                                    .color = ink, .bg = dgx_ui_under_ink(paint, ink, paper), .scale = (uint8_t)k};
    dgx_image_blit(paint->scr, hole.x - paint->ox + im->origin_x * k, hole.y - paint->oy + im->origin_y * k, im, &how); /* cut to its box by the painter's clip */
    return false;
}

static const dgx_ui_picture_look_t soft_picture_default = {.ink = 1, .paper = DGX_UI_PAPER};
const dgx_ui_look_t dgx_ui_look_soft_picture = {.draw = dgx_ui_draw_soft_picture, .params = &soft_picture_default, .fill = DGX_UI_PAPER};
