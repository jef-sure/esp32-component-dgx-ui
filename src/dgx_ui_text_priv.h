#pragma once
/*
 * dgx_ui_text_priv.h
 *
 *  What the looks of text share: a letter of UTF-8 stepped over, and a part of a text measured in a font. Each look
 *  that includes this has its own copy, so a program that uses only the one-line look pays for nothing else.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <string.h>

#include "dgx_font.h"
#include "dgx_ui_text.h"

/* bytes of a text measured at once, and of a line drawn */
enum { DGX_UI_LINE = 256 };

/* the byte after the letter of UTF-8 at i */
static inline int dgx_ui_next_char(const char *s, int i)
{
    ++i;
    while (((unsigned char)s[i] & 0xC0) == 0x80) ++i;
    return i;
}

/* how wide n bytes of a text are in a font, as dgx_font_draw() would set them: at most DGX_UI_LINE bytes measured */
static inline int dgx_ui_width_of(dgx_font_t *font, const char *s, int n)
{
    char line[DGX_UI_LINE + 1];
    if (n > DGX_UI_LINE) n = DGX_UI_LINE;
    if (n <= 0) return 0;
    memcpy(line, s, (size_t)n);
    line[n] = 0;
    const dgx_font_context_t ctx = {.font = font};
    return dgx_font_bounds(&ctx, line, NULL, NULL);
}

/* the handwriting a variant names (dgx_ui_hand.c): the width a text takes in it at its height, narrowed as far as the
   variant allows to go into field, INT_MAX for a variant it cannot write (no handwritten font, no height); and the
   one-line look's drawing of it */
struct dgx_ui_hand_line_t; /* a line placed and timed for a pass: dgx_ui_hand_priv.h */
struct dgx_ui_hand_t {
    int (*width)(const dgx_ui_font_variant_t *v, const char *text, int field);
    bool (*draw)(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms, const dgx_ui_font_variant_t *v);
    /* the rows of a variant's base line under the top of its line; -1 for a variant it cannot write */
    int (*base_rows)(const dgx_ui_font_variant_t *v);
    /* the writing into the canvas of a widget of DGX_UI_CANVAS (dgx_ui_hand_canvas.c); NULL: always straight */
    const struct dgx_ui_hand_canvas_t *canvas;
};
struct dgx_ui_hand_canvas_t {
    /* a line of the one-line look; false: no canvas to be had, written straight */
    bool (*line)(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, const dgx_ui_font_variant_t *v, const char *text,
                 const struct dgx_ui_hand_line_t *line, int ink);
    /* a widget's canvas and its putting, dgx_ui_canvas() and dgx_ui_canvas_put(), for the handwritten lines */
    dgx_screen_t *(*of)(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, bool *made);
    void (*put)(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, int ink);
};

/* the look of lines in a bitmap or dot variant a look of handwritten lines fitted from its ladder, its first row on
   first_rows under the box's top (dgx_ui_lines_look.c) */
bool dgx_ui_draw_lines_in(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms, int chosen, int first_rows);

/* a variant the looks can draw: a font of bitmaps or dots with an advance; handwriting not yet */
static inline bool dgx_ui_variant_usable(dgx_font_t *font)
{
    return font && font->f_type != DGX_FONT_HW && font->yAdvance > 0;
}

/* the base line of a style's first variant, in rows under the box's top: a text in a smaller variant stands on it.
   -1 when the first variant cannot be drawn: the text stands on its own */
static inline int dgx_ui_first_base(const dgx_ui_text_style_t *style)
{
    const dgx_ui_font_variant_t *first = &style->variants[0];
    if (first->hand) return first->hand->base_rows(first);
    return dgx_ui_variant_usable(first->font) ? -first->font->yOffsetLowest : -1;
}
