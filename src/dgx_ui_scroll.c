/*
 * dgx_ui_scroll.c
 *
 *  A scrolling box's behaviour: a BOX of a column whose children may be taller than it, its offset in pixels in its
 *  place; its pad above the first child and below the last goes up and down with them, at the sides it stays. The layout lays the column at the offset (the behaviour's scroll()), the children carried marked by
 *  nothing; the finger drags the children or the knob of its bar, taps on the bar step or page; the keys scroll a box
 *  of text alone; the focus is kept in view. Its parameters described, for an appearance. The look that draws it is
 *  dgx_ui_scroll_look.c. A program that names it not carries none of it: the core reaches it through its look and two
 *  pointers set by its first layout.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <limits.h>

#include "dgx_ui_appearance.h"
#include "dgx_ui_scroll_priv.h"
#include "esp_log.h"

static const char *TAG = "dgx_ui";

const dgx_ui_scroll_look_t dgx_ui_scroll_default = {.track = 3, .knob = 1, .focused = DGX_UI_PAPER, .arrows = 1, .bar = 5};

/* ---- the parts of a box ---- */

const dgx_ui_scroll_look_t *dgx_ui_scroll_params(const dgx_ui_node_t *n)
{
    const dgx_ui_look_t *look = n->widget->look;
    return look && look->params ? look->params : &dgx_ui_scroll_default;
}

void dgx_ui_scroll_measure(const dgx_ui_t *ui, int i, dgx_ui_scroll_box_t *b)
{
    const dgx_ui_node_t *n    = dgx_ui_at(ui, i);
    const int            pad  = dgx_ui_pad(ui, n), view = n->h > 0 ? n->h : 0;
    int                  top  = INT_MAX, bottom = INT_MIN;
    for (int c = n->child; c >= 0; c = dgx_ui_at(ui, c)->next) {
        const dgx_ui_node_t *k = dgx_ui_at(ui, c);
        if (k->state.bits & DGX_UI_HIDDEN) continue;
        if (k->y < top) top = k->y;
        if (k->y + k->h > bottom) bottom = k->y + k->h;
    }
    /* the pad above the first child and below the last part of the content, gone up or down with it; at the sides it stays */
    b->rows    = dgx_ui_rect(n->x, n->y, n->w, view);
    b->content = top <= bottom ? bottom - top + 2 * pad : 0;
    b->offset  = top <= bottom ? n->y + pad - top : 0;
    b->most    = b->content > view ? b->content - view : 0;
    b->bar     = b->most ? dgx_ui_scroll_params(n)->bar * ui->unit : 0;
    if (b->bar > n->w - 2 * pad) b->bar = n->w - 2 * pad > 0 ? n->w - 2 * pad : 0;
    b->bar_x  = n->x + n->w - pad - b->bar;
    b->column = dgx_ui_rect(n->x, b->rows.y, b->bar ? b->bar_x - n->x : n->w, view);
}

/* ---- the layout, the finger, the keys ---- */

static void follow(dgx_ui_t *ui, int i);
static bool take(dgx_ui_t *ui, int x, int y);

/* whether i is a or under it */
static bool within(const dgx_ui_t *ui, int i, int a)
{
    for (; i >= 0; i = dgx_ui_at(ui, i)->parent)
        if (i == a) return true;
    return false;
}

/* a widget under the box takes the focus: the box does not then */
static bool busy(const dgx_ui_t *ui, int i)
{
    for (int j = 0; j < ui->count; ++j)
        if (j != i && dgx_ui_widget(ui, j) && within(ui, j, i) && dgx_ui_focusable(ui, j)) return true;
    return false;
}

/* the box's children and all under them gone up or down by dy, marked by nothing: only carried */
static void carry(dgx_ui_t *ui, int i, int dy)
{
    for (int c = dgx_ui_at(ui, i)->child; c >= 0; c = dgx_ui_at(ui, c)->next) {
        dgx_ui_at(ui, c)->y = (int16_t)(dgx_ui_at(ui, c)->y + dy);
        carry(ui, c, dy);
    }
}

