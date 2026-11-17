/*
 * dgx_ui_lines_look.c
 *
 *  The look of a text of lines, dgx_ui_look_lines, for a widget that names it: the text laid into lines word by word
 *  each time it is drawn (a word wider than a line by
 *  its letters), the variant of the style fitted, each line shown a field of DGX under a clip of its own row, the
 *  paper between the fields only when drawn whole. An important text too long scrolls a line every scroll_ms; one
 *  that is not is cut, its last line seen ending in an ellipsis. Letters appear one by one when asked, each where it
 *  will stand. Fonts of bitmaps and dots; handwriting not yet.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_ui_text_priv.h"

enum { LINE = DGX_UI_LINE, MAX_LINES = 1000 };
static const char ELLIPSIS[] = "...";

#define next_char dgx_ui_next_char
#define width_of  dgx_ui_width_of

static int letters_in(const char *s, int from, int to)
{
    int n = 0;
    for (int k = from; k < to; k = next_char(s, k)) ++n;
    return n;
}

/* the line of a text that begins at byte from: its end in *end; where the next begins (past spaces and one break) */
static int lay_line(const char *text, int len, int from, dgx_font_t *f, int width, int *end)
{
    const int start = from;
    int       e     = start; /* the end of the last word that goes in */
    for (;;) {
        int w = e;
        while (w < len && text[w] == ' ') ++w;
        int we = w;
        while (we < len && text[we] != ' ' && text[we] != '\n') ++we;
        if (we - start > LINE || width_of(f, text + start, we - start) > width) {
            if (e == start) {
                /* one word wider than the line: as many of its letters as go, one at least */
                int c = start;
                while (c < we && next_char(text, c) - start <= LINE && width_of(f, text + start, next_char(text, c) - start) <= width) c = next_char(text, c);
                e = c > start ? c : next_char(text, start);
            }
            break;
        }
        e = we;
        if (we >= len || text[we] == '\n') break;
    }
    *end     = e;
    int next = e;
    while (next < len && text[next] == ' ') ++next;
    if (next < len && text[next] == '\n') ++next;
    return next;
}

/* how many lines a text takes, up to a limit; and the letters of its first `rows` lines */
static int count_lines(const char *text, int len, dgx_font_t *f, int width, int limit, int rows, int *letters)
{
    int n = 0, from = 0, l = 0;
    while (from < len && n < limit) {
        int end;
        const int next = lay_line(text, len, from, f, width, &end);
        if (n < rows) l += letters_in(text, from, end);
        from = next, ++n;
    }
    if (letters) *letters = l;
    return n;
}

static bool usable(const dgx_ui_font_variant_t *v)
{
    return dgx_ui_variant_usable(v->font);
}

/* the first variant the text goes into whole, its lines in the rows the box has for it; else the one that shows
   the most letters, the earlier of equals; -1 when there is none of a bitmap or dot font */
static int fit(const dgx_ui_text_style_t *st, const char *text, int len, int width, int height)
{
    int best = -1, most = -1;
    for (int v = 0; v < st->count; ++v) {
        if (!usable(&st->variants[v])) continue;
        dgx_font_t *f    = st->variants[v].font;
        const int   rows = height / f->yAdvance > 0 ? height / f->yAdvance : 1;
        int         letters;
        const int   n = count_lines(text, len, f, width, rows + 1, rows, &letters);
        if (n <= rows) return v;
        if (letters > most) best = v, most = letters;
    }
    return best;
}

/* the clip of the screen cut to rows top .. bottom of the tree's screen; false when nothing of them is left */
static bool clip_rows(dgx_ui_paint_t *paint, const int part[4], int top, int bottom)
{
    const int t = top - paint->oy, b = bottom - paint->oy;
    const int y0 = part[1] > t ? part[1] : t, y1 = part[1] + part[3] < b ? part[1] + part[3] : b;
    if (y1 <= y0) return false;
    dgx_screen_clip(paint->scr, part[0], y0, part[2], y1 - y0);
    return true;
}

