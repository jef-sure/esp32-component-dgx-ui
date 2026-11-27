/*
 * dgx_ui_nine_look.c
 *
 *  The look of a frame of nine pictures, a box or a frame holding children: the fill under all, the corners as they
 *  are, the edges and the middle laid again and again from their cell's top-left corner, cut by the cell, never
 *  stretched. The cells are of the corners' sizes: the left and the top of the top-left's, the right of the top-right's,
 *  the bottom of the bottom-left's, each piece at its own scale. A box smaller than its corners cuts them, the far ones
 *  kept at the box's far edges, none over another. Drawn in strips of an 8-bit virtual screen over the paint's buffer,
 *  each piece through its map, a picture of 1 bit in ink, and sent through the table of the painter's colours around the
 *  children: every pixel of the box once. It needs the virtual screens of DGX (CONFIG_DGX_ENABLE_VSCREEN), a buffer
 *  of a row of the box and a 16-bit screen, or on 18 or 24 bits the strips of pictures (dgx_ui_strips_pictures);
 *  without them, or with no pieces, it is the fill alone, told once in the log. A program that names it not carries
 *  none of it.
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

enum { PIECES = 9, MOST_PARTS = 48 };

/* a piece this look takes, and its size on the screen: a picture of 1, 4 or 8 bits; false for none or another */
static bool piece_size(const dgx_ui_t *ui, const dgx_ui_picture_t *piece, int *w, int *h, int *scale)
{
    const dgx_image_t *im = piece ? piece->image : NULL;
    *w = *h = 0, *scale = 1;
    if (!im || (im->format != DGX_IMAGE_1BPP && im->format != DGX_IMAGE_4BPP && im->format != DGX_IMAGE_8BPP)) return false;
    *scale = dgx_ui_picture_scale(ui, piece);
    *w = im->width * *scale, *h = im->height * *scale;
    return true;
}

/* the box less the children that cover it: the parts of a strip that are the frame's to send; false when they are
   more than the room for them */
static bool parts_of(const dgx_ui_t *ui, int i, dgx_ui_rect_t strip, dgx_ui_rect_t *parts, int *count)
{
    *count = 1, parts[0] = strip;
    for (int c = dgx_ui_at(ui, i)->child; c >= 0; c = dgx_ui_at(ui, c)->next) {
        const dgx_ui_node_t *k = dgx_ui_at(ui, c);
        if ((k->state.bits & DGX_UI_HIDDEN) || !dgx_ui_look_of(k->widget)) continue; /* covers nothing */
        const dgx_ui_rect_t child = dgx_ui_rect(k->x, k->y, k->w, k->h);
        dgx_ui_rect_t       left[MOST_PARTS];
        int                 kept = 0;
        for (int p = 0; p < *count; ++p) {
            dgx_ui_rect_t cut[4];
            const int     n = dgx_ui_rect_subtract(&parts[p], &child, cut);
            if (kept + n > MOST_PARTS) return false;
            for (int j = 0; j < n; ++j) left[kept++] = cut[j];
        }
        for (int p = 0; p < kept; ++p) parts[p] = left[p];
        *count = kept;
    }
    return true;
}

