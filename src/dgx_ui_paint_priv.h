#pragma once
/*
 * dgx_ui_paint_priv.h
 *
 *  What the painter and the looks of a box and a frame share: the look a widget is drawn by.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_font.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_priv.h"

/* the look of a widget: its own, else its kind's; NULL: nothing draws it */
const dgx_ui_look_t *dgx_ui_look_of(const dgx_ui_widget_t *w);

/* the part of the paint's buffer a look may use now, its bytes in *size: past the painter's strip while the widget is
   drawn in strips over the buffer (a display of 18 or 24 bits), else all of it; NULL none */
void *dgx_ui_buffer_free(const dgx_ui_paint_t *paint, size_t *size);

/* the painter's strips, named by the program (dgx_ui_strips, dgx_ui_strips.c): a part r of widget i drawn through the
   buffer, *more set when the look asks for time; false, not drawn: the painter draws the part straight */
struct dgx_ui_strips_t {
    bool (*draw)(dgx_ui_paint_t *paint, const dgx_ui_look_t *look, dgx_ui_t *ui, int i, uint32_t now_ms, dgx_ui_rect_t r, bool *more);
};

/* the drawn halves of laying the paper and of letters in a field (dgx_ui_background.c), named by the program through
   dgx_ui_background_set(): the drawn background under a clip of a rectangle of the tree's screen; letters over it */
struct dgx_ui_drawn_background_t {
    void (*lay)(dgx_ui_paint_t *paint, int x, int y, int w, int h);
    void (*text)(dgx_ui_paint_t *paint, const dgx_font_context_t *ctx, int x, int y, const char *text, uint8_t paper);
};

/* the canvases of a paint (dgx_ui_canvas.c), reached by the painter through these two only: a program with no canvas
   carries none of them */
struct dgx_ui_canvases_t {
    void (*end)(dgx_ui_paint_t *paint, dgx_ui_t *ui); /* after a pass over ui: the canvases of widgets gone freed, NO_MEMORY told */
    void (*free)(dgx_ui_paint_t *paint);              /* all freed, the table too */
};

/* memory a look keeps for widget i from pass to pass, of at least size bytes, in the paint's table of canvases
   (dgx_ui_canvas.c): freed when the widget goes, as a canvas is; *made: new or larger, its contents not set. NULL:
   none to be had, the look does without */
void *dgx_ui_kept(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, size_t size, bool *made);

/* the canvas of widget i as dgx_ui_canvas() gives it, and in *note sixteen bytes a look keeps with it (dgx_ui_canvas.c),
   the paint's as the canvas is: zeros when the canvas is made or made again, gone with it; what they mean is the
   look's. NULL in *note with no canvas */
enum { DGX_UI_CANVAS_NOTE = 16 };
dgx_screen_t *dgx_ui_canvas_noted(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, bool *made, void **note);

/* a look's paper, an index or DGX_UI_PAPER, is the program's drawn background here: laid by dgx_ui_lay_paper(), and
   the letters and pictures over it drawn without paper */
static inline bool dgx_ui_paper_drawn(const dgx_ui_paint_t *paint, uint8_t paper)
{
    return paper == DGX_UI_PAPER && paint->on_background;
}

/* letters in a field of a look, as dgx_font_draw() draws them with ctx->field at x, y of paint's screen, the field's
   paper `paper`, an index or DGX_UI_PAPER (ctx->bg its colour). On a drawn background the field's rectangle is laid
   and the letters drawn over it without a field, set where the field sets them and cut to it */
void dgx_ui_field_text(dgx_ui_paint_t *paint, const dgx_font_context_t *ctx, int x, int y, const char *text, uint8_t paper);

/* a rectangle of the tree's screen composed, as dgx_ui_paint_area() does it; background: only where the drawn
   background shows (dgx_ui_background_changed()), nothing within a widget whose look fills */
void dgx_ui_compose(dgx_ui_t *ui, dgx_ui_paint_t *paint, dgx_ui_rect_t area, bool background);
