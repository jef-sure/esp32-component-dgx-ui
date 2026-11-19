#pragma once
/*
 * dgx_ui_hand_priv.h
 *
 *  What the library's handwriting (dgx_ui_hand.c) and its writing into a canvas (dgx_ui_hand_canvas.c) share.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_hw_font.h"
#include "dgx_ui_text_priv.h"

/* a line of handwriting placed and timed for a pass */
struct dgx_ui_hand_line_t {
    dgx_hw_context_t ctx;          /* its font, scales, pen and joining; the screen and the ink set where it is written */
    int              x, base;      /* the start of its base line on the tree's screen */
    uint32_t         total_ms;     /* how long the whole text takes to come; 0: it is there at once */
    uint32_t         elapsed_ms;   /* how long it has been coming, at most total_ms */
    float            effort, to;   /* the effort of the whole text and the effort written to by this pass */
    bool             anew;         /* begun again: written from nothing */
    bool             came;         /* all of it written */
};

/* the width of the pen in pixels, 1 for 0 */
static inline int dgx_ui_hand_pen(const dgx_ui_font_variant_t *variant)
{
    const int width = (int)(variant->pen & ~(uint32_t)DGX_PEN_SOFT);
    return width > 0 ? width : 1;
}

/* the effort written in had_ms of the line's time */
static inline float dgx_ui_hand_effort_at(const struct dgx_ui_hand_line_t *line, uint32_t had_ms)
{
    return line->total_ms ? line->effort * (float)had_ms / (float)line->total_ms : 0;
}

int  dgx_ui_hand_width(const dgx_ui_font_variant_t *variant, const char *text, int field);
int  dgx_ui_hand_base_rows(const dgx_ui_font_variant_t *variant);
bool dgx_ui_hand_draw(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms, const dgx_ui_font_variant_t *variant);
/* the strokes from effort `from` to `to` written, those before `from` under an empty clip; all: the text at once */
void dgx_ui_hand_write(const dgx_hw_context_t *ctx, int x, int y, const char *text, float from, float to, bool all);
