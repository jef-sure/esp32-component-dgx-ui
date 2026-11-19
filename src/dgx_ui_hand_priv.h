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
    uint32_t         since_ms;     /* when its letters began to come */
    uint32_t         elapsed_ms;   /* how long it has been coming, at most total_ms */
    float            effort, to;   /* the effort of the whole text and the effort written to by this pass */
    bool             anew;         /* begun again: written from nothing */
    bool             came;         /* all of it written */
};

/*
 * What a canvas of handwriting holds, in the sixteen bytes the paint keeps with the canvas (dgx_ui_canvas_noted()): a
 * call writes into the canvas from nothing when this is not what it writes now, else from written_ms on. Kept with the
 * canvas, not in the text box: another paint of the same tree has canvases of its own, written at moments of its own.
 */
typedef struct {
    uint32_t text_hash;  /* the text's, dgx_ui_hand_hash() */
    uint32_t since_ms;   /* when its letters began to come */
    uint32_t written_ms; /* how far the pen got, in ms of their time */
    uint16_t top;        /* the line on top */
    uint8_t  variant;    /* the variant of the style */
    uint8_t  written;    /* 0, as all the bytes of a new canvas: nothing in it */
} dgx_ui_hand_held_t;

/* a text's hash, FNV-1a of its len bytes */
static inline uint32_t dgx_ui_hand_hash(const char *text, int len)
{
    uint32_t hash = 2166136261u;
    for (int k = 0; k < len; ++k) hash = (hash ^ (uint8_t)text[k]) * 16777619u;
    return hash;
}

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
/* the width of n bytes of a text at the variant's height, not narrowed: a line of the handwritten lines */
int  dgx_ui_hand_span(const dgx_ui_font_variant_t *variant, const char *text, int n);
bool dgx_ui_hand_draw(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms, const dgx_ui_font_variant_t *variant);
/* the strokes from effort `from` to `to` written, those before `from` under an empty clip; all: the text at once */
void dgx_ui_hand_write(const dgx_hw_context_t *ctx, int x, int y, const char *text, float from, float to, bool all);
