/*
 * dgx_ui_text_look.c
 *
 *  The look of a text of one line, the default of a LABEL: the variant of the style it goes into, set in a field of
 *  DGX, cut with an ellipsis when none takes it whole. A value drawn again writes its field once and nothing else.
 *  Several lines, a scroll and letters appearing are the look of lines, dgx_ui_lines_look.c, named by a widget that
 *  wants them: a program of labels does not carry it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_ui_paint_priv.h"
#include "dgx_ui_text.h"
#include "dgx_ui_text_priv.h"

static const char ELLIPSIS[] = "...";

/* how many bytes of a text go into a field with an ellipsis after them, in whole letters; and how many letters */
static int cut_for(const dgx_font_t *font, const char *text, int field, int *letters)
{
    const int dots = dgx_ui_width_of(font, ELLIPSIS, (int)sizeof ELLIPSIS - 1);
    int       n = 0, count = 0;
    for (int k = 0; text[k]; k = dgx_ui_next_char(text, k)) {
        const int end = dgx_ui_next_char(text, k);
        if (end > DGX_UI_LINE || dgx_ui_width_of(font, text, end) + dots > field) break; /* past the line kept: not taken */
        n = end, ++count;
    }
    if (letters) *letters = count;
    return n;
}

/* the first variant the text goes into whole; else the one that shows the most letters, the earlier of equals; -1
   when there is none the look can draw. A handwritten one that does not go in even narrowed is cut by the box: it
   counts as showing none, taken only when nothing else can draw */
static int fit(const dgx_ui_text_style_t *st, const char *text, int field)
{
    int best = -1, most = -1;
    for (int v = 0; v < st->count; ++v) {
        const dgx_ui_font_variant_t *candidate = &st->variants[v];
        const dgx_font_t            *f         = candidate->font;
        if (candidate->hand) {
            if (candidate->hand->width(candidate, text, field) <= field) return v;
            if (most < 0) best = v, most = 0;
            continue;
        }
        if (!dgx_ui_variant_usable(f)) continue;
        if (dgx_ui_width_of(f, text, (int)strlen(text)) <= field && strlen(text) <= DGX_UI_LINE) return v;
        int letters;
        cut_for(f, text, field, &letters);
        if (letters > most) best = v, most = letters;
    }
    return best;
}

bool dgx_ui_draw_text(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    dgx_ui_node_t            *w   = dgx_ui_node(ui, i);
    dgx_ui_textbox_t         *box = w->widget->text;
    const dgx_ui_text_look_t *p   = look->params;
    const uint32_t ink = dgx_ui_color(paint, p->ink), paper = dgx_ui_color(paint, p->paper == DGX_UI_PAPER ? paint->paper : p->paper);
    const int      x = w->x - paint->ox, oy = paint->oy;
    if (!box || !box->style || !box->style->count || w->w <= 0) {
        if (paint->whole) dgx_ui_lay_paper(paint, p->paper, w->x, w->y, w->w, w->h);
        return false;
    }
    const dgx_ui_text_style_t *st   = box->style;
    const char                *text = box->buf ? box->buf : "";
    /* the variant: fitted again when the text or the width changed; remembered only by the painter's own pass, so
       that a composing does not hide from it that the variant, and with it the rows around the field, changed */
    int v = box->variant;
    if (w->marks & DGX_UI_MARK_FIT) v = fit(st, text, w->w);
    if (v < 0 || v >= st->count || !(st->variants[v].hand || dgx_ui_variant_usable(st->variants[v].font))) {
        if (paint->whole) dgx_ui_lay_paper(paint, p->paper, w->x, w->y, w->w, w->h);
        return false;
    }
    if (v != box->variant) paint->whole = true; /* another height: around it too */
    if (!paint->composing && paint->part + 1 >= paint->parts) box->variant = (uint8_t)v; /* stored by the last part only */
    if (st->variants[v].hand) return st->variants[v].hand->draw(look, paint, ui, i, now_ms, &st->variants[v]); /* the library's handwriting */
    const dgx_font_t *f = st->variants[v].font;
    /* the base line: the first variant's, its own when the first cannot be drawn; or the middle of the box */
    const int first_rows = dgx_ui_first_base(st);
    /* the base line: the first variant's, or the middle of the box for a smaller one */
    const int cell = f->y_bottom_max - f->y_offset_lowest;
    const int base = (st->flags & DGX_UI_TEXT_MIDDLE) ? w->y + (w->h - cell) / 2 - f->y_offset_lowest : w->y + (first_rows >= 0 ? first_rows : -f->y_offset_lowest);
    const int top = base + f->y_offset_lowest, bottom = base + f->y_bottom_max;
    if (paint->whole) {
        /* the rows of the box above and below the field: the field writes its own */
        if (top > w->y) dgx_ui_lay_paper(paint, p->paper, w->x, w->y, w->w, top - w->y);
        if (bottom < w->y + w->h) dgx_ui_lay_paper(paint, p->paper, w->x, bottom, w->w, w->y + w->h - bottom);
    }
    /* the text, cut with an ellipsis when it does not go in */
    char      line[DGX_UI_LINE + sizeof ELLIPSIS];
    const int n = (int)strlen(text);
    if (n <= DGX_UI_LINE && dgx_ui_width_of(f, text, n) <= w->w) {
        memcpy(line, text, (size_t)n + 1);
    } else {
        const int keep = cut_for(f, text, w->w, NULL);
        memcpy(line, text, (size_t)keep);
        memcpy(line + keep, ELLIPSIS, sizeof ELLIPSIS);
    }
    const dgx_font_context_t ctx = {.scr = paint->scr, .font = f, .color = ink, .bg = paper, .field = w->w, .align = st->align};
    dgx_ui_field_text(paint, &ctx, x, base - oy, line, p->paper);
    return false;
}

static const dgx_ui_text_look_t text_default = {.ink = 1, .paper = DGX_UI_PAPER};
const dgx_ui_look_t             dgx_ui_look_text = {.draw = dgx_ui_draw_text, .params = &text_default, .fill = -1};
