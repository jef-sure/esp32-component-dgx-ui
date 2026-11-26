/*
 * dgx_ui_lines_look.c
 *
 *  The look of a text of lines, dgx_ui_look_lines, for a widget that names it: the text laid into lines word by word
 *  each time it is drawn (a word wider than a line by
 *  its letters), the variant of the style fitted, each line shown a field of DGX under a clip of its own row, the
 *  paper between the fields only when drawn whole. An important text too long scrolls a line every scroll_ms; one
 *  that is not is cut, its last line seen ending in an ellipsis. Letters appear one by one when asked, each where it
 *  will stand, a pass drawing only the rows whose letters came. Fonts of bitmaps and dots; handwriting is dgx_ui_look_hand_lines (dgx_ui_hand_lines.c), which lays its
 *  lines by the same source (dgx_ui_lines_layout.h) and draws a bitmap variant of its ladder by dgx_ui_draw_lines_in().
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_ui_paint_priv.h"
#include "dgx_ui_text.h"
#include "dgx_ui_text_priv.h"

enum { LINE = DGX_UI_LINE, MAX_LINES = 1000 };
static const char ELLIPSIS[] = "...";

#define next_char dgx_ui_next_char
#define width_of  dgx_ui_width_of

/* the shared laying of lines, by the fonts' own measure; a variant of no bitmap or dot font is not drawn here */
#define LINES_MEASURE(v, s, n, unit) (width_of((v)->font, s, n) * dgx_ui_variant_scale(v, unit))
#define LINES_HEIGHT(v, unit)        (dgx_ui_variant_usable((v)->font) ? (v)->font->y_advance * dgx_ui_variant_scale(v, unit) : 0)
#include "dgx_ui_lines_layout.h"

/* the clip of the screen cut to rows top .. bottom of the tree's screen; false when nothing of them is left */
static bool clip_rows(dgx_ui_paint_t *paint, const int part[4], int top, int bottom)
{
    const int t = top - paint->oy, b = bottom - paint->oy;
    const int y0 = part[1] > t ? part[1] : t, y1 = part[1] + part[3] < b ? part[1] + part[3] : b;
    if (y1 <= y0) return false;
    dgx_screen_clip(paint->scr, part[0], y0, part[2], y1 - y0);
    return true;
}

/* the look's drawing; chosen: the variant a look of handwritten lines fitted, -1 fitted here; first_rows: its first
   row's base line under the box's top, -1 the first bitmap variant's */
