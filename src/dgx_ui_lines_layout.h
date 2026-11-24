#pragma once
/*
 * dgx_ui_lines_layout.h
 *
 *  The laying of a text into lines, one source for the look of lines (dgx_ui_lines_look.c) and the handwriting's
 *  (dgx_ui_hand_lines.c). Each includes it with its own measure defined before:
 *
 *      LINES_MEASURE(v, s, n, unit)  the width of n bytes of s in the variant v, in a tree of that unit
 *      LINES_HEIGHT(v, unit)         the height of a line of the variant v; 0 when the look cannot draw it
 *
 *  so that the look of lines lays by its fonts' own measure as it always did, and a program that names no handwriting
 *  carries nothing of the other.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_text_priv.h"

/* letters of the bytes from .. to of a text */
static int letters_in(const char *s, int from, int to)
{
    int n = 0;
    for (int k = from; k < to; k = dgx_ui_next_char(s, k)) ++n;
    return n;
}

/* the line of a text that begins at byte from: its end in *end; where the next begins (past spaces and one break) */
static int lay_line(const char *text, int len, int from, const dgx_ui_font_variant_t *v, int width, int *end, int unit)
{
    const int start = from;
    int       e     = start; /* the end of the last word that goes in */
    for (;;) {
        int w = e;
        while (w < len && text[w] == ' ') ++w;
        int we = w;
        while (we < len && text[we] != ' ' && text[we] != '\n') ++we;
        if (we - start > DGX_UI_LINE || LINES_MEASURE(v, text + start, we - start, unit) > width) {
            if (e == start) {
                /* one word wider than the line: as many of its letters as go, one at least */
                int c = start;
                while (c < we && dgx_ui_next_char(text, c) - start <= DGX_UI_LINE &&
                       LINES_MEASURE(v, text + start, dgx_ui_next_char(text, c) - start, unit) <= width)
                    c = dgx_ui_next_char(text, c);
                e = c > start ? c : dgx_ui_next_char(text, start);
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
static int count_lines(const char *text, int len, const dgx_ui_font_variant_t *v, int width, int limit, int rows, int *letters, int unit)
{
    int n = 0, from = 0, l = 0;
    while (from < len && n < limit) {
        int       end;
        const int next = lay_line(text, len, from, v, width, &end, unit);
        if (n < rows) l += letters_in(text, from, end);
        from = next, ++n;
    }
    if (letters) *letters = l;
    return n;
}

/* the first variant the text goes into whole, its lines in the rows the box has for it; else the one that shows
   the most letters, the earlier of equals; -1 when there is none the look can draw */
static int fit_lines(const dgx_ui_text_style_t *st, const char *text, int len, int width, int height, int unit)
{
    int best = -1, most = -1;
    for (int v = 0; v < st->count; ++v) {
        const int line_height = LINES_HEIGHT(&st->fonts[v], unit);
        if (line_height <= 0) continue;
        const int rows = height / line_height > 0 ? height / line_height : 1;
        int       letters;
        const int n = count_lines(text, len, &st->fonts[v], width, rows + 1, rows, &letters, unit);
        if (n <= rows) return v;
        if (letters > most) best = v, most = letters;
    }
    return best;
}
