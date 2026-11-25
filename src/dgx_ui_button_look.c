/*
 * dgx_ui_button_look.c
 *
 *  The look of a button: a face over a shadow to its right and below; pressed, the face moves onto the shadow and
 *  paper shows above and left of it. The box is cut into the face, the shadow's strips and the paper's corners, so
 *  every pixel of it is written once in any state; the text, the variant 0 of the text box's style, is a field of
 *  DGX in the middle of the face, its paper the face. Focused, a frame of a point round the face in the colour
 *  `focused`, moving with the face; the inside drawn the same under the clip less the frame.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_ui_paint_priv.h"
#include "dgx_ui_text.h"
#include "dgx_ui_text_priv.h"

static void fill(dgx_ui_paint_t *paint, int x, int y, int w, int h, uint32_t c)
{
    if (w > 0 && h > 0) dgx_fill_rectangle(paint->scr, x - paint->ox, y - paint->oy, w, h, c);
}

/* the face, fx, fy, fw, fh: its rows above and below the text's field, then the field */
static void draw_face(dgx_ui_paint_t *paint, const dgx_ui_node_t *n, int fx, int fy, int fw, int fh, uint32_t face, uint32_t ink, int unit)
{
    const dgx_ui_textbox_t *box = n->widget->text;
    const dgx_font_t       *f   = box && box->style && box->style->count ? box->style->fonts[0].font : NULL;
    if (!dgx_ui_variant_usable(f) || fw <= 0) {
        fill(paint, fx, fy, fw, fh, face);
        return;
    }
    const int s    = dgx_ui_variant_scale(&box->style->fonts[0], unit); /* the pixels a pixel of the font takes */
    const int cell = (f->y_bottom_max - f->y_offset_lowest) * s, base = fy + (fh - cell) / 2 - f->y_offset_lowest * s;
    const int top = base + f->y_offset_lowest * s, bottom = base + f->y_bottom_max * s;
    fill(paint, fx, fy, fw, top - fy, face);
    fill(paint, fx, bottom, fw, fy + fh - bottom, face);
    /* the field kept to the rows of the face: in a button lower than the font it would write over the strips */
    int px, py, pw, ph;
    dgx_screen_clip_get(paint->scr, &px, &py, &pw, &ph);
    const int t = (top > fy ? top : fy) - paint->oy, b = (bottom < fy + fh ? bottom : fy + fh) - paint->oy;
    const int y0 = t > py ? t : py, y1 = b < py + ph ? b : py + ph;
    if (y1 > y0) {
        dgx_screen_clip(paint->scr, px, y0, pw, y1 - y0);
        const dgx_font_context_t ctx = {.scr = paint->scr, .font = f, .color = ink, .bg = face, .scale = s, .field = fw,
                                        .align = DGX_FONT_ALIGN_CENTER};
        dgx_font_draw(&ctx, fx - paint->ox, base - paint->oy, box->buf ? box->buf : "");
        dgx_screen_clip(paint->scr, px, py, pw, ph);
    }
}

void dgx_ui_draw_button_face(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, dgx_ui_face_t inside)
{
    const dgx_ui_node_t        *n = dgx_ui_node(ui, i);
    const dgx_ui_button_look_t *p = look->params;
    const unsigned              bits = n->state.bits;
    const bool                  pressed = bits & DGX_UI_PRESSED, off = bits & DGX_UI_DISABLED;
    const uint32_t shadow = dgx_ui_color(paint, p->shadow);
    const uint32_t face   = dgx_ui_color(paint, off ? p->disabled : pressed ? p->pressed : p->face);
    const uint32_t ink    = dgx_ui_color(paint, off ? p->ink_disabled : p->ink);
    int            d      = p->depth * ui->unit;
    const int      x = n->x, y = n->y, w = n->w, h = n->h;
    if (2 * d > w) d = w / 2;
    if (2 * d > h) d = h / 2;
    int fx, fy;
    if (pressed) {
        /* the face on the shadow: paper above and left */
        fx = x + d, fy = y + d;
        dgx_ui_lay_paper(paint, DGX_UI_PAPER, x, y, w, d);
        dgx_ui_lay_paper(paint, DGX_UI_PAPER, x, y + d, d, h - d);
    } else {
        /* the face up and left, the shadow right and below, paper in the two corners the shadow leaves */
        fx = x, fy = y;
        fill(paint, x + w - d, y + d, d, h - d, shadow);
        fill(paint, x + d, y + h - d, w - 2 * d, d, shadow);
        dgx_ui_lay_paper(paint, DGX_UI_PAPER, x + w - d, y, d, d);
        dgx_ui_lay_paper(paint, DGX_UI_PAPER, x, y + h - d, d, d);
    }
    /* the face, and round it the frame of the focus, which moves with it */
    const int      fw = w - d, fh = h - d;
    dgx_ui_frame_t frame;
    if (dgx_ui_frame_begin(paint, ui, i, dgx_ui_rect(fx, fy, fw, fh), p->focused, &frame)) inside(paint, n, fx, fy, fw, fh, face, ink, ui->unit);
    dgx_ui_frame_end(paint, ui, &frame);
}

bool dgx_ui_draw_button(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    dgx_ui_draw_button_face(look, paint, ui, i, draw_face);
    return false;
}

static const dgx_ui_button_look_t button_default = {
    .face = 2, .pressed = 3, .focused = 1, .disabled = 0, .ink = 1, .ink_disabled = 2, .shadow = 0, .depth = 1};
const dgx_ui_look_t dgx_ui_look_button = {.draw = dgx_ui_draw_button, .params = &button_default, .fill = -1};
