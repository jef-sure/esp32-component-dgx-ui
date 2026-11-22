/*
 * dgx_ui_background.c
 *
 *  The program's drawn background: its picture laid under a clip of a rectangle, the letters of a field over it, and
 *  the picture changed in a rectangle composed there at once where it shows, the widgets on it drawn again over it.
 *  Named by dgx_ui_background_set(): a program with no drawn background does not carry it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_ui_paint_priv.h"

/* the clip of the screen cut to a rectangle of it, in c (x, y, w, h); was: the clip it had. false: nothing of it
   within the clip, the clip as it was */
static bool clip_to(dgx_screen_t *scr, int x, int y, int w, int h, int c[4], int was[4])
{
    dgx_screen_clip_get(scr, &was[0], &was[1], &was[2], &was[3]);
    c[0] = DGX_MAX(x, was[0]), c[1] = DGX_MAX(y, was[1]);
    c[2] = DGX_MIN(x + w, was[0] + was[2]) - c[0], c[3] = DGX_MIN(y + h, was[1] + was[3]) - c[1];
    if (c[2] <= 0 || c[3] <= 0) return false;
    dgx_screen_clip(scr, c[0], c[1], c[2], c[3]);
    return true;
}

/* the program's picture on a rectangle of the tree's screen, under a clip of it within the painter's, the clip given
   back after */
static void lay(dgx_ui_paint_t *paint, int x, int y, int w, int h)
{
    dgx_screen_t *scr = paint->scr;
    int           c[4], was[4];
    if (!clip_to(scr, x - paint->ox, y - paint->oy, w, h, c, was)) return;
    paint->background_draw(paint, c[0] + paint->ox, c[1] + paint->oy, c[2], c[3], paint->background_ctx);
    dgx_screen_clip(scr, was[0], was[1], was[2], was[3]);
}

/* letters of a field over the picture: the field's rectangle, as high as the cell of DGX_FONT_FILL, laid; the letters
   set in it as the field sets them (the room left by their advances, a half of it centred), cut to it */
static void text(dgx_ui_paint_t *paint, const dgx_font_context_t *ctx, int x, int y, const char *s, uint8_t paper)
{
    const dgx_font_t *f     = ctx->font;
    const int         scale = ctx->scale > 1 ? ctx->scale : 1; /* a variant of points */
    const int         top = y + f->y_offset_lowest * scale, h = (f->y_bottom_max - f->y_offset_lowest) * scale;
    lay(paint, x + paint->ox, top + paint->oy, ctx->field, h);
    dgx_font_context_t c = *ctx;
    c.field = 0, c.flags &= ~(uint32_t)(DGX_FONT_FILL | DGX_FONT_BG);
    /* soft edges mixed with what lies under them; on a display that cannot be read, with the paper's colour */
    if (!dgx_screen_is_readable(paint->scr)) c.flags |= DGX_FONT_BG;
    const int room  = ctx->field - dgx_font_bounds(&c, s, NULL, NULL);
    const int start = ctx->align == DGX_FONT_ALIGN_RIGHT ? room : ctx->align == DGX_FONT_ALIGN_CENTER ? room / 2 : 0;
    int       in[4], was[4];
    if (!clip_to(paint->scr, x, top, ctx->field, h, in, was)) return;
    dgx_font_draw(&c, x + start, y, s);
    dgx_screen_clip(paint->scr, was[0], was[1], was[2], was[3]);
}

const dgx_ui_drawn_background_t dgx_ui_drawn_background = {.lay = lay, .text = text};

void dgx_ui_background_changed(dgx_ui_t *ui, dgx_ui_paint_t *paint, int x, int y, int w, int h)
{
    if (paint->drawn) dgx_ui_compose(ui, paint, dgx_ui_rect(x, y, w, h), true);
}
