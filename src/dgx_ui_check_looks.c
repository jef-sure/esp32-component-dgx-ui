/*
 * dgx_ui_check_looks.c
 *
 *  The looks of a radio button and of a checkbox: a mark as high as a line of the text at the left (a ring, a square),
 *  in it a sign while chosen or on (a dot, a tick), and the widget's text to the right. The mark's square is written
 *  row by row in runs of one colour, each pixel classed once by where it lies, so every pixel of the box is written
 *  once in any state. The mark stands two points in from the box's edges, so that the focus, a frame of a point round
 *  the box in the colour `focused`, has a point of paper between it and the mark; the inside is drawn the same under the
 *  clip less the frame. The layout gives the box a point more above and below its line for that frame. Each look is a
 *  function of its own; a program that names neither carries neither, as the linker keeps of an archive only what is
 *  named (-ffunction-sections).
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_ui_paint_priv.h"
#include "dgx_ui_text.h"
#include "dgx_ui_text_priv.h"

/* what a pixel of the mark's square is */
enum { AROUND, MARK, INSIDE, SIGN, CLASSES };

/* the class of the pixel at column x, row y of a square of side `side`; on: the sign shown */
typedef int (*class_fn)(int x, int y, int side, bool on);

/* the thickness of a ring or of a square's border in pixels for a mark of side `side` */
static int border_of(int side)
{
    return 1 + side / 12;
}

/* ---- a ring and a dot, of radii in halves of a pixel from the square's middle ---- */

static int ring_class(int x, int y, int side, bool on)
{
    const int border = border_of(side);
    const int dx = 2 * x + 1 - side, dy = 2 * y + 1 - side, far = dx * dx + dy * dy; /* in halves of a pixel */
    const int outer = side, inner = side - 2 * border, dot = side - 4 * border;
    if (far > outer * outer) return AROUND;
    if (far > inner * inner) return MARK;
    return on && dot > 0 && far <= dot * dot ? SIGN : INSIDE;
}

/* ---- a square and a tick: two strokes at 45 degrees from the left, down to the bottom, then up to the right ---- */

/* whether the point px, py lies within `half` of the stroke from ax, ay going `run` along x at 45 degrees, down (fall
   1) or up (fall -1); all in halves of a pixel. Along a stroke of 45 degrees the distances are sums and differences */
static bool near_stroke(int px, int py, int ax, int ay, int run, int fall, int half)
{
    const int vx = px - ax, vy = (py - ay) * fall; /* the stroke as one going down */
    const int along = vx + vy, across = vx - vy;
    if (along >= 0 && along <= 2 * run) return across * across <= 2 * half * half;
    const int ex = along < 0 ? vx : vx - run, ey = along < 0 ? vy : vy - run; /* from the nearer end */
    return ex * ex + ey * ey <= half * half;
}

static int square_class(int x, int y, int side, bool on)
{
    const int border = border_of(side);
    if (x < border || y < border || x >= side - border || y >= side - border) return MARK;
    if (!on) return INSIDE;
    const int px = 2 * x + 1, py = 2 * y + 1, span = 2 * side, half = border + 1; /* halves of a pixel */
    const int ax = span * 22 / 100, ay = span / 2, down = span * 18 / 100, up = span * 40 / 100;
    return near_stroke(px, py, ax, ay, down, 1, half) || near_stroke(px, py, ax + down, ay + down, up, -1, half) ? SIGN : INSIDE;
}

/* ---- the box: the mark's column, a gap, the text's field ---- */