bool dgx_ui_draw_nine(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    if (!paint->whole) return false;
    const dgx_ui_node_t      *n   = dgx_ui_node(ui, i);
    const dgx_ui_nine_look_t *p   = look->params;
    const dgx_ui_rect_t       box = dgx_ui_rect(n->x, n->y, n->w, n->h);
    const uint32_t            fill = dgx_ui_color(paint, p->fill);
    /* the pieces, their sizes and scales; the cells by the corners, cut to the box, none over another */
    int  w[PIECES], h[PIECES], scale[PIECES], taken = 0;
    bool other = false;
    for (int k = 0; k < PIECES && p->pieces; ++k) {
        const bool took = piece_size(ui, &p->pieces[k], &w[k], &h[k], &scale[k]);
        taken += took, other |= !took && p->pieces[k].rgb565;
    }
    if (other) {
        static bool told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
        if (!told) ESP_LOGW(TAG, "widget %d: a frame of nine pictures takes pictures of 1, 4 or 8 bits: a piece of RGB565 is its cell's fill", i);
        told = true;
    }
    size_t         room;
    uint8_t       *buffer = dgx_ui_buffer_free(paint, &room);
    int            cx, cy, cw, ch;
    dgx_screen_clip_get(paint->scr, &cx, &cy, &cw, &ch);
    dgx_ui_rect_t  part;
    const bool     shown = dgx_ui_rect_intersect(&box, &(dgx_ui_rect_t){cx + paint->ox, cy + paint->oy, cw, ch}, &part);
    /* on 18 or 24 bits through the strips of pictures (dgx_ui_strips_pictures): a row of the screen's depth beside a row
       of indices */
    const bool             deep   = paint->scr->color_bits != 16;
    const dgx_ui_strips_t *strips = paint->strips;
    const size_t           row    = (size_t)part.w + (deep ? dgx_ui_deep_row(part.w, 1) : 0);
    const int              rows   = shown && part.w ? (int)(room / row) : 0;
    if (!taken || !shown || !buffer || rows < 1 || (deep && !(strips && strips->put_indexed))) {
        if (taken && shown) {
            static bool told;
            if (!told) {
                if (deep) ESP_LOGW(TAG, "widget %d: a frame of nine pictures on %d bits: name dgx_ui_strips_pictures for the paint's strips, its buffer %u bytes a row at least: its fill alone",
                                   i, paint->scr->color_bits, (unsigned)row);
                else ESP_LOGW(TAG, "widget %d: a frame of nine pictures needs a buffer of a row of its box (%d bytes) and a 16-bit screen: its fill alone", i, n->w);
            }
            told = true;
        }
        dgx_ui_fill_around(paint, ui, i, box, fill);
        return false;
    }
    /* the painter's own: an 8-bit screen over the buffer and the table of its indices, made at their first need */
    if (!paint->indexed) paint->indexed = dgx_vscreen_from_array(buffer, part.w, 1, 8, DgxScreenRGB);
    if (!deep && !paint->lut) paint->lut = malloc(256 * sizeof *paint->lut);
    if (!paint->indexed || (!deep && !paint->lut)) {
        dgx_ui_fill_around(paint, ui, i, box, fill);
        return false;
    }
    for (int k = 0; !deep && k < 256; ++k) paint->lut[k] = (uint16_t)(k < paint->color_count && paint->colors ? paint->colors[k] : 0);
    const int right  = w[2] < box.w / 2 ? w[2] : box.w / 2, left = w[0] < box.w - right ? w[0] : box.w - right;
    const int bottom = h[6] < box.h / 2 ? h[6] : box.h / 2, top = h[0] < box.h - bottom ? h[0] : box.h - bottom;
    const int xs[3] = {box.x, box.x + left, box.x + box.w - right}, ws[3] = {left, box.w - left - right, right};
    const int ys[3] = {box.y, box.y + top, box.y + box.h - bottom}, hs[3] = {top, box.h - top - bottom, bottom};
    /* where a piece starts: its cell's corner; the far column and row from the box's far edges, so that a corner cut
       keeps its outer edge, as when it is whole */
    const int starts_x[3] = {xs[0], xs[1], box.x + box.w - w[2]}, starts_y[3] = {ys[0], ys[1], box.y + box.h - h[6]};
    dgx_ui_rect_t parts[MOST_PARTS];
    int           count;
    for (int y0 = part.y; y0 < part.y + part.h; y0 += rows) {
        const dgx_ui_rect_t strip = dgx_ui_rect(part.x, y0, part.w, part.y + part.h - y0 < rows ? part.y + part.h - y0 : rows);
        if (!parts_of(ui, i, strip, parts, &count)) { /* more pieces round the children than a strip keeps: as a frame */
            dgx_ui_fill_around(paint, ui, i, strip, fill);
            continue;
        }
        if (!count) continue;
        dgx_vscreen_array_set(paint->indexed, buffer, strip.w, strip.h);
        dgx_fill_rectangle(paint->indexed, 0, 0, strip.w, strip.h, (uint32_t)p->fill);
        for (int k = 0; k < PIECES; ++k) {
            const dgx_ui_rect_t cell = dgx_ui_rect(xs[k % 3], ys[k / 3], ws[k % 3], hs[k / 3]);
            dgx_ui_rect_t       here;
            if (!w[k] || !h[k] || !dgx_ui_rect_intersect(&cell, &strip, &here)) continue;
            const dgx_image_t     *im     = p->pieces[k].image;
            const bool             corner = k == 0 || k == 2 || k == 6 || k == 8;
            const dgx_image_blit_t how    = {.flags = DGX_IMAGE_CLIP | (p->pieces[k].solid ? DGX_IMAGE_OPAQUE : 0u), .map = p->pieces[k].map, .color = p->ink,
                                             .bg = p->pieces[k].zero, .scale = (uint8_t)scale[k],
                                             .clip_x = (int16_t)(here.x - strip.x), .clip_y = (int16_t)(here.y - strip.y),
                                             .clip_w = (int16_t)here.w, .clip_h = (int16_t)here.h};
            /* a corner once; an edge and the middle again and again, the tiles in the strip alone */
            const int sx = starts_x[k % 3], sy = starts_y[k / 3];
            const int first_y = corner ? sy : sy + (here.y > sy ? (here.y - sy) / h[k] * h[k] : 0);
            for (int ty = first_y; ty < here.y + here.h; ty += h[k]) {
                const int first_x = corner ? sx : sx + (here.x > sx ? (here.x - sx) / w[k] * w[k] : 0);
                for (int tx = first_x; tx < here.x + here.w; tx += w[k]) {
                    dgx_image_blit(paint->indexed, tx - strip.x + im->origin_x * scale[k], ty - strip.y + im->origin_y * scale[k], im, &how);
                    if (corner) break;
                }
                if (corner) break;
            }
        }
        /* the strip sent around the children, under the painter's clip: every pixel of the box once */
        for (int k = 0; k < count; ++k) {
            if (deep)
                strips->put_indexed(paint, parts[k].x, parts[k].y, buffer + (size_t)(parts[k].y - strip.y) * (size_t)strip.w + (size_t)(parts[k].x - strip.x), strip.w,
                                    parts[k].w, parts[k].h, 1, false, (size_t)part.w * (size_t)rows);
            else
                dgx_vscreen8_region_to_screen16(paint->scr, parts[k].x - paint->ox, parts[k].y - paint->oy, paint->indexed, parts[k].x - strip.x,
                                                parts[k].y - strip.y, parts[k].w, parts[k].h, paint->lut, DGX_VSCREEN_EXACT);
        }
    }
    return false;
}

static const dgx_ui_nine_look_t nine_default = {.pieces = NULL, .fill = 2, .ink = 1};
const dgx_ui_look_t             dgx_ui_look_nine = {.draw = dgx_ui_draw_nine, .params = &nine_default, .fill = 2};
