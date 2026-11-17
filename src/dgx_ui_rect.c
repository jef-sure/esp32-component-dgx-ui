/*
 * dgx_ui_rect.c
 *
 *  Rectangles of a screen: their common part, and what of a box lies outside a cut.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui.h"

bool dgx_ui_rect_intersect(const dgx_ui_rect_t *a, const dgx_ui_rect_t *b, dgx_ui_rect_t *out)
{
    /* in int: x + w of two int16_t does not fit in one */
    const int left = a->x > b->x ? a->x : b->x, top = a->y > b->y ? a->y : b->y;
    const int ar = a->x + a->w, br = b->x + b->w, ab = a->y + a->h, bb = b->y + b->h;
    const int right = ar < br ? ar : br, bottom = ab < bb ? ab : bb;
    if (dgx_ui_rect_empty(a) || dgx_ui_rect_empty(b) || left >= right || top >= bottom) {
        *out = dgx_ui_rect(0, 0, 0, 0);
        return false;
    }
    *out = dgx_ui_rect(left, top, right - left, bottom - top);
    return true;
}

int dgx_ui_rect_subtract(const dgx_ui_rect_t *box, const dgx_ui_rect_t *cut, dgx_ui_rect_t out[4])
{
    if (dgx_ui_rect_empty(box)) return 0;
    dgx_ui_rect_t in;
    if (!dgx_ui_rect_intersect(box, cut, &in)) {
        out[0] = *box;
        return 1;
    }
    const int box_right = box->x + box->w, box_bottom = box->y + box->h;
    const int in_right = in.x + in.w, in_bottom = in.y + in.h;
    int       n = 0;
    if (in.y > box->y) out[n++] = dgx_ui_rect(box->x, box->y, box->w, in.y - box->y);
    if (in_bottom < box_bottom) out[n++] = dgx_ui_rect(box->x, in_bottom, box->w, box_bottom - in_bottom);
    if (in.x > box->x) out[n++] = dgx_ui_rect(box->x, in.y, in.x - box->x, in.h);
    if (in_right < box_right) out[n++] = dgx_ui_rect(in_right, in.y, box_right - in_right, in.h);
    return n;
}
