/*
 * dgx_ui_control_looks.c
 *
 *  The looks of a switch and of a slider: a track and a knob, the box cut into pieces that write every pixel once.
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

/* ---- a switch: the track, the knob a square at one end ---- */

bool dgx_ui_draw_switch(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t        *n  = dgx_ui_node(ui, i);
    const dgx_ui_switch_look_t *p  = look->params;
    const bool                  on = dgx_ui_value_of(n) != 0;
    static bool                 told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
    dgx_ui_tell_no_place(i, n->widget, "a switch", &told);
    const uint32_t track = dgx_ui_color(paint, on ? p->on : p->off);
    const unsigned bits  = n->state.bits;
    const uint32_t knob  = dgx_ui_color(paint, (bits & DGX_UI_DISABLED) ? p->disabled : (bits & DGX_UI_FOCUSED) ? p->focused : p->knob);
    const int      k     = n->h < n->w / 2 ? n->h : n->w / 2; /* a square knob */
    const int      kx    = on ? n->x + n->w - k : n->x;
    fill(paint, n->x, n->y, kx - n->x, n->h, track);                  /* left of the knob */
    fill(paint, kx, n->y, k, k, knob);                                /* the knob */
    fill(paint, kx, n->y + k, k, n->h - k, track);                    /* under it, when the box is taller */
    fill(paint, kx + k, n->y, n->x + n->w - kx - k, n->h, track);     /* right of it */
    return false;
}

static const dgx_ui_switch_look_t switch_default = {.off = 2, .on = 3, .knob = 1, .focused = 1, .disabled = 2};
const dgx_ui_look_t               dgx_ui_look_switch = {.draw = dgx_ui_draw_switch, .params = &switch_default, .fill = -1};

/* ---- a slider: paper above and below a track in the middle, filled up to the knob ---- */


bool dgx_ui_draw_slider(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t        *n = dgx_ui_node(ui, i);
    const dgx_ui_widget_t      *w = n->widget;
    const dgx_ui_slider_look_t *p = look->params;
    static bool                 told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
    dgx_ui_tell_no_place(i, w, "a slider", &told);
    const uint32_t track = dgx_ui_color(paint, p->track), filled = dgx_ui_color(paint, p->fill);
    const unsigned bits  = n->state.bits;
    const uint32_t knob  = dgx_ui_color(paint, (bits & DGX_UI_DISABLED) ? p->disabled : (bits & DGX_UI_FOCUSED) ? p->focused : p->knob);
    int            kw    = DGX_UI_KNOB * ui->unit, th = p->track_h * ui->unit;
    if (kw > n->w) kw = n->w;
    if (th > n->h) th = n->h;
    const int span = w->max > w->min ? w->max - w->min : 1, at = (n->w - kw) * (dgx_ui_value_of(n) - w->min) / span;
    const int kx = n->x + (at < 0 ? 0 : at > n->w - kw ? n->w - kw : at), ty = n->y + (n->h - th) / 2;
    /* left of the knob: paper, the filled track, paper; the knob whole height; right of it: paper, the track, paper */
    dgx_ui_lay_paper(paint, DGX_UI_PAPER, n->x, n->y, kx - n->x, ty - n->y);
    fill(paint, n->x, ty, kx - n->x, th, filled);
    dgx_ui_lay_paper(paint, DGX_UI_PAPER, n->x, ty + th, kx - n->x, n->y + n->h - ty - th);
    fill(paint, kx, n->y, kw, n->h, knob);
    const int rx = kx + kw, rw = n->x + n->w - rx;
    dgx_ui_lay_paper(paint, DGX_UI_PAPER, rx, n->y, rw, ty - n->y);
    fill(paint, rx, ty, rw, th, track);
    dgx_ui_lay_paper(paint, DGX_UI_PAPER, rx, ty + th, rw, n->y + n->h - ty - th);
    return false;
}

static const dgx_ui_slider_look_t slider_default = {.track = 2, .fill = 3, .knob = 1, .focused = 1, .disabled = 2, .track_h = 2};
const dgx_ui_look_t               dgx_ui_look_slider = {.draw = dgx_ui_draw_slider, .params = &slider_default, .fill = -1};
