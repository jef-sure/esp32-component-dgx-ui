/*
 * dgx_ui_list_look.c
 *
 *  The look of a list: its rows the lines of its text box ("Apple\nBanana\nCherry"), row r the line r, its value
 *  min + r; the row picked, the value, on a bar, of another colour while the list has the focus; a disabled list's
 *  rows in a colour of their own. The rows shown share the height of the box, the last taking the odd pixels: all of
 *  them when they go in, else a window of them from its text box's top (dgx_ui_list_rows()), and a bar on the right
 *  edge, its length the window's share of the rows and its place the window's. Each line in the middle of its row, in
 *  the variant 0 of the style, a quarter of a line's height in from the edges as a dropdown's face sets its option,
 *  cut by its row. Drawn whole, every pixel of the box once; when only the pick or the focus changed, the row picked
 *  before and the row picked now, and nothing else.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_ui_paint_priv.h"
#include "dgx_ui_text.h"
#include "dgx_ui_text_priv.h"

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

/* row r of the window of so many rows, each rh high and rw wide, showing the line `line`: its rows above and below the
   line's cell, then the line in a field as wide as the row, all under a clip of the row; its paper the index paper
   (DGX_UI_PAPER: what lies under the list) */
static void draw_row(dgx_ui_paint_t *paint, const dgx_ui_node_t *n, const dgx_font_t *f, int align, int r, int rows, int rh, int rw, int line_k,
                     const char *text, uint8_t paper, uint32_t fg)
{
    const uint32_t bg = dgx_ui_color(paint, paper == DGX_UI_PAPER ? paint->paper : paper);
    const int      y = n->y + r * rh, h = r == rows - 1 ? n->h - r * rh : rh;
    int       px, py, pw, ph;
    dgx_screen_clip_get(paint->scr, &px, &py, &pw, &ph);
    const int y0 = y - paint->oy > py ? y - paint->oy : py, y1 = y + h - paint->oy < py + ph ? y + h - paint->oy : py + ph;
    if (y1 <= y0) return; /* the row out of the part drawn */
    dgx_screen_clip(paint->scr, px, y0, pw, y1 - y0);
    const int cell = f->y_bottom_max - f->y_offset_lowest, base = y + (h - cell) / 2 - f->y_offset_lowest;
    const int top = base + f->y_offset_lowest, bottom = base + f->y_bottom_max;
    dgx_ui_lay_paper(paint, paper, n->x, y, rw, top - y);
    dgx_ui_lay_paper(paint, paper, n->x, bottom, rw, y + h - bottom);
    int         len;
    const char *at = line_of(text, line_k, &len);
    char        line[DGX_UI_LINE + 1];
    if (len > DGX_UI_LINE) len = DGX_UI_LINE;
    memcpy(line, at, (size_t)len), line[len] = 0;
    /* a quarter of a line's height in from each edge, as a dropdown's face sets its option: the option stays where it
       is when the dropdown's list opens under it */
    int pad = cell / 4;
    if (2 * pad >= rw) pad = 0;
    dgx_ui_lay_paper(paint, paper, n->x, top, pad, bottom - top);
    dgx_ui_lay_paper(paint, paper, n->x + rw - pad, top, pad, bottom - top);
    const dgx_font_context_t ctx = {.scr = paint->scr, .font = f, .color = fg, .bg = bg, .field = rw - 2 * pad, .align = (dgx_font_align_t)align};
    dgx_ui_field_text(paint, &ctx, n->x + pad - paint->ox, base - paint->oy, line, paper);
    dgx_screen_clip(paint->scr, px, py, pw, ph);
}

bool dgx_ui_draw_list(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t      *n = dgx_ui_node(ui, i);
    const dgx_ui_widget_t    *w = n->widget;
    const dgx_ui_list_look_t *p = look->params;
    static bool               told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
    dgx_ui_tell_no_place(i, w, "a list", &told);
    dgx_ui_textbox_t         *box   = w->text;
    const dgx_font_t         *f     = box && box->style && box->style->count ? box->style->variants[0].font : NULL;
    const int                 all   = w->max - w->min + 1;
    int                       rh, top;
    const int                 rows = all > 0 ? dgx_ui_list_rows(ui, n, &rh, &top) : 0;
    if (!dgx_ui_variant_usable(f) || rows <= 0 || rh <= 0 || n->w <= 0) {
        if (paint->whole) dgx_ui_lay_paper(paint, p->paper, n->x, n->y, n->w, n->h);
        return false;
    }
    const char    *text = box->buf ? box->buf : "";
    const bool     focused = dgx_ui_bits_shown(ui, n) & DGX_UI_FOCUSED, off = n->state.bits & DGX_UI_DISABLED;
    const uint8_t  bar = focused ? p->focused : p->pick;
    const uint32_t ink = dgx_ui_color(paint, off ? p->disabled : p->ink);
    const uint32_t bar_ink = dgx_ui_color(paint, off ? p->disabled : p->pick_ink);
    /* the window brought to the pick, its value set or its height changed (DGX_UI_MARK_WINDOW); stored, as what a look
       remembers, by the last part of the painter's own pass, the parts before it finding the same */
    const int pick = dgx_ui_value_of(n) - w->min;
    if ((n->marks & DGX_UI_MARK_WINDOW) && rows < all) top = pick < top ? pick : pick >= top + rows ? pick - rows + 1 : top;
    const bool moved = top != box->top;
    if (moved && !paint->composing && paint->part + 1 >= paint->parts) box->top = (uint16_t)top;
    const int picked = pick - top, was = n->state_drawn.value - w->min - top;
    /* a list that scrolls: the bar on its right edge, the rows narrower by it */
    const int bw = rows < all && 4 * ui->unit < n->w ? 2 * ui->unit : 0, rw = n->w - bw;
    /* only the pick or the focus changed: the row picked before and the row picked now; disabled, or the window
       moved (CHANGED), all of them */
    const bool whole = paint->whole || moved || (n->marks & (DGX_UI_MARK_CHANGED | DGX_UI_MARK_FIT)) ||
                       ((n->state.bits ^ n->state_drawn.bits) & DGX_UI_DISABLED);
    for (int r = 0; r < rows; ++r) {
        if (!whole && r != picked && r != was) continue;
        draw_row(paint, n, f, box->style->align, r, rows, rh, rw, top + r, text, r == picked ? bar : p->paper, r == picked ? bar_ink : ink);
    }
    if (bw && whole) {
        /* the window's share of the rows, at its place: the paper above and below it */
        const int bx = n->x + rw, ty = n->y + n->h * top / all; /* from the share above the window to the share up to its end */
        int       th = n->y + n->h * (top + rows) / all - ty;
        if (th < 1) th = 1;
        if (ty + th > n->y + n->h) th = n->y + n->h - ty;
        dgx_ui_lay_paper(paint, p->paper, bx, n->y, bw, ty - n->y);
        dgx_fill_rectangle(paint->scr, bx - paint->ox, ty - paint->oy, bw, th, dgx_ui_color(paint, p->bar));
        dgx_ui_lay_paper(paint, p->paper, bx, ty + th, bw, n->y + n->h - ty - th);
    }
    return false;
}

static const dgx_ui_list_look_t list_default = {.ink = 1, .paper = DGX_UI_PAPER, .pick = 3, .pick_ink = 0, .focused = 1, .disabled = 2, .bar = 3};
const dgx_ui_look_t             dgx_ui_look_list = {.draw = dgx_ui_draw_list, .params = &list_default, .fill = -1};