static void draw_inside(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, class_fn class_of, bool on)
{
    const dgx_ui_node_t       *n    = dgx_ui_node(ui, i);
    const dgx_ui_check_look_t *p    = look->params;
    const unsigned             bits = n->state.bits;
    const bool                 off  = bits & DGX_UI_DISABLED;
    const dgx_ui_textbox_t    *box  = n->widget->text;
    const dgx_font_t          *font = box && box->style && box->style->count ? box->style->variants[0].font : NULL;
    if (!dgx_ui_variant_usable(font)) font = NULL;
    const int s    = font ? dgx_ui_variant_scale(&box->style->variants[0], ui->unit) : 1; /* the pixels a pixel of the font takes */
    const int cell = font ? (font->y_bottom_max - font->y_offset_lowest) * s : n->h;
    /* the mark two points in from the edges: the frame of the focus, then a point of paper */
    const int point = ui->unit, margin = 2 * point;
    int       side  = cell < n->h - 2 * margin ? cell : n->h - 2 * margin;
    if (side > n->w - 2 * margin) side = n->w - 2 * margin; /* the margin on the right too, where there is no text */
    if (side < 0) side = 0;
    const int     mark_x = n->x + margin, mark_y = n->y + (n->h - side) / 2, gap = side / 2;
    const uint8_t index[CLASSES] = {
        [AROUND] = p->paper,
        [MARK]   = off ? p->disabled : p->mark,
        [INSIDE] = (bits & DGX_UI_PRESSED) && !off ? p->pressed : p->inside,
        [SIGN]   = off ? p->disabled : p->sign,
    };
    int clip_x, clip_y, clip_w, clip_h;
    dgx_screen_clip_get(paint->scr, &clip_x, &clip_y, &clip_w, &clip_h);
    /* the two points before the mark, then the mark's column: paper above and below, the square row by row in runs,
       the rows the clip leaves out skipped */
    dgx_ui_lay_paper(paint, p->paper, n->x, n->y, margin < n->w ? margin : n->w, n->h);
    dgx_ui_lay_paper(paint, p->paper, mark_x, n->y, side, mark_y - n->y);
    dgx_ui_lay_paper(paint, p->paper, mark_x, mark_y + side, side, n->y + n->h - mark_y - side);
    for (int row = 0; row < side; ++row) {
        const int y = mark_y + row - paint->oy;
        if (y < clip_y || y >= clip_y + clip_h) continue;
        for (int start = 0, column = 1, run = class_of(0, row, side, on); column <= side; ++column) {
            const int next = column < side ? class_of(column, row, side, on) : -1;
            if (next == run) continue;
            dgx_ui_lay_paper(paint, index[run], mark_x + start, mark_y + row, column - start, 1);
            start = column, run = next;
        }
    }
    /* the gap and the text's column: the rows of the text in its field, paper above and below */
    const int text_x = mark_x + side + gap, text_w = n->x + n->w - text_x;
    dgx_ui_lay_paper(paint, p->paper, mark_x + side, n->y, (text_w > 0 ? gap : n->x + n->w - mark_x - side), n->h);
    if (text_w <= 0) return;
    if (!font) {
        dgx_ui_lay_paper(paint, p->paper, text_x, n->y, text_w, n->h);
        return;
    }
    const int base = n->y + (n->h - cell) / 2 - font->y_offset_lowest * s;
    const int top = base + font->y_offset_lowest * s, bottom = base + font->y_bottom_max * s;
    dgx_ui_lay_paper(paint, p->paper, text_x, n->y, text_w, top - n->y);
    dgx_ui_lay_paper(paint, p->paper, text_x, bottom, text_w, n->y + n->h - bottom);
    const int rows_top = (top > n->y ? top : n->y) - paint->oy, rows_bottom = (bottom < n->y + n->h ? bottom : n->y + n->h) - paint->oy;
    const int from = rows_top > clip_y ? rows_top : clip_y, to = rows_bottom < clip_y + clip_h ? rows_bottom : clip_y + clip_h;
    if (to <= from) return;
    /* the first line of the text box: a radio's or a checkbox's words */
    char        line[DGX_UI_LINE + 1];
    const char *text = box->buf ? box->buf : "", *end = strchr(text, '\n');
    int         length = end ? (int)(end - text) : (int)strlen(text);
    if (length > DGX_UI_LINE) length = DGX_UI_LINE;
    memcpy(line, text, (size_t)length), line[length] = 0;
    const uint8_t            paper = p->paper;
    const dgx_font_context_t ctx   = {.scr   = paint->scr,
                                      .font  = font,
                                      .color = dgx_ui_color(paint, off ? p->ink_disabled : p->ink),
                                      .bg    = dgx_ui_color(paint, paper == DGX_UI_PAPER ? paint->paper : paper),
                                      .scale = s,
                                      .field = text_w,
                                      .align = DGX_FONT_ALIGN_LEFT};
    dgx_screen_clip(paint->scr, clip_x, from, clip_w, to - from);
    dgx_ui_field_text(paint, &ctx, text_x - paint->ox, base - paint->oy, line, paper);
    dgx_screen_clip(paint->scr, clip_x, clip_y, clip_w, clip_h);
}

/* the box, and round it the frame of the focus */
static void draw_checked(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, class_fn class_of, bool on)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    dgx_ui_frame_t       frame;
    if (dgx_ui_frame_begin(paint, ui, i, dgx_ui_rect(n->x, n->y, n->w, n->h), ((const dgx_ui_check_look_t *)look->params)->focused, &frame))
        draw_inside(look, paint, ui, i, class_of, on);
    dgx_ui_frame_end(paint, ui, &frame);
}

bool dgx_ui_draw_radio(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    static bool          told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
    dgx_ui_tell_no_place(i, n->widget, "a radio", &told);
    draw_checked(look, paint, ui, i, ring_class, dgx_ui_radio_chosen(n));
    return false;
}

bool dgx_ui_draw_checkbox(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    static bool          told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
    dgx_ui_tell_no_place(i, n->widget, "a checkbox", &told);
    draw_checked(look, paint, ui, i, square_class, dgx_ui_value_of(n) != 0);
    return false;
}

static const dgx_ui_check_look_t check_default = {
    .mark = 1, .focused = 1, .disabled = 2, .sign = 1, .inside = DGX_UI_PAPER, .pressed = 2, .ink = 1, .ink_disabled = 2, .paper = DGX_UI_PAPER};
const dgx_ui_look_t dgx_ui_look_radio    = {.draw = dgx_ui_draw_radio, .params = &check_default, .fill = -1};
const dgx_ui_look_t dgx_ui_look_checkbox = {.draw = dgx_ui_draw_checkbox, .params = &check_default, .fill = -1};