/* the box's children laid: carried back to the top of the column first, so that a child where it was is marked by
   nothing, laid by the core with the bar's column of the content as it was, laid again when the content now asks for
   another; the offset the place's, kept within 0..content - view and written back when it was not, which the person
   did not do: nobody told; the children carried to it */
static void lay(dgx_ui_t *ui, int i)
{
    dgx_ui_scroll_follow = follow, dgx_ui_scroll_take = take, dgx_ui_scroll_busy = busy;
    dgx_ui_scroll_box_t b;
    dgx_ui_scroll_measure(ui, i, &b);
    if (b.offset) carry(ui, i, b.offset);
    const int bar = b.bar;
    dgx_ui_lay_children(ui, i, bar);
    dgx_ui_scroll_measure(ui, i, &b);
    if (b.bar != bar) dgx_ui_lay_children(ui, i, b.bar), dgx_ui_scroll_measure(ui, i, &b); /* the bar came or went */
    int32_t *at = dgx_ui_at(ui, i)->widget->value;
    if (!at) {
        static bool told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
        if (!told) ESP_LOGW(TAG, "widget %d: a scrolling box of no place (value): it does not scroll", i);
        told = true;
        return;
    }
    const int32_t offset = *at < 0 ? 0 : *at > b.most ? b.most : *at;
    if (*at != offset) *at = offset;
    if (offset) carry(ui, i, -offset);
}

/* the offset set as the person set it: within its bounds, on() told, laid again */
static void scroll_to(dgx_ui_t *ui, int i, const dgx_ui_scroll_box_t *b, int to)
{
    to = to < 0 ? 0 : to > b->most ? b->most : to;
    if (!dgx_ui_at(ui, i)->widget->value || to == b->offset) return;
    dgx_ui_user_value(ui, i, to);
    ui->needs_layout = true;
}

/* how far a step goes: the widget's step in points; else to the edge of the next child that comes in, no more than the
   view; else an eighth of it */
static int step_of(const dgx_ui_t *ui, int i, const dgx_ui_scroll_box_t *b, int way)
{
    const dgx_ui_node_t *n = dgx_ui_at(ui, i);
    if (n->widget->step > 0) return n->widget->step * ui->unit;
    int best = 0;
    for (int c = n->child; c >= 0; c = dgx_ui_at(ui, c)->next) {
        const dgx_ui_node_t *k = dgx_ui_at(ui, c);
        if (k->state.bits & DGX_UI_HIDDEN) continue;
        const int by = way > 0 ? k->y + k->h - (b->rows.y + b->rows.h) : b->rows.y - k->y;
        if (by > 0 && (!best || by < best)) best = by;
    }
    return best > 0 && best <= b->rows.h ? best : b->rows.h / 8 > 0 ? b->rows.h / 8 : 1;
}

/* the focus scrolled out of the box's view: to the first widget wholly in view that takes it, from the side the
   children came in, the bottom when they went up */
static void focus_in_view(dgx_ui_t *ui, int i, int went)
{
    dgx_ui_layout(ui);
    const int f = ui->focus;
    if (f < 0 || f == i || !within(ui, f, i)) return;
    dgx_ui_scroll_box_t b;
    dgx_ui_scroll_measure(ui, i, &b);
    const dgx_ui_node_t *k = dgx_ui_at(ui, f);
    if (k->y >= b.rows.y && k->y + k->h <= b.rows.y + b.rows.h) return;
    int pick = -1;
    for (int j = 0; j < ui->count; ++j) {
        if (j == i || !within(ui, j, i) || !dgx_ui_focusable(ui, j)) continue;
        const dgx_ui_node_t *m = dgx_ui_at(ui, j);
        if (m->y < b.rows.y || m->y + m->h > b.rows.y + b.rows.h) continue;
        if (pick < 0 || (went > 0 ? m->y > dgx_ui_at(ui, pick)->y : m->y < dgx_ui_at(ui, pick)->y)) pick = j;
    }
    if (pick >= 0) dgx_ui_focus(ui, pick);
}