static bool draw_lines(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms, int chosen, int first_rows)
{
    dgx_ui_node_t            *w   = dgx_ui_node_rw(ui, i);
    dgx_ui_textbox_t         *box = w->widget->text;
    const dgx_ui_text_look_t *p   = look->params;
    const uint32_t ink = dgx_ui_color(paint, p->ink), paper = dgx_ui_color(paint, p->paper == DGX_UI_PAPER ? paint->paper : p->paper);
    const int      x = w->x - paint->ox;
    if (!box || !box->style || !box->style->count || w->w <= 0) {
        if (paint->whole) dgx_ui_lay_paper(paint, p->paper, w->x, w->y, w->w, w->h);
        return false;
    }
    const dgx_ui_text_style_t *st   = box->style;
    const char                *text = box->buf ? box->buf : "";
    const int                  len  = (int)strlen(text);

    /* what is remembered is stored only by the last part of the painter's own pass: the parts before it must find
       the same as it, and a composing stores nothing */
    const bool commit = !paint->composing && paint->part + 1 >= paint->parts;
    /* the variant: fitted again when the text or the width changed */
    int v = box->variant;
    if (chosen >= 0) v = chosen;
    else if (w->marks & DGX_UI_MARK_FIT) v = fit_lines(st, text, len, w->w, w->h, ui->unit);
    if (v < 0 || v >= st->count || !dgx_ui_variant_usable(st->fonts[v].font)) {
        if (paint->whole) dgx_ui_lay_paper(paint, p->paper, w->x, w->y, w->w, w->h);
        return false;
    }
    if (v != box->variant) paint->whole = true; /* another height: the rows around the fields too */
    const uint32_t since = (box->flags & DGX_UI_TEXTBOX_START) ? now_ms : box->since_ms;
    const dgx_ui_font_variant_t *var = &st->fonts[v];
    const dgx_font_t            *f = var->font, *f0 = dgx_ui_variant_usable(st->fonts[0].font) ? st->fonts[0].font : f;
    /* the pixels a pixel of its font takes, and of the first variant's whose base line it stands on */
    const int scale = dgx_ui_variant_scale(var, ui->unit), scale0 = f0 == f ? scale : dgx_ui_variant_scale(&st->fonts[0], ui->unit);
    const int      lh = f->y_advance * scale, cell_top = f->y_offset_lowest * scale, cell = (f->y_bottom_max - f->y_offset_lowest) * scale;
    const int      rows = w->h / lh > 0 ? w->h / lh : 1;

    /* the lines and letters of the text: counted again only when the text, the width or the variant changed */
    int total, all_letters;
    if (!(box->flags & DGX_UI_TEXTBOX_COUNTED) || (w->marks & (DGX_UI_MARK_CHANGED | DGX_UI_MARK_FIT)) || v != box->variant) {
        total = count_lines(text, len, var, w->w, MAX_LINES, MAX_LINES, &all_letters, ui->unit);
    } else {
        total = box->total_lines, all_letters = box->total_letters;
    }
    /* the letters come so far and the first line shown; once all came, and once the scroll reached its end, no
       time is counted for them: a text drawn again days later is as it was */
    const bool     important = st->flags & DGX_UI_STYLE_IMPORTANT;
    const int      last_top  = total > rows ? total - rows : 0;
    const uint32_t elapsed   = now_ms - since; /* by difference: now_ms runs over in 49.7 days */
    const bool     came      = !box->letter_ms || (box->flags & DGX_UI_TEXTBOX_CAME);
    /* the letters' time, none when the program showed them whole (dgx_ui_text_whole()): the scroll counts from then */
    const uint32_t appear_ms = box->letter_ms && !(box->flags & DGX_UI_TEXTBOX_WHOLE) ? (uint32_t)all_letters * box->letter_ms : 0;
    int            come      = came ? all_letters : (int)(elapsed / box->letter_ms);
    if (come > all_letters) come = all_letters;
    int  top  = box->top;
    bool more = come < all_letters;
    if (important && st->scroll_ms && total > rows) {
        if (box->flags & DGX_UI_TEXTBOX_ENDED) {
            top = last_top;
        } else {
            /* a line up every scroll_ms once all its letters came, the first shown so long too */
            top = come < all_letters || elapsed < appear_ms ? 0 : (int)((elapsed - appear_ms) / st->scroll_ms);
            if (top > last_top) top = last_top;
            more |= top < last_top;
        }
    }
    if (top > last_top) top = last_top;
    /* called again for time only: drawn when the line on top moved or more letters came */
    const bool timed_only = !paint->whole && !(w->marks & (DGX_UI_MARK_CHANGED | DGX_UI_MARK_FIT)) && (w->marks & DGX_UI_MARK_TIME);
    const bool same       = top == box->top && come == box->letters_drawn;
    /* what the last pass drew, before this one keeps its own: a row is drawn again only when its letters shown, the
       line on top or the text changed, or the widget is drawn whole (another variant is); the block in the middle
       moves only when the number of the rows shown changed */
    const int  come_before  = box->letters_drawn;
    const bool rows_anew    = (w->marks & (DGX_UI_MARK_CHANGED | DGX_UI_MARK_FIT)) || top != box->top;
    const int  shown_before = (box->flags & DGX_UI_TEXTBOX_COUNTED) ? (box->total_lines - box->top < rows ? box->total_lines - box->top : rows) : -1;
    if (commit) {
        box->variant = (uint8_t)v, box->top = (uint16_t)top, box->letters_drawn = (uint16_t)come;
        box->total_lines = (uint16_t)total, box->total_letters = (uint16_t)(all_letters > 0xFFFF ? 0xFFFF : all_letters);
        box->since_ms    = since;
        box->flags       = (uint8_t)((box->flags & ~DGX_UI_TEXTBOX_START) | DGX_UI_TEXTBOX_COUNTED);
        if (come >= all_letters) box->flags |= DGX_UI_TEXTBOX_CAME;
        if (important && st->scroll_ms && total > rows && top >= last_top) box->flags |= DGX_UI_TEXTBOX_ENDED;
    }
    /* its next moment: the next letter, or once all came the next line up; by difference, as above */
    if (more) dgx_ui_wake_in(paint, come < all_letters ? (uint32_t)(come + 1) * box->letter_ms - elapsed : appear_ms + (uint32_t)(top + 1) * st->scroll_ms - elapsed);
    if (timed_only && same) return more;
    const bool cut   = !important && total - top > rows; /* lines follow the last row: it ends in an ellipsis */
    const int  shown = total - top < rows ? total - top : rows;

    /* the base line of the first row: the first variant's, or the block of the rows shown in the middle */
    int base0 = w->y + (first_rows >= 0 ? first_rows : -f0->y_offset_lowest * scale0);
    if (st->flags & DGX_UI_STYLE_MIDDLE) {
        const int block = shown > 0 ? (shown - 1) * lh + cell : cell;
        base0           = w->y + (w->h - block) / 2 - cell_top;
        if (rows > 1 && shown != shown_before) paint->whole = true; /* the block moved: the rows around it too */
    }
    int part[4];
    dgx_screen_clip_get(paint->scr, &part[0], &part[1], &part[2], &part[3]);

    /* each row: a field under a clip of its band; the paper above, between and below them when whole */
    int from = 0, line = 0, letters_before = 0, filled = w->y;
    const int bottom = w->y + w->h;
    for (int k = 0; k < rows; ++k) {
        const int ft = base0 + k * lh + cell_top, fb = ft + cell;
        const int band_top = ft > filled ? ft : filled, band_bottom = fb < bottom ? fb : bottom;
        if (paint->whole && band_top > filled) {
            dgx_screen_clip(paint->scr, part[0], part[1], part[2], part[3]);
            dgx_ui_lay_paper(paint, p->paper, w->x, filled, w->w, band_top - filled);
        }
        if (band_bottom > filled) filled = band_bottom;
        /* the line of this row */
        int start = len, end = len;
        while (from < len && line <= top + k) {
            const int next = lay_line(text, len, from, var, w->w, &end, ui->unit);
            start          = from;
            if (line < top + k) letters_before += letters_in(text, from, end);
            from = next, ++line;
        }
        if (line <= top + k) start = end = len; /* past the last line: an empty row */
        /* the letters this row shows, from row_first to letters_before: its own, and the last row of a text cut its
           ellipsis's dots as letters after them (no row follows it); the letters shown by one of the two passes, the
           last and this, from changed_from to changed_to (the same letters returned above): none of them in this row,
           it stands as drawn */
        const bool cut_row   = cut && k == rows - 1;
        const int  row_first = letters_before;
        letters_before += letters_in(text, start, end) + (cut_row ? (int)sizeof ELLIPSIS - 1 : 0);
        const int changed_from = come < come_before ? come : come_before, changed_to = come < come_before ? come_before : come;
        if (!paint->whole && !rows_anew && (changed_from >= letters_before || changed_to <= row_first)) continue;
        char s[LINE + sizeof ELLIPSIS];
        int  n = end - start > LINE ? LINE : end - start;
        memcpy(s, text + start, (size_t)n);
        s[n] = 0;
        if (cut_row) {
            /* the last line seen of a text cut: as much of it as goes with the ellipsis */
            while (n > 0) {
                while (n > 0 && s[n - 1] == ' ') --n;
                memcpy(s + n, ELLIPSIS, sizeof ELLIPSIS);
                if (width_of(f, s, (int)strlen(s)) * scale <= w->w) break;
                while (n > 0 && ((unsigned char)s[--n] & 0xC0) == 0x80) {}
                s[n] = 0;
            }
            if (n == 0) memcpy(s, ELLIPSIS, sizeof ELLIPSIS);
        }
        /* appearing: as many of its letters as came, each where it will stand */
        int fx = x, fw = w->w;
        dgx_font_align_t align = st->align;
        if (come < all_letters) {
            const int here = come - row_first;
            if (st->align != DGX_FONT_ALIGN_LEFT && here > 0) {
                const int room = w->w - width_of(f, s, (int)strlen(s)) * scale;
                const int lead = room <= 0 ? 0 : st->align == DGX_FONT_ALIGN_CENTER ? room / 2 : room;
                fx += lead, fw -= lead, align = DGX_FONT_ALIGN_LEFT;
                if (lead > 0 && clip_rows(paint, part, band_top, band_bottom))
                    dgx_ui_lay_paper(paint, p->paper, w->x, band_top, lead, band_bottom - band_top);
            }
            int b = 0;
            for (int l = 0; s[b] && l < (here > 0 ? here : 0); ++l) b = next_char(s, b);
            s[b] = 0;
        }
        if (clip_rows(paint, part, band_top, band_bottom)) {
            const dgx_font_context_t ctx = {.scr = paint->scr, .font = f, .color = ink, .bg = paper, .scale = scale, .field = fw, .align = align};
            dgx_ui_field_text(paint, &ctx, fx, base0 + k * lh - paint->oy, s, p->paper);
        }
    }
    dgx_screen_clip(paint->scr, part[0], part[1], part[2], part[3]);
    if (paint->whole && bottom > filled) dgx_ui_lay_paper(paint, p->paper, w->x, filled, w->w, bottom - filled);
    return more;
}

bool dgx_ui_draw_lines(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    return draw_lines(look, paint, ui, i, now_ms, -1, -1);
}

bool dgx_ui_draw_lines_in(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms, int chosen, int first_rows)
{
    return draw_lines(look, paint, ui, i, now_ms, chosen, first_rows);
}

static const dgx_ui_text_look_t lines_default = {.ink = 1, .paper = DGX_UI_PAPER};
int dgx_ui_lines_count(const dgx_ui_t *ui, const dgx_ui_text_style_t *style, const char *text, int width)
{
    return lines_count(ui, style, text, width, MAX_LINES);
}

const dgx_ui_look_t             dgx_ui_look_lines = {.draw = dgx_ui_draw_lines, .params = &lines_default, .fill = DGX_UI_PAPER};
