/*
 * dgx_ui_list_look.c
 *
 *  The look of a list: its rows the lines of its text box ("Apple\nBanana\nCherry"), row r the line r, its value
 *  min + r; the row picked, the value, on a bar, of another colour while the list has the focus; a disabled list's
 *  rows in a colour of their own. The rows share the
 *  height of the box, the last taking the odd pixels; each line in the middle of its row, in the variant 0 of the
 *  style, a quarter of a line's height in from the edges as a dropdown's face sets its option, cut by its row. Drawn whole, every pixel of the box once; when only the pick or the focus changed, the row
 *  picked before and the row picked now, and nothing else.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_ui_text_priv.h"

static void fill(dgx_ui_paint_t *paint, int x, int y, int w, int h, uint32_t c)
{
    if (w > 0 && h > 0) dgx_fill_rectangle(paint->scr, x - paint->ox, y - paint->oy, w, h, c);
}

/* the line k of a text of lines ended by '\n': where it begins, and its bytes in *len; none past the last */
static const char *line_of(const char *text, int k, int *len)
{
    while (k > 0 && *text) {
        const char *end = strchr(text, '\n');
        if (!end) {
            text += strlen(text);
            break;
        }
        text = end + 1, --k;
    }
    if (k > 0) {
        *len = 0;
        return text;
    }
    const char *end = strchr(text, '\n');
    *len            = end ? (int)(end - text) : (int)strlen(text);
    return text;
}

/* row r of the list: its rows above and below the line's cell, then the line in a field as wide as the row, all under
   a clip of the row */
static void draw_row(dgx_ui_paint_t *paint, const dgx_ui_node_t *n, dgx_font_t *f, int align, int r, int rows, const char *text, uint32_t bg,
                     uint32_t fg)
{
    const int rh = n->h / rows, y = n->y + r * rh, h = r == rows - 1 ? n->h - r * rh : rh;
    int       px, py, pw, ph;
    dgx_screen_clip_get(paint->scr, &px, &py, &pw, &ph);
    const int y0 = y - paint->oy > py ? y - paint->oy : py, y1 = y + h - paint->oy < py + ph ? y + h - paint->oy : py + ph;
    if (y1 <= y0) return; /* the row out of the part drawn */
    dgx_screen_clip(paint->scr, px, y0, pw, y1 - y0);
    const int cell = f->yBottomMax - f->yOffsetLowest, base = y + (h - cell) / 2 - f->yOffsetLowest;
    const int top = base + f->yOffsetLowest, bottom = base + f->yBottomMax;
    fill(paint, n->x, y, n->w, top - y, bg);
    fill(paint, n->x, bottom, n->w, y + h - bottom, bg);
    int         len;
    const char *at = line_of(text, r, &len);
    char        line[DGX_UI_LINE + 1];
    if (len > DGX_UI_LINE) len = DGX_UI_LINE;
    memcpy(line, at, (size_t)len), line[len] = 0;
    /* a quarter of a line's height in from each edge, as a dropdown's face sets its option: the option stays where it
       is when the dropdown's list opens under it */
    int pad = cell / 4;
    if (2 * pad >= n->w) pad = 0;
    fill(paint, n->x, top, pad, bottom - top, bg);
    fill(paint, n->x + n->w - pad, top, pad, bottom - top, bg);
    const dgx_font_context_t ctx = {.scr = paint->scr, .font = f, .color = fg, .bg = bg, .field = n->w - 2 * pad, .align = (dgx_font_align_t)align};
    dgx_font_draw(&ctx, n->x + pad - paint->ox, base - paint->oy, line);
    dgx_screen_clip(paint->scr, px, py, pw, ph);
}

bool dgx_ui_draw_list(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t      *n = dgx_ui_node(ui, i);
    const dgx_ui_widget_t    *w = n->widget;
    const dgx_ui_list_look_t *p = look->params;
    const uint32_t            paper = dgx_ui_color(paint, p->paper == DGX_UI_PAPER ? paint->paper : p->paper);
    const dgx_ui_textbox_t   *box   = w->text;
    dgx_font_t               *f     = box && box->style && box->style->count ? box->style->variants[0].font : NULL;
    const int                 rows  = w->max - w->min + 1;
    if (!dgx_ui_variant_usable(f) || rows <= 0 || n->h < rows || n->w <= 0) {
        if (paint->whole) fill(paint, n->x, n->y, n->w, n->h, paper);
        return false;
    }
    const char    *text = box->buf ? box->buf : "";
    const bool     focused = n->state_value.bits & DGX_UI_FOCUSED, off = n->state_value.bits & DGX_UI_DISABLED;
    const uint32_t bar = dgx_ui_color(paint, focused ? p->focused : p->pick), ink = dgx_ui_color(paint, off ? p->disabled : p->ink);
    const uint32_t bar_ink = dgx_ui_color(paint, off ? p->disabled : p->pick_ink);
    const int      picked = n->state_value.value - w->min, was = n->state_drawn.value - w->min;
    /* only the pick or the focus changed: the row picked before and the row picked now; disabled, all of them */
    const bool all = paint->whole || (n->marks & (DGX_UI_MARK_CHANGED | DGX_UI_MARK_FIT)) ||
                     ((n->state_value.bits ^ n->state_drawn.bits) & DGX_UI_DISABLED);
    for (int r = 0; r < rows; ++r) {
        if (!all && r != picked && r != was) continue;
        draw_row(paint, n, f, box->style->align, r, rows, text, r == picked ? bar : paper, r == picked ? bar_ink : ink);
    }
    return false;
}

static const dgx_ui_list_look_t list_default = {.ink = 1, .paper = DGX_UI_PAPER, .pick = 3, .pick_ink = 0, .focused = 1, .disabled = 2};
const dgx_ui_look_t             dgx_ui_look_list = {.draw = dgx_ui_draw_list, .params = &list_default, .fill = -1};