void dgx_ui_scroll_bar(const dgx_ui_scroll_look_t *p, const dgx_ui_scroll_box_t *b, int *ah, int *ky, int *kh)
{
    *ah            = p->arrows != DGX_UI_PAPER && b->rows.h >= 4 * b->bar ? (b->bar + 1) / 2 : 0;
    const int th   = b->rows.h - 2 * *ah;
    int       knob = b->content > 0 ? (int)((int64_t)th * b->rows.h / b->content) : th;
    if (knob < 2) knob = th < 2 ? th : 2;
    *kh = knob;
    *ky = *ah + (b->most ? (int)((int64_t)(th - knob) * b->offset / b->most) : 0);
}

/* the finger moved while it holds the box: the children dragged, or the knob, once it went 4 points */
static bool moved(dgx_ui_t *ui, int i, int x, int y)
{
    (void)x, (void)y;
    dgx_ui_scroll_box_t b;
    dgx_ui_scroll_measure(ui, i, &b);
    const int dy = ui->move_y - ui->press_y;
    if (!ui->dragged && (dy < 0 ? -dy : dy) < 4 * ui->unit) return false;
    if (b.bar && ui->press_x >= b.bar_x) {
        int ah, ky, kh;
        dgx_ui_scroll_bar(dgx_ui_scroll_params(dgx_ui_at(ui, i)), &b, &ah, &ky, &kh);
        const int th = b.rows.h - 2 * ah - kh;
        scroll_to(ui, i, &b, th > 0 ? ui->press_value + (int)((int64_t)dy * b.most / th) : ui->press_value);
    } else {
        scroll_to(ui, i, &b, ui->press_value - dy);
    }
    return true;
}

/* let go: a tap on the bar, by an end (its arrow, or half a step) a step, on the track a page; the focus kept in view */
static void up(dgx_ui_t *ui, int i, int x, int y, bool click)
{
    (void)x, (void)y;
    dgx_ui_scroll_box_t b;
    dgx_ui_scroll_measure(ui, i, &b);
    const int was = b.offset;
    if (click && b.bar && ui->press_x >= b.bar_x) {
        int ah, ky, kh;
        dgx_ui_scroll_bar(dgx_ui_scroll_params(dgx_ui_at(ui, i)), &b, &ah, &ky, &kh);
        const int at = ui->press_y - b.rows.y, half = step_of(ui, i, &b, 1) / 2, end = ah > half ? ah : half;
        if (at < end) scroll_to(ui, i, &b, b.offset - step_of(ui, i, &b, -1));
        else if (at >= b.rows.h - end) scroll_to(ui, i, &b, b.offset + step_of(ui, i, &b, 1));
        else if (at < ky) scroll_to(ui, i, &b, b.offset - b.rows.h);
        else if (at >= ky + kh) scroll_to(ui, i, &b, b.offset + b.rows.h);
    }
    if (dgx_ui_value_of(dgx_ui_at(ui, i)) != was || ui->press_value != was) focus_in_view(ui, i, dgx_ui_value_of(dgx_ui_at(ui, i)) - ui->press_value);
}

/* the keys while the box has the focus (none under it takes it): next and previous scroll a step, past an end the
   focus goes on */
static bool key(dgx_ui_t *ui, int i, int k)
{
    if (k != DGX_UI_KEY_NEXT && k != DGX_UI_KEY_PREV) return false;
    dgx_ui_scroll_box_t b;
    dgx_ui_scroll_measure(ui, i, &b);
    const int way = k == DGX_UI_KEY_NEXT ? 1 : -1;
    if (way > 0 ? b.offset >= b.most : b.offset <= 0) return false;
    scroll_to(ui, i, &b, b.offset + way * step_of(ui, i, &b, way));
    return true;
}

const dgx_ui_behavior_t dgx_ui_scroll_behavior = {
    .flags = DGX_UI_BEHAVIOR_FOCUS | DGX_UI_BEHAVIOR_ALONE, .move = moved, .up = up, .key = key, .scroll = lay};

