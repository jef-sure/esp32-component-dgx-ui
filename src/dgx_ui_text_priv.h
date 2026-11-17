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

/* a variant the looks can draw: a font of bitmaps or dots with an advance; handwriting not yet */
static inline bool dgx_ui_variant_usable(dgx_font_t *font)
{
    return font && font->f_type != DGX_FONT_HW && font->yAdvance > 0;
}
