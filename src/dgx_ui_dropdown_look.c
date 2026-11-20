/*
 * dgx_ui_dropdown_look.c
 *
 *  The look of a dropdown: a flat face of the colour of its state, the option chosen (the line value - min of its text
 *  box, as a list's) on the left in the middle of the face, and an arrow pointing down on the right. Every pixel of
 *  the box once, in any state. Focused, a frame of a point round the box in the colour `focused`, the inside drawn the
 *  same under the clip less the frame.
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

/* the line k of a text of lines ended by '\n', into line (at most DGX_UI_LINE bytes); empty before the first and past
   the last */
static void line_of(const char *text, int k, char *line)
{
    if (k < 0) {
        line[0] = 0;
        return;
    }
    while (k > 0 && *text) {
        const char *end = strchr(text, '\n');
        if (!end) {
            text += strlen(text);
            break;
        }
        text = end + 1, --k;
    }
    const char *end = k > 0 ? text : strchr(text, '\n');
    int         len = k > 0 ? 0 : end ? (int)(end - text) : (int)strlen(text);
    if (len > DGX_UI_LINE) len = DGX_UI_LINE;
    memcpy(line, text, (size_t)len), line[len] = 0;
}

static void draw_inside(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i)
{
    const dgx_ui_node_t          *n = dgx_ui_node(ui, i);
    const dgx_ui_widget_t        *w = n->widget;
    const dgx_ui_dropdown_look_t *p = look->params;
    static bool                   told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
    dgx_ui_tell_no_place(i, w, "a dropdown", &told);
    const unsigned                bits = n->state.bits;
    const bool                    off  = bits & DGX_UI_DISABLED;
    const uint32_t face = dgx_ui_color(paint, off ? p->disabled : (bits & DGX_UI_PRESSED) ? p->pressed : p->face);
    const uint32_t ink = dgx_ui_color(paint, off ? p->text_disabled : p->text), arrow = dgx_ui_color(paint, off ? p->text_disabled : p->arrow);
    const dgx_ui_textbox_t *box = w->text;
    dgx_font_t             *f   = box && box->style && box->style->count ? box->style->variants[0].font : NULL;
    /* the arrow's square on the right, as wide as the box is high (at most half of it) */
    const int aw = n->h < n->w / 2 ? n->h : n->w / 2, tw = n->w - aw;
    if (!dgx_ui_variant_usable(f) || n->h <= 0) {
        fill(paint, n->x, n->y, tw, n->h, face);
    } else {
        /* the option chosen, in the middle rows of the face: the rows above and below it, then its field */
        const int cell = f->yBottomMax - f->yOffsetLowest, base = n->y + (n->h - cell) / 2 - f->yOffsetLowest;
        const int top = base + f->yOffsetLowest, bottom = base + f->yBottomMax;
        int       px, py, pw, ph;
        dgx_screen_clip_get(paint->scr, &px, &py, &pw, &ph);
        fill(paint, n->x, n->y, tw, top - n->y, face);
        fill(paint, n->x, bottom, tw, n->y + n->h - bottom, face);
        const int pad = cell / 4 > 2 * ui->unit ? cell / 4 : 2 * ui->unit; /* the text away from the left edge, past the focus's frame */
        char      line[DGX_UI_LINE + 1];
        line_of(box->buf ? box->buf : "", dgx_ui_value_of(n) - w->min, line);
        const int y0 = (top > n->y ? top : n->y) - paint->oy, y1 = (bottom < n->y + n->h ? bottom : n->y + n->h) - paint->oy;
        const int cy0 = y0 > py ? y0 : py, cy1 = y1 < py + ph ? y1 : py + ph;
        if (cy1 > cy0) {
            dgx_screen_clip(paint->scr, px, cy0, pw, cy1 - cy0);
            fill(paint, n->x, top, pad < tw ? pad : tw, bottom - top, face);
            if (tw > pad) {
                const dgx_font_context_t ctx = {.scr = paint->scr, .font = f, .color = ink, .bg = face, .field = tw - pad, .align = DGX_FONT_ALIGN_LEFT};
                dgx_font_draw(&ctx, n->x + pad - paint->ox, base - paint->oy, line);
            }
            dgx_screen_clip(paint->scr, px, py, pw, ph);
        }
    }
    /* the arrow: a triangle pointing down in the middle of its square, row by row, the face around it */
    const int ax = n->x + tw, half = aw / 4, cx = ax + aw / 2, ty = n->y + (n->h - half) / 2;
    for (int y = n->y; y < n->y + n->h; ++y) {
        const int r = y - ty, hw = half - r; /* the half width of the triangle at this row */
        if (r < 0 || r >= half || hw <= 0) {
            fill(paint, ax, y, aw, 1, face);
            continue;
        }
        fill(paint, ax, y, cx - hw - ax, 1, face);
        fill(paint, cx - hw, y, 2 * hw, 1, arrow);
        fill(paint, cx + hw, y, ax + aw - cx - hw, 1, face);
    }
}

bool dgx_ui_draw_dropdown(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    dgx_ui_frame_t       frame; /* the focus: a frame round the box */
    if (dgx_ui_frame_begin(paint, ui, i, dgx_ui_rect(n->x, n->y, n->w, n->h), ((const dgx_ui_dropdown_look_t *)look->params)->focused, &frame))
        draw_inside(look, paint, ui, i);
    dgx_ui_frame_end(paint, ui, &frame);
    return false;
}

static const dgx_ui_dropdown_look_t dropdown_default = {.face = 2, .pressed = 3, .focused = 1, .disabled = 0, .text = 1, .text_disabled = 2, .arrow = 1};
const dgx_ui_look_t dgx_ui_look_dropdown = {.draw = dgx_ui_draw_dropdown, .params = &dropdown_default, .fill = -1};