/* the boxes above i, the nearest first, each moved the least so that i is wholly in its view */
static void follow(dgx_ui_t *ui, int i)
{
    for (int a = dgx_ui_at(ui, i)->parent; a >= 0; a = dgx_ui_at(ui, a)->parent) {
        const dgx_ui_widget_t *w = dgx_ui_at(ui, a)->widget;
        if (!(w->flags & DGX_UI_SCROLL) || !w->look || w->look->behavior != &dgx_ui_scroll_behavior) continue;
        dgx_ui_layout(ui);
        dgx_ui_scroll_box_t b;
        dgx_ui_scroll_measure(ui, a, &b);
        const dgx_ui_node_t *k = dgx_ui_at(ui, i);
        const int            below = k->y + k->h - (b.rows.y + b.rows.h), above = b.rows.y - k->y;
        if (above > 0) scroll_to(ui, a, &b, b.offset - above);
        else if (below > 0) scroll_to(ui, a, &b, b.offset + (below < -above ? below : -above)); /* taller than the view: its top at the top */
    }
    dgx_ui_layout(ui);
}

void dgx_ui_scroll_to(dgx_ui_t *ui, int i)
{
    if (dgx_ui_widget(ui, i)) follow(ui, i);
}

/* the finger pressed on a widget under a scrolling box went up or down first, by 4 points or half the widget: the press
   the box's, the widget let go as it was, its value given back; not a widget that takes such a move itself */
static bool take(dgx_ui_t *ui, int x, int y)
{
    const int p = ui->pressed;
    if (ui->dragged || p < 0) return false;
    dgx_ui_node_t         *n = dgx_ui_at(ui, p);
    const dgx_ui_widget_t *w = n->widget;
    if ((w->flags & DGX_UI_SCROLL) || w->kind == DGX_UI_LIST || (w->kind == DGX_UI_BUTTON && (w->flags & (DGX_UI_DRAG | DGX_UI_REPEAT))) ||
        (w->kind == DGX_UI_OWN && w->look && w->look->behavior && w->look->behavior->move))
        return false;
    int a = n->parent;
    while (a >= 0 && !(dgx_ui_at(ui, a)->widget->flags & DGX_UI_SCROLL && dgx_ui_at(ui, a)->widget->look &&
                       dgx_ui_at(ui, a)->widget->look->behavior == &dgx_ui_scroll_behavior))
        a = dgx_ui_at(ui, a)->parent;
    if (a < 0) return false;
    const int dx = x - ui->press_x, dy = y - ui->press_y, ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    int       least = 4 * ui->unit;
    if (n->h / 2 < least) least = n->h / 2 > 0 ? n->h / 2 : 1;
    if (ay < least || ay <= ax) return false;
    if (w->value && w->kind != DGX_UI_RADIO && dgx_ui_value_of(n) != ui->press_value) dgx_ui_user_value(ui, p, ui->press_value);
    n = dgx_ui_at(ui, p);
    if (n->widget) n->state.bits &= (uint8_t)~DGX_UI_PRESSED, dgx_ui_touched(ui, p);
    ui->pressed = (int16_t)a, ui->press_value = dgx_ui_value_of(dgx_ui_at(ui, a)), ui->dragged = true;
    const dgx_ui_node_t *box = dgx_ui_at(ui, a);
    moved(ui, a, x - box->x, y - box->y);
    return true;
}

/* ---- the parameters, described for an appearance ---- */

static const dgx_ui_field_t scroll_fields[] = {DGX_UI_COLOR_FIELD(dgx_ui_scroll_look_t, track), DGX_UI_COLOR_FIELD(dgx_ui_scroll_look_t, knob),
                                               DGX_UI_COLOR_OR_UNDER_FIELD(dgx_ui_scroll_look_t, focused), DGX_UI_COLOR_OR_UNDER_FIELD(dgx_ui_scroll_look_t, arrows),
                                               DGX_UI_NUMBER_FIELD(dgx_ui_scroll_look_t, bar, 0, 32)};
const dgx_ui_params_t dgx_ui_params_scroll = DGX_UI_PARAMS_BEHAVING(dgx_ui_scroll_look_t, 1u << DGX_UI_BOX, scroll_fields, &dgx_ui_scroll_behavior);
