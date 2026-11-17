/*
 * dgx_ui_input_look.c
 *
 *  The look of a field typed into: its text, the variant 0 of its style, and when it is focused a caret where the
 *  next letter goes. The row of the text is cut into the field up to the caret, the caret, and the empty field after
 *  it, so every pixel is written once; a text longer than the field shows its end, the caret at the right edge.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_ui_text_priv.h"

static void fill(dgx_ui_paint_t *paint, int x, int y, int w, int h, uint32_t c)
{
    if (w > 0 && h > 0) dgx_fill_rectangle(paint->scr, x - paint->ox, y - paint->oy, w, h, c);
}

static bool input_draw(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t       *n   = dgx_ui_node(ui, i);
    const dgx_ui_input_look_t *p   = look->params;
    const dgx_ui_textbox_t    *box = n->widget->text;
    const uint32_t paper = dgx_ui_color(paint, p->paper == DGX_UI_PAPER ? paint->paper : p->paper), ink = dgx_ui_color(paint, p->ink);
    dgx_font_t *f = box && box->style && box->style->count ? box->style->variants[0].font : NULL;
    if (!dgx_ui_variant_usable(f) || n->w <= 0) {
        fill(paint, n->x, n->y, n->w, n->h, paper);
        return false;
    }
    const char *text  = box->buf ? box->buf : "";
    const bool  caret = n->state_value.bits & DGX_UI_FOCUSED;
    const int   cw    = caret ? (ui->unit > 1 ? ui->unit : 1) : 0;
    const int   base = n->y - f->yOffsetLowest, top = base + f->yOffsetLowest, bottom = base + f->yBottomMax;
    /* the rows above and below the text */
    fill(paint, n->x, n->y, n->w, top - n->y, paper);
    fill(paint, n->x, bottom, n->w, n->y + n->h - bottom, paper);
    /* the text up to the caret, its end when it is longer than the field */
    const int room = n->w - cw, tw = dgx_ui_width_of(f, text, (int)strlen(text));
    const int field = tw < room ? tw : room;
    int       px, py, pw, ph;
    dgx_screen_clip_get(paint->scr, &px, &py, &pw, &ph);
    const int t = (top > n->y ? top : n->y) - paint->oy, b = (bottom < n->y + n->h ? bottom : n->y + n->h) - paint->oy;
    const int y0 = t > py ? t : py, y1 = b < py + ph ? b : py + ph;
    if (y1 <= y0) return false;
    dgx_screen_clip(paint->scr, px, y0, pw, y1 - y0);
    if (field > 0) {
        const dgx_font_context_t ctx = {.scr = paint->scr, .font = f, .color = ink, .bg = paper, .field = field, .align = tw > room ? DGX_FONT_ALIGN_RIGHT : DGX_FONT_ALIGN_LEFT};
        dgx_font_draw(&ctx, n->x - paint->ox, base - paint->oy, text);
    }
    fill(paint, n->x + field, top, cw, bottom - top, dgx_ui_color(paint, p->caret));
    fill(paint, n->x + field + cw, top, n->w - field - cw, bottom - top, paper);
    dgx_screen_clip(paint->scr, px, py, pw, ph);
    return false;
}

static const dgx_ui_input_look_t input_default = {.ink = 1, .paper = DGX_UI_PAPER, .caret = 3};
const dgx_ui_look_t              dgx_ui_look_input = {.draw = input_draw, .params = &input_default, .fill = -1};
