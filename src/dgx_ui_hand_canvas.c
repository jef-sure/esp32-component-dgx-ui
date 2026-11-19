/*
 * dgx_ui_hand_canvas.c
 *
 *  The library's handwriting into a canvas (dgx_ui_handwriting_canvas): on a widget of DGX_UI_CANVAS a line is
 *  written into its 8-bit canvas as amounts of ink, by a soft pen too, and put on the screen over the paper or the
 *  drawn background; on any other widget, or with no canvas to be had, it is dgx_ui_handwriting. A program that
 *  names only dgx_ui_handwriting carries none of this, nor the canvases.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_hand_priv.h"
#include "dgx_ui_paint_priv.h"

/* what the canvas holds is kept with it from pass to pass: written from nothing when made or begun again, else the
   new strokes only */
static bool into_canvas(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, const dgx_ui_font_variant_t *variant, const char *text,
                        const struct dgx_ui_hand_line_t *line, int ink)
{
    dgx_ui_node_t    *node = dgx_ui_node(ui, i);
    dgx_ui_textbox_t *box  = node->widget->text;
    bool              made;
    dgx_screen_t     *canvas = dgx_ui_canvas(paint, ui, i, &made);
    if (!canvas) return false;
    const bool     anew   = made || line->anew;
    const uint32_t had_ms = anew ? 0 : box->letters_drawn;
    if (anew) dgx_fill_rectangle(canvas, 0, 0, canvas->width, canvas->height, 0);
    if (anew || line->elapsed_ms > had_ms) {
        dgx_hw_context_t pen = line->ctx;
        pen.scr = canvas, pen.color = 255, pen.width = dgx_ui_hand_pen(variant) | (int)(variant->pen & DGX_PEN_SOFT); /* soft as asked */
        dgx_ui_hand_write(&pen, line->x - node->x, line->base - node->y, text, dgx_ui_hand_effort_at(line, had_ms), line->to, !line->total_ms);
        box->letters_drawn = (uint16_t)(line->elapsed_ms > 0xFFFF ? 0xFFFF : line->elapsed_ms);
    }
    dgx_ui_canvas_put(paint, ui, i, ink);
    return true;
}

static const struct dgx_ui_hand_canvas_t canvas_ways = {.line = into_canvas, .of = dgx_ui_canvas, .put = dgx_ui_canvas_put};
const dgx_ui_hand_t dgx_ui_handwriting_canvas = {.width = dgx_ui_hand_width, .draw = dgx_ui_hand_draw, .base_rows = dgx_ui_hand_base_rows, .canvas = &canvas_ways};
