/*
 * dgx_ui_input_look.c
 *
 *  The look of a field typed into: its text, the variant 0 of its style, at the top of the field or in its middle
 *  (DGX_UI_TEXT_MIDDLE), and when it is focused a caret where the next letter goes; a secret field a star for each
 *  letter. The row of the text is cut into the field up to the caret, the caret, and the empty field after
 *  it, so every pixel is written once; a text longer than the field shows its end, the caret at the right edge.
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

/* the field drawn with a text: its own, or the stars of a secret one */
static bool draw(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, const char *text)
{
    const dgx_ui_node_t       *n   = dgx_ui_node(ui, i);
    const dgx_ui_input_look_t *p   = look->params;
    const dgx_ui_textbox_t    *box = n->widget->text;
    const uint32_t paper = dgx_ui_color(paint, p->paper == DGX_UI_PAPER ? paint->paper : p->paper), ink = dgx_ui_color(paint, p->ink);
    const dgx_font_t *f = box && box->style && box->style->count ? box->style->variants[0].font : NULL;
    if (!dgx_ui_variant_usable(f) || n->w <= 0) {
        dgx_ui_lay_paper(paint, p->paper, n->x, n->y, n->w, n->h);
        return false;
    }
    const bool  caret = dgx_ui_bits_shown(ui, n) & DGX_UI_FOCUSED;
    const int   cw    = caret ? (ui->unit > 1 ? ui->unit : 1) : 0;
    const int   s    = dgx_ui_variant_scale(&box->style->variants[0], ui->unit); /* the pixels a pixel of the font takes */
    const int   cell = (f->y_bottom_max - f->y_offset_lowest) * s;
    const int   base = (box->style->flags & DGX_UI_TEXT_MIDDLE) ? n->y + (n->h - cell) / 2 - f->y_offset_lowest * s : n->y - f->y_offset_lowest * s;
    const int   top = base + f->y_offset_lowest * s, bottom = base + f->y_bottom_max * s;
    /* the rows above and below the text */
    dgx_ui_lay_paper(paint, p->paper, n->x, n->y, n->w, top - n->y);
    dgx_ui_lay_paper(paint, p->paper, n->x, bottom, n->w, n->y + n->h - bottom);
    /* the text up to the caret, its end when it is longer than the field */
    const int room = n->w - cw, tw = dgx_ui_width_of(f, text, (int)strlen(text)) * s;
    const int field = tw < room ? tw : room;
    int       px, py, pw, ph;
    dgx_screen_clip_get(paint->scr, &px, &py, &pw, &ph);
    const int t = (top > n->y ? top : n->y) - paint->oy, b = (bottom < n->y + n->h ? bottom : n->y + n->h) - paint->oy;
    const int y0 = t > py ? t : py, y1 = b < py + ph ? b : py + ph;
    if (y1 <= y0) return false;
    dgx_screen_clip(paint->scr, px, y0, pw, y1 - y0);
    if (field > 0) {
        const dgx_font_context_t ctx = {.scr = paint->scr, .font = f, .color = ink, .bg = paper, .scale = s, .field = field,
                                        .align = tw > room ? DGX_FONT_ALIGN_RIGHT : DGX_FONT_ALIGN_LEFT};
        dgx_ui_field_text(paint, &ctx, n->x - paint->ox, base - paint->oy, text, p->paper);
    }
    fill(paint, n->x + field, top, cw, bottom - top, dgx_ui_color(paint, p->caret));
    dgx_ui_lay_paper(paint, p->paper, n->x + field + cw, top, n->w - field - cw, bottom - top);
    dgx_screen_clip(paint->scr, px, py, pw, ph);
    return false;
}

/* as many stars as letters, at most a line of them: their room on the stack only for a secret field, never inlined
   into the draw of a plain one */
__attribute__((noinline)) static bool draw_secret(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, const char *text)
{
    char stars[DGX_UI_LINE + 1];
    int  k = 0;
    for (int at = 0; text[at] && k < DGX_UI_LINE; at = dgx_ui_next_char(text, at)) stars[k++] = '*';
    stars[k] = 0;
    return draw(look, paint, ui, i, stars);
}

bool dgx_ui_draw_input(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_textbox_t *box  = dgx_ui_node(ui, i)->widget->text;
    const char             *text = box && box->buf ? box->buf : "";
    return ((const dgx_ui_input_look_t *)look->params)->secret ? draw_secret(look, paint, ui, i, text) : draw(look, paint, ui, i, text);
}

static const dgx_ui_input_look_t input_default = {.ink = 1, .paper = DGX_UI_PAPER, .caret = 1};
const dgx_ui_look_t              dgx_ui_look_input = {.draw = dgx_ui_draw_input, .params = &input_default, .fill = -1};
