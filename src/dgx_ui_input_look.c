/*
 * dgx_ui_input_look.c
 *
 *  The look of a field typed into: its text, the variant 0 of its style, at the top of the field or in its middle
 *  (DGX_UI_STYLE_MIDDLE), and when it is focused a caret where the next letter goes, shown and hidden every half
 *  period (blink_ms); a secret field a star for each letter. The row of the text is cut into the field up to the caret,
 *  the caret, and the empty field after it, so every pixel is written once; a text longer than the field shows its
 *  end, the caret at the right edge. The caret's phase counts from the text set or the focus come, kept in the text
 *  box; when only time changed it, the caret's rectangle alone is drawn, and the loop woken for the next.
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
    if (w > 0 && h > 0) dgx_ui_fill(paint, x, y, w, h, c);
}

/* the caret's rectangle: of its colour, or the paper */
__attribute__((noinline)) static void caret_cell(dgx_ui_paint_t *paint, const dgx_ui_input_look_t *p, int x, int top, int w, int h, bool shown)
{
    if (shown) fill(paint, x, top, w, h, dgx_ui_color(paint, p->caret));
    else dgx_ui_lay_paper(paint, p->paper, x, top, w, h);
}

/* the field drawn with a text: its own, or the stars of a secret one */
static bool draw(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, const char *text, uint32_t now_ms)
{
    const dgx_ui_node_t       *n   = dgx_ui_node(ui, i);
    const dgx_ui_input_look_t *p   = look->params;
    dgx_ui_textbox_t          *box = n->widget->text;
    const uint32_t paper = dgx_ui_color(paint, p->paper == DGX_UI_PAPER ? paint->paper : p->paper), ink = dgx_ui_color(paint, p->ink);
    const dgx_font_t *f = box && box->style && box->style->count ? box->style->fonts[0].font : NULL;
    if (!dgx_ui_variant_usable(f) || n->w <= 0) {
        dgx_ui_lay_paper(paint, p->paper, n->x, n->y, n->w, n->h);
        return false;
    }
    const uint8_t bits  = dgx_ui_bits_shown(ui, n);
    const bool    caret = bits & DGX_UI_FOCUSED;
    const int   cw    = caret ? (ui->unit > 1 ? ui->unit : 1) : 0;
    /* the caret's phase: from the text set or the focus come, shown the first half of each period; kept, but by a
       composing. Only time changed: the caret alone, when its phase did */
    const uint32_t half    = caret ? p->blink_ms : 0;
    const uint32_t since   = (box->flags & DGX_UI_TEXTBOX_START) || (caret && !(n->state_drawn.bits & DGX_UI_FOCUSED)) ? now_ms : box->since_ms;
    const uint32_t gone    = now_ms - since, halves = half ? gone / half : 0;
    const bool     shown   = caret && !(halves & 1);
    const bool     alone   = !paint->whole && !(n->marks & (DGX_UI_MARK_CHANGED | DGX_UI_MARK_FIT)) && n->state_drawn.bits == bits;
    const bool     was     = box->flags & DGX_UI_TEXTBOX_CARET;
    if (!paint->composing) box->since_ms = since, box->flags = (uint8_t)((box->flags & ~(DGX_UI_TEXTBOX_START | DGX_UI_TEXTBOX_CARET)) | (shown ? DGX_UI_TEXTBOX_CARET : 0));
    if (half) dgx_ui_wake_in(paint, half * (halves + 1) - gone);
    const int   s    = dgx_ui_variant_scale(&box->style->fonts[0], ui->unit); /* the pixels a pixel of the font takes */
    const int   cell = (f->y_bottom_max - f->y_offset_lowest) * s;
    const int   base = (box->style->flags & DGX_UI_STYLE_MIDDLE) ? n->y + (n->h - cell) / 2 - f->y_offset_lowest * s : n->y - f->y_offset_lowest * s;
    const int   top = base + f->y_offset_lowest * s, bottom = base + f->y_bottom_max * s;
    const int room = n->w - cw, tw = dgx_ui_width_of(f, text, (int)strlen(text)) * s;
    const int field = tw < room ? tw : room;
    if (alone) { /* only time: the caret's rectangle, when it came or went */
        if (shown != was) caret_cell(paint, p, n->x + field, top, cw, bottom - top, shown);
        return half != 0;
    }
    /* the rows above and below the text */
    dgx_ui_lay_paper(paint, p->paper, n->x, n->y, n->w, top - n->y);
    dgx_ui_lay_paper(paint, p->paper, n->x, bottom, n->w, n->y + n->h - bottom);
    /* the text up to the caret, its end when it is longer than the field */
    int       px, py, pw, ph;
    dgx_screen_clip_get(paint->scr, &px, &py, &pw, &ph);
    const int t = (top > n->y ? top : n->y) - paint->oy, b = (bottom < n->y + n->h ? bottom : n->y + n->h) - paint->oy;
    const int y0 = t > py ? t : py, y1 = b < py + ph ? b : py + ph;
    if (y1 <= y0) return half != 0;
    dgx_screen_clip(paint->scr, px, y0, pw, y1 - y0);
    if (field > 0) {
        const dgx_font_context_t ctx = {.scr = paint->scr, .font = f, .color = ink, .bg = paper, .scale = s, .field = field,
                                        .align = tw > room ? DGX_FONT_ALIGN_RIGHT : DGX_FONT_ALIGN_LEFT};
        dgx_ui_field_text(paint, &ctx, n->x - paint->ox, base - paint->oy, text, p->paper);
    }
    caret_cell(paint, p, n->x + field, top, cw, bottom - top, shown);
    dgx_ui_lay_paper(paint, p->paper, n->x + field + cw, top, n->w - field - cw, bottom - top);
    dgx_screen_clip(paint->scr, px, py, pw, ph);
    return half != 0;
}

/* as many stars as letters, at most a line of them: their room on the stack only for a secret field, never inlined
   into the draw of a plain one */
__attribute__((noinline)) static bool draw_secret(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, const char *text, uint32_t now_ms)
{
    char stars[DGX_UI_LINE + 1];
    int  k = 0;
    for (int at = 0; text[at] && k < DGX_UI_LINE; at = dgx_ui_next_char(text, at)) stars[k++] = '*';
    stars[k] = 0;
    return draw(look, paint, ui, i, stars, now_ms);
}

bool dgx_ui_draw_input(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_textbox_t *box  = dgx_ui_node(ui, i)->widget->text;
    const char             *text = box && box->buf ? box->buf : "";
    return ((const dgx_ui_input_look_t *)look->params)->secret ? draw_secret(look, paint, ui, i, text, now_ms) : draw(look, paint, ui, i, text, now_ms);
}

static const dgx_ui_input_look_t input_default = {.ink = 1, .paper = DGX_UI_PAPER, .caret = 1, .blink_ms = 500};
const dgx_ui_look_t              dgx_ui_look_input = {.draw = dgx_ui_draw_input, .params = &input_default, .fill = DGX_UI_PAPER};