bool dgx_ui_draw_lines(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    dgx_ui_node_t            *w   = dgx_ui_node(ui, i);
    dgx_ui_textbox_t         *box = w->widget->text;
    const dgx_ui_text_look_t *p   = look->params;
    const uint32_t ink = dgx_ui_color(paint, p->ink), paper = dgx_ui_color(paint, p->paper == DGX_UI_PAPER ? paint->paper : p->paper);
    const int      x = w->x - paint->ox;
    if (!box || !box->style || !box->style->count || w->w <= 0) {
        if (paint->whole) dgx_fill_rectangle(paint->scr, x, w->y - paint->oy, w->w, w->h, paper);
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
    if (w->marks & DGX_UI_MARK_FIT) v = fit(st, text, len, w->w, w->h);
    if (v < 0 || v >= st->count || !usable(&st->variants[v])) {
        if (paint->whole) dgx_fill_rectangle(paint->scr, x, w->y - paint->oy, w->w, w->h, paper);
        return false;
    }
    if (v != box->variant) paint->whole = true; /* another height: the rows around the fields too */
    const uint32_t since = (box->flags & DGX_UI_TEXTBOX_START) ? now_ms : box->since_ms;
    dgx_font_t    *f = st->variants[v].font, *f0 = usable(&st->variants[0]) ? st->variants[0].font : f;
    const int      lh = f->yAdvance, cell_top = f->yOffsetLowest, cell = f->yBottomMax - f->yOffsetLowest;
    const int      rows = w->h / lh > 0 ? w->h / lh : 1;

    /* the lines and letters of the text: counted again only when the text, the width or the variant changed */
    int total, all_letters;
    if (!(box->flags & DGX_UI_TEXTBOX_COUNTED) || (w->marks & (DGX_UI_MARK_CHANGED | DGX_UI_MARK_FIT)) || v != box->variant) {
        total = count_lines(text, len, f, w->w, MAX_LINES, MAX_LINES, &all_letters);
    } else {
        total = box->total_lines, all_letters = box->total_letters;
    }
    /* the letters come so far and the first line shown; once all came, and once the scroll reached its end, no
       time is counted for them: a text drawn again days later is as it was */
    const bool     important = st->flags & DGX_UI_TEXT_IMPORTANT;
    const int      last_top  = total > rows ? total - rows : 0;
    const uint32_t elapsed   = now_ms - since; /* by difference: now_ms runs over in 49.7 days */
    const bool     came      = !box->letter_ms || (box->flags & DGX_UI_TEXTBOX_CAME);
    const uint32_t appear_ms = box->letter_ms ? (uint32_t)all_letters * box->letter_ms : 0;
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
    if (commit) {
        box->variant = (uint8_t)v, box->top = (uint16_t)top, box->letters_drawn = (uint16_t)come;
        box->total_lines = (uint16_t)total, box->total_letters = (uint16_t)(all_letters > 0xFFFF ? 0xFFFF : all_letters);
        box->since_ms    = since;
        box->flags       = (uint8_t)((box->flags & ~DGX_UI_TEXTBOX_START) | DGX_UI_TEXTBOX_COUNTED);
        if (come >= all_letters) box->flags |= DGX_UI_TEXTBOX_CAME;
        if (important && st->scroll_ms && total > rows && top >= last_top) box->flags |= DGX_UI_TEXTBOX_ENDED;
    }
    if (timed_only && same) return more;
    const bool cut   = !important && total - top > rows; /* lines follow the last row: it ends in an ellipsis */
    const int  shown = total - top < rows ? total - top : rows;

    /* the base line of the first row: the first variant's, or the block of the rows shown in the middle */
    int base0 = w->y - f0->yOffsetLowest;
    if (st->flags & DGX_UI_TEXT_MIDDLE) {
        const int block = shown > 0 ? (shown - 1) * lh + cell : cell;
        base0           = w->y + (w->h - block) / 2 - cell_top;
        if (rows > 1) paint->whole = true; /* the block moves with the number of its lines: the rows around it too */
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
            dgx_fill_rectangle(paint->scr, x, filled - paint->oy, w->w, band_top - filled, paper);
        }
        if (band_bottom > filled) filled = band_bottom;
        /* the line of this row */
        int start = len, end = len;
        while (from < len && line <= top + k) {
            const int next = lay_line(text, len, from, f, w->w, &end);
            start          = from;
            if (line < top + k) letters_before += letters_in(text, from, end);
            from = next, ++line;
        }
        if (line <= top + k) start = end = len; /* past the last line: an empty row */
        char s[LINE + sizeof ELLIPSIS];
        int  n = end - start > LINE ? LINE : end - start;
        memcpy(s, text + start, (size_t)n);
        s[n] = 0;
        if (cut && k == rows - 1) {
            /* the last line seen of a text cut: as much of it as goes with the ellipsis */
            while (n > 0) {
                while (n > 0 && s[n - 1] == ' ') --n;
                memcpy(s + n, ELLIPSIS, sizeof ELLIPSIS);
                if (width_of(f, s, (int)strlen(s)) <= w->w) break;
                while (n > 0 && ((unsigned char)s[--n] & 0xC0) == 0x80) {}
                s[n] = 0;
            }
            if (n == 0) memcpy(s, ELLIPSIS, sizeof ELLIPSIS);
        }
        /* appearing: as many of its letters as came, each where it will stand */
        int fx = x, fw = w->w;
        dgx_font_align_t align = st->align;
        if (come < all_letters) {
            const int here = come - letters_before;
            if (st->align != DGX_FONT_ALIGN_LEFT && here > 0) {
                const int room = w->w - width_of(f, s, (int)strlen(s));
                const int lead = room <= 0 ? 0 : st->align == DGX_FONT_ALIGN_CENTER ? room / 2 : room;
                fx += lead, fw -= lead, align = DGX_FONT_ALIGN_LEFT;
                if (lead > 0 && clip_rows(paint, part, band_top, band_bottom))
                    dgx_fill_rectangle(paint->scr, x, band_top - paint->oy, lead, band_bottom - band_top, paper);
            }
            int b = 0;
            for (int l = 0; s[b] && l < (here > 0 ? here : 0); ++l) b = next_char(s, b);
            s[b] = 0;
        }
        if (clip_rows(paint, part, band_top, band_bottom)) {
            const dgx_font_context_t ctx = {.scr = paint->scr, .font = f, .color = ink, .bg = paper, .field = fw, .align = align};
            dgx_font_draw(&ctx, fx, base0 + k * lh - paint->oy, s);
        }
        letters_before += letters_in(text, start, end); /* the letters of this row came before the next row's */
    }
    dgx_screen_clip(paint->scr, part[0], part[1], part[2], part[3]);
    if (paint->whole && bottom > filled) dgx_fill_rectangle(paint->scr, x, filled - paint->oy, w->w, bottom - filled, paper);
    return more;
}

static const dgx_ui_text_look_t lines_default = {.ink = 1, .paper = DGX_UI_PAPER};
const dgx_ui_look_t             dgx_ui_look_lines = {.draw = dgx_ui_draw_lines, .params = &lines_default, .fill = -1};
