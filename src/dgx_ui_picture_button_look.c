/*
 * dgx_ui_picture_button_look.c
 *
 *  The look of a picture button: the button's face over its shadow, pressed onto it, framed with the focus
 *  (dgx_ui_draw_button_face()), and in the middle of the face a picture of 1 bit in place of a text: a button's one
 *  picture; a switch's first while it is off and its second while it is on, as the switch look shows it; a radio's
 *  first while it is not chosen and its second while it is. The picture's ones in the ink of the state, its zeros the
 *  face, the face laid round it: every pixel once. A program that names it not carries none of it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_image.h"
#include "dgx_ui_paint_priv.h"
#include "esp_log.h"

static const char *TAG = "dgx_ui";

static void fill(dgx_ui_paint_t *paint, int x, int y, int w, int h, uint32_t c)
{
    if (w > 0 && h > 0) dgx_fill_rectangle(paint->scr, x - paint->ox, y - paint->oy, w, h, c);
}

/* the picture a widget shows: a button its one; a switch or a radio the second while it is on or chosen, else the
   first; NULL none */
static const dgx_ui_picture_t *shown_picture(const dgx_ui_node_t *n)
{
    const dgx_ui_widget_t *w = n->widget;
    if (!w->picture) return NULL;
    if (w->kind != DGX_UI_SWITCH && w->kind != DGX_UI_RADIO) return w->picture;
    return &w->picture[dgx_ui_shown(n) != 0];
}

/* the face's inside: the picture in the middle of the face, cut to it, the face laid round it */
static void picture_face(dgx_ui_paint_t *paint, const dgx_ui_node_t *n, int fx, int fy, int fw, int fh, uint32_t face, uint32_t ink, int unit)
{
    const dgx_ui_picture_t *pic = shown_picture(n);
    if (!pic || !pic->image || pic->image->format != DGX_IMAGE_1BPP) {
        static bool told; /* once for the program, every tree and task */
        if (pic && !told) {
            told = true;
            ESP_LOGW(TAG, "a picture button (tag %d) draws pictures of 1 bit; its face is plain", n->widget->tag);
        }
        fill(paint, fx, fy, fw, fh, face);
        return;
    }
    const dgx_image_t  *im   = pic->image;
    const int           by   = pic->scale * unit, k = !pic->scale ? 1 : by > 255 ? 255 : by; /* as dgx_ui_picture_scale() */
    const dgx_ui_rect_t area = dgx_ui_rect(fx, fy, fw, fh);
    const dgx_ui_rect_t hole = dgx_ui_rect(fx + (fw - im->width * k) / 2, fy + (fh - im->height * k) / 2, im->width * k, im->height * k);
    dgx_ui_rect_t       parts[4], cut;
    const int           count = dgx_ui_rect_subtract(&area, &hole, parts);
    for (int k = 0; k < count; ++k) fill(paint, parts[k].x, parts[k].y, parts[k].w, parts[k].h, face);
    if (!dgx_ui_rect_intersect(&hole, &area, &cut)) return;
    /* the picture cut to the face: the clip narrowed to the part of it, and given back */
    int px, py, pw, ph;
    dgx_screen_clip_get(paint->scr, &px, &py, &pw, &ph);
    const int x0 = cut.x - paint->ox > px ? cut.x - paint->ox : px, y0 = cut.y - paint->oy > py ? cut.y - paint->oy : py;
    const int x1 = cut.x + cut.w - paint->ox < px + pw ? cut.x + cut.w - paint->ox : px + pw;
    const int y1 = cut.y + cut.h - paint->oy < py + ph ? cut.y + cut.h - paint->oy : py + ph;
    if (x1 > x0 && y1 > y0) {
        dgx_screen_clip(paint->scr, x0, y0, x1 - x0, y1 - y0);
        const dgx_image_blit_t how = {.flags = DGX_IMAGE_OPAQUE, .color = ink, .bg = face, .scale = (uint8_t)k};
        dgx_image_blit(paint->scr, hole.x - paint->ox + im->origin_x * k, hole.y - paint->oy + im->origin_y * k, im, &how);
        dgx_screen_clip(paint->scr, px, py, pw, ph);
    }
}

bool dgx_ui_draw_picture_button(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    dgx_ui_draw_button_face(look, paint, ui, i, picture_face);
    return false;
}

static const dgx_ui_button_look_t picture_button_default = {
    .face = 2, .pressed = 3, .focused = 1, .disabled = 0, .text = 1, .text_disabled = 2, .shadow = 0, .depth = 1};
const dgx_ui_look_t dgx_ui_look_picture_button = {.draw = dgx_ui_draw_picture_button, .params = &picture_button_default, .fill = -1};
