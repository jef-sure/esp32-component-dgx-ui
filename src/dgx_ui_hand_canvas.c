/*
 * dgx_ui_hand_canvas.c
 *
 *  The library's handwriting into a canvas (dgx_ui_handwriting_canvas): on a widget of DGX_UI_CANVAS a line is
 *  written into its 8-bit canvas as amounts of ink, by a soft pen too, and put on the screen over the paper or the
 *  drawn background; on any other widget, on a screen not of 16 bits, or with no canvas to be had, it is
 *  dgx_ui_handwriting, by a hard pen. A program that names only dgx_ui_handwriting carries none of this, nor the
 *  canvases.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <string.h>

#include "dgx_ui_hand_priv.h"
#include "dgx_ui_paint_priv.h"

_Static_assert(sizeof(dgx_ui_hand_held_t) <= DGX_UI_CANVAS_NOTE, "what a canvas of handwriting holds goes into its note");

/* the widget's canvas, none on a screen not of 16 bits, which a canvas does not go to: the hand writes straight there */
static dgx_screen_t *canvas_of(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, bool *made, void **note)
{
    if (paint->scr->color_bits != 16) {
        *made = false, *note = NULL;
        return NULL;
    }
    return dgx_ui_canvas_noted(paint, ui, i, made, note);
}

/* what the canvas holds is kept with it from pass to pass (dgx_ui_hand_held_t): written from nothing when made, begun
   again, or holding another text, another start of its letters, another variant or a later moment; else the new
   strokes only. Written so by any call, a composing too: the canvas holds what was written into it */
static bool into_canvas(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, const dgx_ui_font_variant_t *variant, const char *text,
                        const struct dgx_ui_hand_line_t *line, int ink)
{
    const dgx_ui_node_t    *node = dgx_ui_node(ui, i);
    const dgx_ui_textbox_t *box  = node->widget->text;
    bool                    made;
    void                   *note;
    dgx_screen_t           *canvas = canvas_of(paint, ui, i, &made, &note);
    if (!canvas) return false;
    dgx_ui_hand_held_t      *held = note;
    const dgx_ui_hand_held_t now  = {.text_hash  = dgx_ui_hand_hash(text, (int)strlen(text)),
                                     .since_ms   = line->since_ms,
                                     .written_ms = line->elapsed_ms,
                                     .variant    = (uint8_t)(variant - box->style->fonts),
                                     .written    = 1};
    const bool anew = made || line->anew || !held || !held->written || held->text_hash != now.text_hash || held->since_ms != now.since_ms ||
                      held->variant != now.variant || now.written_ms < held->written_ms;
    const uint32_t had_ms = anew ? 0 : held->written_ms;
    if (anew) dgx_fill_rectangle(canvas, 0, 0, canvas->width, canvas->height, 0);
    if (anew || line->elapsed_ms > had_ms) {
        dgx_hw_context_t pen = line->ctx;
        pen.scr = canvas, pen.color = 255, pen.width = dgx_ui_hand_pen(variant, ui->unit) | (int)(variant->pen & DGX_PEN_SOFT); /* soft as asked */
        dgx_ui_hand_write(&pen, line->x - node->x, line->base - node->y, text, dgx_ui_hand_effort_at(line, had_ms), line->to, !line->total_ms);
        if (held) *held = now;
    }
    dgx_ui_canvas_put(paint, ui, i, ink);
    return true;
}

static const struct dgx_ui_hand_canvas_t canvas_ways = {.line = into_canvas, .of = canvas_of, .put = dgx_ui_canvas_put};
const dgx_ui_hand_t dgx_ui_handwriting_canvas = {.width = dgx_ui_hand_width, .draw = dgx_ui_hand_draw, .base_rows = dgx_ui_hand_base_rows, .canvas = &canvas_ways};
