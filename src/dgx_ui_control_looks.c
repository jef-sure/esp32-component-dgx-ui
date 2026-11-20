/*
 * dgx_ui_control_looks.c
 *
 *  The looks of a switch and of a slider: a track and a knob, the box cut into pieces that write every pixel once. The
 *  focus is on the knob: hollow, a border of a point in the colour `focused` round the track's colour (a switch's) or
 *  the paper (a slider's), where without the focus it is solid; parameters whose `focused` is DGX_UI_PAPER keep it
 *  solid, no focus shown. A finger on the widget widens it by a point (a switch's toward its middle). The track keeps
 *  its length and its colours.
 *  Each is a look of its own; a program that names neither carries neither, as the linker keeps of an archive only
 *  what is named (each in a section of its own: -ffunction-sections).
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_ui_paint_priv.h"
#include "esp_log.h"

static const char TAG[] = "dgx_ui";

void dgx_ui_tell_no_place(int i, const dgx_ui_widget_t *w, const char *kind, bool *told)
{
    if (w->value || *told) return;
    *told = true;
    ESP_LOGW(TAG, "widget %d (tag %d, %s) has no place for its value: give it .value = &a variable of the program's", i, w->tag, kind);
}

static void fill(dgx_ui_paint_t *paint, int x, int y, int w, int h, uint32_t c)
{
    if (w > 0 && h > 0) dgx_fill_rectangle(paint->scr, x - paint->ox, y - paint->oy, w, h, c);
}

/* a knob: solid of `knob`, or with the focus hollow, a border of a point in `border` round `inside` (a colour), or round
   the paper laid when `paper` */
static void draw_knob(dgx_ui_paint_t *paint, int x, int y, int w, int h, int point, bool focused, uint32_t knob, uint32_t border,
                      uint32_t inside, bool paper)
{
    if (!focused) {
        fill(paint, x, y, w, h, knob);
        return;
    }
    if (w <= 2 * point || h <= 2 * point) {
        fill(paint, x, y, w, h, border);
        return;
    }
    fill(paint, x, y, w, point, border);
    fill(paint, x, y + h - point, w, point, border);
    fill(paint, x, y + point, point, h - 2 * point, border);
    fill(paint, x + w - point, y + point, point, h - 2 * point, border);
    if (paper) dgx_ui_lay_paper(paint, DGX_UI_PAPER, x + point, y + point, w - 2 * point, h - 2 * point);
    else fill(paint, x + point, y + point, w - 2 * point, h - 2 * point, inside);
}

/* ---- a switch: the track, the knob a square at one end ---- */

bool dgx_ui_draw_switch(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t        *n = dgx_ui_node(ui, i);
    const dgx_ui_switch_look_t *p = look->params;
    static bool                 told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
    dgx_ui_tell_no_place(i, n->widget, "a switch", &told);
    const bool     on    = dgx_ui_value_of(n) != 0;
    const uint32_t track = dgx_ui_color(paint, on ? p->on : p->off);
    const unsigned bits  = dgx_ui_bits_shown(ui, n);
    const uint32_t knob  = dgx_ui_color(paint, (bits & DGX_UI_DISABLED) ? p->disabled : p->knob);
    const int      k     = n->h < n->w / 2 ? n->h : n->w / 2; /* a square knob */
    int            kw    = k;
    if ((bits & DGX_UI_PRESSED) && kw + ui->unit <= n->w) kw += ui->unit; /* a finger on it: a point wider, toward the middle */
    const int kx = on ? n->x + n->w - kw : n->x;
    fill(paint, n->x, n->y, kx - n->x, n->h, track);                  /* left of the knob */
    const bool hollow = (bits & (DGX_UI_FOCUSED | DGX_UI_DISABLED)) == DGX_UI_FOCUSED && p->focused != DGX_UI_PAPER; /* PAPER: no focus shown */
    draw_knob(paint, kx, n->y, kw, k, ui->unit, hollow, knob, dgx_ui_color(paint, p->focused), track, false);
    fill(paint, kx, n->y + k, kw, n->h - k, track);                   /* under it, when the box is taller */
    fill(paint, kx + kw, n->y, n->x + n->w - kx - kw, n->h, track);   /* right of it */
    return false;
}

static const dgx_ui_switch_look_t switch_default = {.off = 2, .on = 3, .knob = 1, .focused = 1, .disabled = 2};
const dgx_ui_look_t               dgx_ui_look_switch = {.draw = dgx_ui_draw_switch, .params = &switch_default, .fill = -1};

/* ---- a slider: paper above and below a track in the middle, filled up to the knob ---- */


bool dgx_ui_draw_slider(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t        *n = dgx_ui_node(ui, i);
    const dgx_ui_slider_look_t *p = look->params;
    static bool                 told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
    dgx_ui_tell_no_place(i, n->widget, "a slider", &told);
    const dgx_ui_widget_t *w     = n->widget;
    const uint32_t         track = dgx_ui_color(paint, p->track), filled = dgx_ui_color(paint, p->fill);
    const unsigned         bits  = dgx_ui_bits_shown(ui, n);
    const uint32_t         knob  = dgx_ui_color(paint, (bits & DGX_UI_DISABLED) ? p->disabled : p->knob);
    int                    kw    = DGX_UI_KNOB * ui->unit, th = p->track_h * ui->unit;
    if (kw > n->w) kw = n->w;
    if (th > n->h) th = n->h;
    const int span = w->max > w->min ? w->max - w->min : 1, at = (n->w - kw) * (dgx_ui_value_of(n) - w->min) / span;
    int       kx = n->x + (at < 0 ? 0 : at > n->w - kw ? n->w - kw : at);
    const int ty = n->y + (n->h - th) / 2;
    if (bits & DGX_UI_PRESSED) {
        /* a finger on it: the knob a point wider on each side, kept within the box */
        const int left = DGX_MAX(kx - ui->unit, n->x), right = DGX_MIN(kx + kw + ui->unit, n->x + n->w);
        kx = left, kw = right - left;
    }
    /* left of the knob: paper, the filled track, paper; the knob whole height; right of it: paper, the track, paper */
    dgx_ui_lay_paper(paint, DGX_UI_PAPER, n->x, n->y, kx - n->x, ty - n->y);
    fill(paint, n->x, ty, kx - n->x, th, filled);
    dgx_ui_lay_paper(paint, DGX_UI_PAPER, n->x, ty + th, kx - n->x, n->y + n->h - ty - th);
    const bool hollow = (bits & (DGX_UI_FOCUSED | DGX_UI_DISABLED)) == DGX_UI_FOCUSED && p->focused != DGX_UI_PAPER; /* PAPER: no focus shown */
    draw_knob(paint, kx, n->y, kw, n->h, ui->unit, hollow, knob, dgx_ui_color(paint, p->focused), 0, true);
    const int rx = kx + kw, rw = n->x + n->w - rx;
    dgx_ui_lay_paper(paint, DGX_UI_PAPER, rx, n->y, rw, ty - n->y);
    fill(paint, rx, ty, rw, th, track);
    dgx_ui_lay_paper(paint, DGX_UI_PAPER, rx, ty + th, rw, n->y + n->h - ty - th);
    return false;
}

static const dgx_ui_slider_look_t slider_default = {.track = 2, .fill = 3, .knob = 1, .focused = 1, .disabled = 2, .track_h = 2};
const dgx_ui_look_t               dgx_ui_look_slider = {.draw = dgx_ui_draw_slider, .params = &slider_default, .fill = -1};
