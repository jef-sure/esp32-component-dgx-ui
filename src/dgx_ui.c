/*
 * dgx_ui.c
 *
 *  The core of dgx-ui: the tree in the program's array and in blocks, links as in the DOM, the layout in rows and
 *  columns, the state of the widgets, the pointer, the keys and the focus. It draws nothing and calls no function of
 *  DGX. The pointer, the keys and the focus are those of DGX-GE's widgets, told by number and kept apart from what
 *  the program sets: on() hears what the user did only.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <stdlib.h>
#include <string.h>

#include "dgx_ui_priv.h"
#include "esp_log.h"

static const char *TAG = "dgx_ui";


/* ---- the slots ---- */

dgx_ui_node_t *dgx_ui_node(const dgx_ui_t *ui, int i)
{
    if (i < 0 || i >= ui->count) return NULL;
    if (i < ui->first_size) return &ui->first[i];
    const int b = (i - ui->first_size) / DGX_UI_BLOCK;
    return b < ui->block_count ? &ui->blocks[b][(i - ui->first_size) % DGX_UI_BLOCK] : NULL;
}

dgx_ui_node_t *dgx_ui_at(const dgx_ui_t *ui, int i)
{
    if (i < ui->first_size) return &ui->first[i];
    const unsigned k = (unsigned)(i - ui->first_size);
    return &ui->blocks[k / DGX_UI_BLOCK][k % DGX_UI_BLOCK];
}

void dgx_ui_touched(dgx_ui_t *ui, int i)
{
    for (int p = dgx_ui_at(ui, i)->parent; p >= 0; p = dgx_ui_at(ui, p)->parent) dgx_ui_at(ui, p)->marks |= DGX_UI_MARK_BELOW;
}

/* drawn by a look: its own, else its kind's, as the painter has it (dgx_ui_look_of()) */
static bool drawn(const dgx_ui_widget_t *w)
{
    return w->look || w->kind == DGX_UI_BOX || w->kind == DGX_UI_FRAME || w->kind == DGX_UI_LABEL;
}

/* a child of p moved, came or went: the nearest widget from p up that a look draws fills around its children, as one
   drawn by none fills nothing */
static void around(dgx_ui_t *ui, int p)
{
    while (p >= 0 && !drawn(dgx_ui_at(ui, p)->widget)) p = dgx_ui_at(ui, p)->parent;
    if (p < 0) return;
    dgx_ui_at(ui, p)->marks |= DGX_UI_MARK_AROUND;
    dgx_ui_touched(ui, p);
}

/* a node that holds a widget: not a free slot */
static dgx_ui_node_t *live(const dgx_ui_t *ui, int i)
{
    dgx_ui_node_t *n = dgx_ui_node(ui, i);
    return n && n->widget ? n : NULL;
}

const dgx_ui_widget_t *dgx_ui_widget(const dgx_ui_t *ui, int i)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    return n ? n->widget : NULL;
}

/* the node of a number known to be in the tree */
#define NODE(ui, i) dgx_ui_at((ui), (i)) /* a number known to hold one */

void dgx_ui_init(dgx_ui_t *ui, dgx_ui_node_t *first, int first_size, int unit)
{
    memset(ui, 0, sizeof(*ui));
    ui->first      = first;
    ui->first_size = (int16_t)(first && first_size > 0 ? (first_size > 32767 ? 32767 : first_size) : 0);
    ui->unit       = (uint8_t)(unit < 1 ? 1 : unit > 255 ? 255 : unit);
    ui->focus = ui->pressed = -1;
    ui->repeat_key   = -1;
    ui->needs_layout = true;
}

void dgx_ui_free(dgx_ui_t *ui)
{
    for (int b = 0; b < ui->block_count; ++b) free(ui->blocks[b]);
    free(ui->blocks);
    dgx_ui_init(ui, ui->first, ui->first_size, ui->unit);
}

/* a slot for a new widget: a free one, else the next, in a new block when the last is full; -1 no memory */
static int new_slot(dgx_ui_t *ui)
{
    for (int i = 0; i < ui->count; ++i)
        if (!NODE(ui, i)->widget) return i;
    if (ui->count == 32767) return -1;
    const int i = ui->count;
    if (i >= ui->first_size && (i - ui->first_size) / DGX_UI_BLOCK >= ui->block_count) {
        dgx_ui_node_t **blocks = realloc(ui->blocks, (size_t)(ui->block_count + 1) * sizeof *blocks);
        if (!blocks) return -1;
        ui->blocks = blocks;
        if (!(blocks[ui->block_count] = calloc(DGX_UI_BLOCK, sizeof(dgx_ui_node_t)))) return -1;
        ++ui->block_count;
    }
    ++ui->count;
    return i;
}

int dgx_ui_insert(dgx_ui_t *ui, int parent, int before, const dgx_ui_widget_t *w)
{
    if (!w) return -1;
    const bool root = !live(ui, 0) || ui->count == 0;
    if (!root && !live(ui, parent)) return -1;
    if (root) parent = -1;
    const int i = root ? (ui->count ? 0 : new_slot(ui)) : new_slot(ui);
    if (i < 0) {
        ESP_LOGE(TAG, "no memory for a block of %d widgets", DGX_UI_BLOCK);
        return -1;
    }
    dgx_ui_node_t *n = NODE(ui, i);
    memset(n, 0, sizeof(*n));
    n->widget          = w;
    n->parent          = (int16_t)parent;
    n->child = n->next = -1;
    n->marks           = DGX_UI_MARK_WHOLE | DGX_UI_MARK_FIT;
    n->state_value     = w->state;
    n->state_value.bits &= (uint8_t)~(DGX_UI_FOCUSED | DGX_UI_PRESSED);
    n->state_drawn = n->state_value;
    if (parent >= 0) {
        /* linked before `before` among the parent's children, or as the last */
        dgx_ui_node_t *p  = NODE(ui, parent);
        int16_t       *at = &p->child;
        while (*at >= 0 && *at != before) at = &NODE(ui, *at)->next;
        n->next = *at;
        *at     = (int16_t)i; /* drawn when laid: place() marks it and those above it */
    }
    ui->needs_layout = true;
    return i;
}

int dgx_ui_add(dgx_ui_t *ui, int parent, const dgx_ui_widget_t *w)
{
    return dgx_ui_insert(ui, parent, -1, w);
}

/* whether i is a or under it */
static bool within(const dgx_ui_t *ui, int i, int a)
{
    for (; i >= 0; i = NODE(ui, i)->parent)
        if (i == a) return true;
    return false;
}

static void free_subtree(dgx_ui_t *ui, int i)
{
    dgx_ui_node_t *n = NODE(ui, i);
    for (int c = n->child; c >= 0;) {
        const int next = NODE(ui, c)->next;
        free_subtree(ui, c);
        c = next;
    }
    memset(n, 0, sizeof(*n)); /* widget NULL: a free slot */
    n->parent = n->child = n->next = -1;
}

void dgx_ui_remove(dgx_ui_t *ui, int i)
{
    dgx_ui_node_t *n = live(ui, i);
    if (!n) return;
    if (ui->focus >= 0 && within(ui, ui->focus, i)) ui->focus = -1;
    if (ui->pressed >= 0 && within(ui, ui->pressed, i)) ui->pressed = -1;
    if (n->parent >= 0) {
        dgx_ui_node_t *p = NODE(ui, n->parent);
        around(ui, n->parent); /* the place it leaves, filled around the children that stay */
        int16_t *at = &p->child;
        while (*at != i) at = &NODE(ui, *at)->next;
        *at = n->next;
    }
    free_subtree(ui, i);
    while (ui->count > 0 && !NODE(ui, ui->count - 1)->widget) --ui->count;
    ui->needs_layout = true;
}

/* the next widget of the tree after i in its order (parents before children), -1 at the end; below: stays under it */
static int preorder_next(const dgx_ui_t *ui, int i, int below)
{
    const dgx_ui_node_t *n = NODE(ui, i);
    if (n->child >= 0) return n->child;
    for (; i >= 0 && i != below; i = NODE(ui, i)->parent) {
        const int next = NODE(ui, i)->next;
        if (next >= 0) return next;
    }
    return -1;
}

int dgx_ui_find(const dgx_ui_t *ui, int parent, int tag)
{
    const int from = parent < 0 ? 0 : parent;
    if (!live(ui, from)) return -1;
    for (int i = parent < 0 ? 0 : preorder_next(ui, from, from); i >= 0; i = preorder_next(ui, i, from))
        if (NODE(ui, i)->widget->tag == tag) return i;
    return -1;
}

/* ---- the layout ---- */

void dgx_ui_set_root(dgx_ui_t *ui, int x, int y, int w, int h)
{
    ui->root_given   = true;
    ui->root_rect    = dgx_ui_rect(x, y, w, h);
    ui->needs_layout = true;
}

void dgx_ui_relayout(dgx_ui_t *ui)
{
    ui->needs_layout = true;
}

void dgx_ui_screen(dgx_ui_t *ui, int width, int height, int turn)
{
    if (width == ui->screen_w && height == ui->screen_h && (uint8_t)turn == ui->screen_turn) return;
    ui->screen_w = (int16_t)width, ui->screen_h = (int16_t)height, ui->screen_turn = (uint8_t)turn;
    ui->turned = ui->needs_layout = true;
}

bool dgx_ui_hidden(const dgx_ui_t *ui, int i)
{
    for (; i >= 0; i = NODE(ui, i)->parent)
        if (NODE(ui, i)->state_value.bits & DGX_UI_HIDDEN) return true;
    return false;
}

/* the size a widget asks for, in pixels: its own, else its content's; LABEL has no width of its content */
static int want_w(const dgx_ui_t *ui, const dgx_ui_widget_t *w)
{
    int pw, ph;
    if (w->want_w) return w->want_w * ui->unit;
    if (w->picture && (w->kind == DGX_UI_PICTURE || w->kind == DGX_UI_ICON)) return dgx_ui_picture_size(w->picture, &pw, &ph), pw;
    return 0;
}

static int want_h(const dgx_ui_t *ui, const dgx_ui_widget_t *w)
{
    int pw, ph;
    if (w->want_h) return w->want_h * ui->unit;
    if (w->picture && (w->kind == DGX_UI_PICTURE || w->kind == DGX_UI_ICON)) return dgx_ui_picture_size(w->picture, &pw, &ph), ph;
    if (w->text) return dgx_ui_textbox_height(w->text);
    return 0;
}

/* a widget laid in a place: drawn whole when the place changed, its parent filling around it and the others */
static void place(dgx_ui_t *ui, int i, int x, int y, int w, int h)
{
    dgx_ui_node_t *n = NODE(ui, i);
    if (n->x == x && n->y == y && n->w == w && n->h == h) return;
    if (n->w != w) n->marks |= DGX_UI_MARK_FIT;
    n->x = (int16_t)x, n->y = (int16_t)y, n->w = (int16_t)w, n->h = (int16_t)h;
    n->marks |= DGX_UI_MARK_WHOLE;
    around(ui, n->parent);
    dgx_ui_touched(ui, i);
}

static void lay(dgx_ui_t *ui, int i)
{
    const dgx_ui_node_t   *pn = NODE(ui, i);
    const dgx_ui_widget_t *p  = pn->widget;
    const int  u = ui->unit, pad = p->pad * u, gap = p->gap * u;
    const int  ix = pn->x + pad, iy = pn->y + pad, iw = pn->w - 2 * pad, ih = pn->h - 2 * pad;
    const bool column = p->flags & DGX_UI_COLUMN, row = !column && (p->flags & DGX_UI_ROW);
    /* along the stacking: what the others take, the gaps between all; the rest shared by those that take it */
    int fixed = 0, shown = 0, takers = 0;
    if (column || row)
        for (int c = pn->child; c >= 0; c = NODE(ui, c)->next) {
            const dgx_ui_widget_t *w = NODE(ui, c)->widget;
            if (NODE(ui, c)->state_value.bits & DGX_UI_HIDDEN) continue;
            ++shown;
            if (w->flags & DGX_UI_REST) ++takers;
            else fixed += column ? want_h(ui, w) : want_w(ui, w);
        }
    int left = (column ? ih : iw) - fixed - (shown > 1 ? (shown - 1) * gap : 0);
    if (left < 0) left = 0;
    const int share = takers ? left / takers : 0;
    int       along = 0, taken = 0;
    for (int c = pn->child; c >= 0; c = NODE(ui, c)->next) {
        const dgx_ui_widget_t *w = NODE(ui, c)->widget;
        if (NODE(ui, c)->state_value.bits & DGX_UI_HIDDEN) continue;
        int        ww = want_w(ui, w), wh = want_h(ui, w), x, y;
        const bool rest = w->flags & DGX_UI_REST; /* a box of neither lays it where it asks */
        const int  took = !rest ? 0 : ++taken < takers ? share : left - share * (takers - 1); /* the last: the odd pixels */
        if (column) {
            if (rest) wh = took;
            if (w->flags & DGX_UI_STRETCH || !ww) ww = iw;
            x = ix + (w->at_x >= 0 ? w->at_x * u : iw - ww + w->at_x * u), y = iy + along;
            along += wh + gap;
        } else if (row) {
            if (rest) ww = took;
            if (w->flags & DGX_UI_STRETCH || !wh) wh = ih;
            if (!ww && w->text && !rest && !ui->told_width) {
                ESP_LOGW(TAG, "widget %d of a text in a row has no width: give it want_w", c);
                ui->told_width = true;
            }
            x = ix + along, y = iy + (w->at_y >= 0 ? w->at_y * u : ih - wh + w->at_y * u);
            along += ww + gap;
        } else {
            if (!ww) ww = iw - (w->at_x >= 0 ? w->at_x * u : -w->at_x * u);
            if (!wh) wh = ih - (w->at_y >= 0 ? w->at_y * u : -w->at_y * u);
            x = w->at_x >= 0 ? ix + w->at_x * u : ix + iw + w->at_x * u - ww;
            y = w->at_y >= 0 ? iy + w->at_y * u : iy + ih + w->at_y * u - wh;
        }
        place(ui, c, x, y, ww, wh);
        lay(ui, c);
    }
}

bool dgx_ui_layout(dgx_ui_t *ui)
{
    if (!ui->needs_layout) return false;
    ui->needs_layout = false;
    if (!live(ui, 0)) return true;
    const dgx_ui_rect_t root = ui->root_given ? ui->root_rect : dgx_ui_rect(0, 0, ui->screen_w, ui->screen_h);
    place(ui, 0, root.x, root.y, root.w, root.h);
    lay(ui, 0);
    if (ui->turned) {
        for (int i = 0; i < ui->count; ++i) NODE(ui, i)->marks |= DGX_UI_MARK_WHOLE | DGX_UI_MARK_FIT;
        ui->turned = false;
    }
    return true;
}

/* ---- the state ---- */

static void unpress(dgx_ui_t *ui);

/* within min..max when there is a range; an ICON's value outside it is no picture, so it is not clamped */
static int clamp_value(const dgx_ui_widget_t *w, int value)
{
    if (w->max <= w->min || w->kind == DGX_UI_ICON) return value;
    return value < w->min ? w->min : value > w->max ? w->max : value;
}

void dgx_ui_set_value(dgx_ui_t *ui, int i, int value)
{
    dgx_ui_node_t *n = live(ui, i);
    if (!n) return;
    n->state_value.value = (int16_t)clamp_value(n->widget, value);
    dgx_ui_touched(ui, i);
}

void dgx_ui_set_state(dgx_ui_t *ui, int i, unsigned bits, bool on)
{
    dgx_ui_node_t *n = live(ui, i);
    if (!n) return;
    bits &= DGX_UI_DISABLED | DGX_UI_PRESSED;
    n->state_value.bits = (uint8_t)(on ? n->state_value.bits | bits : n->state_value.bits & ~bits);
    dgx_ui_touched(ui, i);
    if ((bits & DGX_UI_DISABLED) && on && ui->focus >= 0 && within(ui, ui->focus, i)) dgx_ui_focus(ui, -1);
    if ((bits & DGX_UI_DISABLED) && on && ui->pressed >= 0 && within(ui, ui->pressed, i)) unpress(ui); /* the press ends, told nothing */
}

void dgx_ui_show(dgx_ui_t *ui, int i, bool shown)
{
    dgx_ui_node_t *n = live(ui, i);
    if (!n || !(n->state_value.bits & DGX_UI_HIDDEN) == shown) return;
    n->state_value.bits = (uint8_t)(shown ? n->state_value.bits & ~DGX_UI_HIDDEN : n->state_value.bits | DGX_UI_HIDDEN);
    if (!shown && ui->focus >= 0 && within(ui, ui->focus, i)) dgx_ui_focus(ui, -1);
    if (!shown && ui->pressed >= 0 && within(ui, ui->pressed, i)) unpress(ui);
    around(ui, n->parent); /* where it went, or will lie: filled around the others */
    n->marks |= DGX_UI_MARK_WHOLE;
    dgx_ui_touched(ui, i);
    ui->needs_layout = true;
}

void dgx_ui_redraw(dgx_ui_t *ui, int i)
{
    dgx_ui_node_t *n = live(ui, i);
    if (!n) return;
    n->marks |= DGX_UI_MARK_WHOLE;
    dgx_ui_touched(ui, i);
}

/* ---- the pointer, the keys and the focus ---- */

static bool interactive(const dgx_ui_widget_t *w)
{
    return w->kind == DGX_UI_LIST || w->kind == DGX_UI_SWITCH || w->kind == DGX_UI_SLIDER || w->kind == DGX_UI_BUTTON ||
           w->kind == DGX_UI_INPUT || w->kind == DGX_UI_DROPDOWN;
}

/* answers the pointer and takes the focus: interactive, shown and not disabled, nor any above it */
static bool usable(const dgx_ui_t *ui, int i)
{
    const dgx_ui_node_t *n = live(ui, i);
    if (!n || !interactive(n->widget) || dgx_ui_hidden(ui, i)) return false;
    for (; i >= 0; i = NODE(ui, i)->parent)
        if (NODE(ui, i)->state_value.bits & DGX_UI_DISABLED) return false;
    return true;
}

static int depth_of(const dgx_ui_t *ui, int i)
{
    int d = 0;
    for (i = NODE(ui, i)->parent; i >= 0; i = NODE(ui, i)->parent) ++d;
    return d;
}

static bool inside(const dgx_ui_node_t *n, int x, int y)
{
    return x >= n->x && y >= n->y && x < n->x + n->w && y < n->y + n->h;
}

/* the deepest usable widget under a point in the tree from i, at depth d: down only where the point is, as a widget is
   seen only within its parent's box; nothing under a hidden or a disabled one */
typedef struct {
    int  best, depth;
    long area;
} hit_t;

static void hit_in(const dgx_ui_t *ui, int i, int d, int x, int y, hit_t *h)
{
    const dgx_ui_node_t *n = NODE(ui, i);
    if (n->state_value.bits & (DGX_UI_HIDDEN | DGX_UI_DISABLED)) return;
    const bool in = inside(n, x, y);
    if (in && interactive(n->widget)) {
        /* of two as deep (a control over a row of a list), the smaller; of two as small, the first in the tree */
        const long a = (long)n->w * n->h;
        if (d > h->depth || (d == h->depth && a < h->area)) h->best = i, h->depth = d, h->area = a;
    }
    if (!in) return;
    for (int c = n->child; c >= 0; c = NODE(ui, c)->next) hit_in(ui, c, d + 1, x, y, h);
}

int dgx_ui_hit(dgx_ui_t *ui, int x, int y)
{
    dgx_ui_layout(ui);
    hit_t h = {.best = -1, .depth = -1};
    if (live(ui, 0)) hit_in(ui, 0, 0, x, y, &h);
    return h.best;
}

static void tell(dgx_ui_t *ui, int i, int what)
{
    const dgx_ui_widget_t *w = NODE(ui, i)->widget;
    if (w->on) w->on(ui, i, NODE(ui, i), what, w->ctx);
}

void dgx_ui_focus(dgx_ui_t *ui, int i)
{
    if (i >= 0 && !usable(ui, i)) i = -1;
    if (ui->focus == i) return;
    dgx_ui_node_t *old = live(ui, ui->focus);
    if (old) old->state_value.bits &= (uint8_t)~DGX_UI_FOCUSED, dgx_ui_touched(ui, ui->focus);
    ui->focus = (int16_t)i;
    if (i >= 0) {
        NODE(ui, i)->state_value.bits |= DGX_UI_FOCUSED;
        dgx_ui_touched(ui, i);
        tell(ui, i, DGX_UI_FOCUS);
    }
}

/* a value set by the user: within its bounds, on() told when it changed */
static void user_value(dgx_ui_t *ui, int i, int value)
{
    dgx_ui_node_t *n = NODE(ui, i);
    value            = clamp_value(n->widget, value);
    if (value == n->state_value.value) return;
    n->state_value.value = (int16_t)value;
    dgx_ui_touched(ui, i);
    tell(ui, i, DGX_UI_CHANGED);
}

static int rows_of(const dgx_ui_widget_t *w)
{
    return w->max - w->min + 1;
}

static int row_height(const dgx_ui_node_t *n)
{
    const int rows = rows_of(n->widget);
    return rows > 0 ? n->h / rows : n->h;
}

/* the point of a slider or a list in its own coordinates to a value */
static void point_to_value(dgx_ui_t *ui, int i, int lx, int ly)
{
    const dgx_ui_node_t   *n = NODE(ui, i);
    const dgx_ui_widget_t *w = n->widget;
    if (w->kind == DGX_UI_SLIDER) {
        /* the knob's middle at the point: its travel is the width less the knob; a slider too narrow: the width */
        const int knob = DGX_UI_KNOB * ui->unit;
        int       span = n->w - knob, at = lx - knob / 2;
        if (span <= 0) span = n->w - 1, at = lx;
        at             = at < 0 ? 0 : at > span ? span : at;
        const int step = w->step > 0 ? w->step : 1;
        int       v    = span > 0 ? w->min + ((w->max - w->min) * at + span / 2) / span : w->min;
        v              = w->min + (v - w->min + step / 2) / step * step;
        user_value(ui, i, v);
    } else if (w->kind == DGX_UI_LIST) {
        const int rh = row_height(n);
        if (rh > 0) user_value(ui, i, w->min + ly / rh);
    }
}

/* a swiped slider: its value when the pointer went down and the way it has gone since, its width the whole range */
static void swipe_to_value(dgx_ui_t *ui, int i, int dx)
{
    const dgx_ui_node_t   *n = NODE(ui, i);
    const dgx_ui_widget_t *w = n->widget;
    if (n->w <= 0) return;
    const int32_t num  = (int32_t)dx * (w->max - w->min);
    const int     by   = (int)(num >= 0 ? (num + n->w / 2) / n->w : -((-num + n->w / 2) / n->w));
    const int     step = w->step > 0 ? w->step : 1;
    const int     v    = ui->press_value + by;
    user_value(ui, i, w->min + (v - w->min + (v >= w->min ? step / 2 : -(step / 2))) / step * step);
}

static bool swiped_slider(const dgx_ui_widget_t *w)
{
    return w->kind == DGX_UI_SLIDER && (w->flags & DGX_UI_SWIPE);
}

/* a list under a point (the deepest), -1 none */
static int list_at(dgx_ui_t *ui, int x, int y)
{
    int best = -1, depth = -1;
    for (int i = 0; i < ui->count; ++i) {
        const dgx_ui_node_t *n = live(ui, i);
        if (!n || n->widget->kind != DGX_UI_LIST || !inside(n, x, y) || !usable(ui, i)) continue;
        const int d = depth_of(ui, i);
        if (d > depth) best = i, depth = d;
    }
    return best;
}

/* a point of a list (its own y) between two of its rows: the top or bottom 1/DGX_UI_ROW_GAP of a row */
static bool between_rows(const dgx_ui_node_t *n, int ly)
{
    const int rh = row_height(n), m = rh / DGX_UI_ROW_GAP, in = rh > 0 && ly >= 0 ? ly % rh : 0;
    return n->widget->kind == DGX_UI_LIST && rh > 0 && (in < m || in >= rh - m);
}

/* pressed: the one under it, where, its value then; a slider or a list takes the point (a swiped slider not yet) */
static void press(dgx_ui_t *ui, int i, int x, int y)
{
    dgx_ui_node_t *n = NODE(ui, i);
    ui->pressed      = (int16_t)i;
    ui->press_x = ui->move_x = (int16_t)x, ui->press_y = ui->move_y = (int16_t)y, ui->dragged = false;
    ui->between = between_rows(n, y - n->y);
    n->state_value.bits |= DGX_UI_PRESSED;
    dgx_ui_touched(ui, i);
    if (!swiped_slider(n->widget) && !ui->between) point_to_value(ui, i, x - n->x, y - n->y);
    if (!live(ui, i)) { /* on(CHANGED) removed it */
        ui->pressed = -1;
        return;
    }
    ui->press_value = n->state_value.value;
}

static void unpress(dgx_ui_t *ui)
{
    dgx_ui_node_t *n = live(ui, ui->pressed);
    if (n) n->state_value.bits &= (uint8_t)~DGX_UI_PRESSED, dgx_ui_touched(ui, ui->pressed);
    ui->pressed = -1;
}

bool dgx_ui_holding(const dgx_ui_t *ui)
{
    return live(ui, ui->pressed) || (ui->popup && dgx_ui_holding(ui->popup));
}

bool dgx_ui_dragging(const dgx_ui_t *ui)
{
    if (ui->popup && dgx_ui_holding(ui->popup)) return dgx_ui_dragging(ui->popup);
    const dgx_ui_node_t *n = live(ui, ui->pressed);
    if (!n) return false;
    const dgx_ui_widget_t *w = n->widget;
    return w->kind == DGX_UI_SLIDER || w->kind == DGX_UI_LIST || (w->kind == DGX_UI_BUTTON && (w->flags & DGX_UI_DRAG));
}

static bool pointer_in(dgx_ui_t *ui, const dgx_ui_pointer_t *p);

/* whether a point lies on a popup or on one over it, which may hang out of it */
static bool on_popup(const dgx_ui_t *pop, int x, int y)
{
    for (; pop; pop = pop->popup) {
        const dgx_ui_rect_t r = pop->root_rect;
        if (x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h) return true;
    }
    return false;
}

bool dgx_ui_pointer(dgx_ui_t *ui, const dgx_ui_pointer_t *p)
{
    dgx_ui_t *pop = ui->popup;
    if (pop) {
        const bool inside = on_popup(pop, p->x, p->y);
        if (p->pressed && !inside && (ui->popup_flags & DGX_UI_POPUP_CLOSE_OUTSIDE)) {
            dgx_ui_popup_close(ui);
            return true; /* the finger that closed it is spent */
        }
        /* a finger down on the popup or on one over it is theirs or nobody's, whatever the flags: no widget they
           hide is pressed; the one on top first */
        if (p->pressed && inside) return dgx_ui_pointer(pop, p);
        if (dgx_ui_pointer(pop, p)) return true; /* held by a popup's widget, at whatever level; or one over closed */
        if (ui->popup_flags & DGX_UI_POPUP_MODAL) return true;
    }
    return pointer_in(ui, p);
}

static bool pointer_in(dgx_ui_t *ui, const dgx_ui_pointer_t *p)
{
    dgx_ui_layout(ui);
    if (p->pressed) {
        unpress(ui);
        const int i = dgx_ui_hit(ui, p->x, p->y);
        if (i < 0) return false;
        ui->press_ms = p->ms, ui->swiped = p->swipe;
        dgx_ui_focus(ui, i);
        if (live(ui, i)) press(ui, i, p->x, p->y); /* on(FOCUS) may have removed it */
        return true;
    }
    dgx_ui_node_t *n = live(ui, ui->pressed);
    if (!n) {
        ui->pressed = -1;
        return false;
    }
    const dgx_ui_widget_t *w = n->widget;
    if (p->swipe) ui->swiped = true; /* told once: kept till it goes up */
    if (p->down) {
        const int dx = p->x - ui->press_x, dy = p->y - ui->press_y, ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
        ui->move_x = (int16_t)p->x, ui->move_y = (int16_t)p->y;
        if (w->kind == DGX_UI_BUTTON && (w->flags & DGX_UI_DRAG)) {
            /* dragged up or down half its width: changed at every move from then on (a scroll bar's knob following) */
            if (!ui->dragged && 2 * ay >= n->w) ui->dragged = true;
            if (ui->dragged) tell(ui, ui->pressed, DGX_UI_CHANGED);
            if (!live(ui, ui->pressed)) ui->pressed = -1;
            return true;
        }
        if (swiped_slider(w) && !ui->dragged && 2 * ay >= n->h && ay > ax) {
            /* up or down first, half its height: the list under it is swiped instead, from where the pointer went down */
            const int l = list_at(ui, ui->press_x, ui->press_y);
            if (l >= 0) {
                if (n->state_value.value != ui->press_value) user_value(ui, ui->pressed, ui->press_value);
                n->state_value.bits &= (uint8_t)~DGX_UI_PRESSED;
                dgx_ui_touched(ui, ui->pressed);
                if (!live(ui, l)) return ui->pressed = -1, true;
                press(ui, l, ui->press_x, ui->press_y);
                if (ui->pressed < 0) return true;
                n = NODE(ui, l), w = n->widget;
            }
        }
        const int i = ui->pressed;
        if (swiped_slider(w)) {
            swipe_to_value(ui, i, dx);
            if (n->state_value.value != ui->press_value) ui->dragged = true; /* moved once: no tap when let go */
        } else if (w->kind == DGX_UI_SLIDER) {
            point_to_value(ui, i, p->x - n->x, p->y - n->y); /* the knob follows */
        } else if (w->kind == DGX_UI_LIST) {
            /* swiped up or down: a row along for each row's height the pointer went (up: on down the list) */
            const int rh = row_height(n);
            if (rh > 0 && 2 * ay >= rh) ui->dragged = true;
            if (rh > 0 && ui->dragged) user_value(ui, i, ui->press_value + (dy <= 0 ? (-dy + rh / 2) / rh : -((dy + rh / 2) / rh)));
        }
        if (!live(ui, i)) ui->pressed = -1; /* on(CHANGED) removed it */
        return true;                        /* pressed till it goes up, wherever it goes */
    }
    if (p->released) {
        const int  i     = ui->pressed;
        const bool click = !ui->dragged && !ui->swiped && !ui->between && (uint32_t)(p->ms - ui->press_ms) < DGX_UI_CLICK_MS;
        unpress(ui);
        if (!click) return true;
        if (swiped_slider(w)) {
            tell(ui, i, DGX_UI_ACTIVATED); /* a click: nothing moved */
            return true;
        }
        if (w->kind == DGX_UI_SWITCH) user_value(ui, i, n->state_value.value ? 0 : 1);
        if (w->kind == DGX_UI_BUTTON || w->kind == DGX_UI_LIST || w->kind == DGX_UI_DROPDOWN) tell(ui, i, DGX_UI_ACTIVATED);
        return true;
    }
    return true;
}

/* the next widget that may be focused after i in the order of the tree (step 1) or before it (-1); -1 none */
static int next_focus(dgx_ui_t *ui, int i, int step)
{
    if (!live(ui, 0)) return -1;
    if (step > 0) {
        int j = i < 0 ? 0 : preorder_next(ui, i, -1);
        for (int k = 0; k <= ui->count; ++k, j = preorder_next(ui, j, -1)) {
            if (j < 0) j = 0; /* round the tree */
            if (usable(ui, j)) return j;
        }
        return -1;
    }
    /* back: the last usable one before i, else the last of all */
    int before = -1, last = -1;
    for (int j = 0; j >= 0; j = preorder_next(ui, j, -1)) {
        if (j == i) before = last;
        if (usable(ui, j)) last = j;
    }
    return i >= 0 && before >= 0 ? before : last;
}

static bool key_in(dgx_ui_t *ui, int key);

bool dgx_ui_key(dgx_ui_t *ui, int key)
{
    if (ui->popup) {
        if (dgx_ui_key(ui->popup, key)) return true; /* the top one first */
        if (ui->popup_flags & DGX_UI_POPUP_MODAL) return key != DGX_UI_KEY_BACK; /* BACK stays the program's: it may close it */
    }
    return key_in(ui, key);
}

static bool key_in(dgx_ui_t *ui, int key)
{
    dgx_ui_layout(ui);
    const int            f = ui->focus;
    dgx_ui_node_t       *n = live(ui, f);
    const dgx_ui_widget_t *w = n ? n->widget : NULL;
    switch (key) {
    case DGX_UI_KEY_NEXT:
    case DGX_UI_KEY_PREV: {
        const int step = key == DGX_UI_KEY_NEXT ? 1 : -1;
        /* in a list the rows first, then the focus goes on */
        if (w && w->kind == DGX_UI_LIST && n->state_value.value + step >= w->min && n->state_value.value + step <= w->max) {
            user_value(ui, f, n->state_value.value + step);
            return true;
        }
        const int i = next_focus(ui, f, step);
        if (i < 0) return false;
        dgx_ui_focus(ui, i);
        return true;
    }
    case DGX_UI_KEY_MORE:
    case DGX_UI_KEY_LESS:
        if (!w) return false;
        if (w->kind == DGX_UI_SLIDER) {
            user_value(ui, f, n->state_value.value + (key == DGX_UI_KEY_MORE ? 1 : -1) * (w->step > 0 ? w->step : 1));
            return true;
        }
        if (w->kind == DGX_UI_SWITCH) {
            user_value(ui, f, key == DGX_UI_KEY_MORE ? 1 : 0);
            return true;
        }
        return false;
    case DGX_UI_KEY_ACTIVATE:
        if (!w) return false;
        if (w->kind == DGX_UI_SWITCH) user_value(ui, f, n->state_value.value ? 0 : 1);
        else if (w->kind == DGX_UI_BUTTON || w->kind == DGX_UI_LIST || w->kind == DGX_UI_DROPDOWN) tell(ui, f, DGX_UI_ACTIVATED);
        else return false;
        return true;
    default:
        return false;
    }
}
/* the key of each button, as DGX-GE's dialogs and DGX-DI's pad are laid: up and down move, left and right change */
static const uint8_t default_keys[8] = {
    DGX_UI_KEY_LESS, DGX_UI_KEY_MORE, DGX_UI_KEY_PREV, DGX_UI_KEY_NEXT, DGX_UI_KEY_ACTIVATE, 0xff, DGX_UI_KEY_BACK, DGX_UI_KEY_BACK,
};

static bool repeats(int key)
{
    return key == DGX_UI_KEY_NEXT || key == DGX_UI_KEY_PREV || key == DGX_UI_KEY_MORE || key == DGX_UI_KEY_LESS;
}

void dgx_ui_button_keys(dgx_ui_t *ui, const uint8_t *keys)
{
    ui->button_keys = keys;
}

bool dgx_ui_buttons(dgx_ui_t *ui, uint32_t held, uint32_t now_ms)
{
    const uint8_t *keys  = ui->button_keys ? ui->button_keys : default_keys;
    const uint32_t went  = held & ~ui->held;
    bool           taken = false;
    ui->held             = held;
    /* the key repeating stops when its button is let go */
    if (ui->repeat_key >= 0) {
        bool still = false;
        for (int b = 0; b < 8; ++b)
            if ((held & (1u << b)) && keys[b] == ui->repeat_key) still = true;
        if (!still) ui->repeat_key = -1;
    }
    for (int b = 0; b < 8; ++b) {
        if (!(went & (1u << b)) || keys[b] == 0xff) continue;
        taken |= dgx_ui_key(ui, keys[b]);
        if (repeats(keys[b])) ui->repeat_key = (int8_t)keys[b], ui->repeat_at = now_ms + DGX_UI_REPEAT_FIRST_MS;
    }
    return taken;
}

bool dgx_ui_time(dgx_ui_t *ui, uint32_t now_ms)
{
    if (ui->repeat_key < 0) return false;
    /* by difference: now_ms runs over in 49.7 days */
    if ((int32_t)(now_ms - ui->repeat_at) >= 0) {
        dgx_ui_key(ui, ui->repeat_key);
        ui->repeat_at += DGX_UI_REPEAT_MS;
        if ((int32_t)(now_ms - ui->repeat_at) >= 0) ui->repeat_at = now_ms + DGX_UI_REPEAT_MS; /* a long stall: no burst */
    }
    return true;
}

/* ---- the popup ---- */

bool dgx_ui_popup_open(dgx_ui_t *ui, dgx_ui_t *popup, int x, int y, int w, int h, unsigned flags)
{
    if (!popup || popup == ui) return false;
    if (popup == ui->popup) dgx_ui_popup_close(ui); /* opened again, elsewhere: what was open over it goes too */
    /* a tree open as a popup already, here or elsewhere, or with a popup of its own (under this one: a loop); and no
       deeper than the depth */
    if (popup->popup || popup->level || ui->level >= DGX_UI_POPUP_DEPTH) {
        ESP_LOGW(TAG, "a popup refused: %s", ui->level >= DGX_UI_POPUP_DEPTH ? "too deep" : "it is open already, or has one over it");
        return false;
    }
    if (ui->popup) dgx_ui_popup_close(ui);
    if (ui->pressed >= 0) unpress(ui); /* the finger on the tree under it is spent */
    dgx_ui_set_root(popup, x, y, w, h);
    for (int i = 0; i < popup->count; ++i) NODE(popup, i)->marks |= DGX_UI_MARK_WHOLE;
    popup->level = (uint8_t)(ui->level + 1);
    ui->popup = popup, ui->popup_flags = (uint8_t)flags;
    return true;
}

/* the rectangle that holds two, an empty one none */
static dgx_ui_rect_t hull(dgx_ui_rect_t a, dgx_ui_rect_t b)
{
    if (dgx_ui_rect_empty(&a)) return b;
    if (dgx_ui_rect_empty(&b)) return a;
    const int x0 = a.x < b.x ? a.x : b.x, y0 = a.y < b.y ? a.y : b.y;
    const int x1 = a.x + a.w > b.x + b.w ? a.x + a.w : b.x + b.w, y1 = a.y + a.h > b.y + b.h ? a.y + a.h : b.y + b.h;
    return dgx_ui_rect(x0, y0, x1 - x0, y1 - y0);
}

void dgx_ui_popup_close(dgx_ui_t *ui)
{
    dgx_ui_t *pop = ui->popup;
    if (!pop) return;
    dgx_ui_popup_close(pop); /* the ones over it first */
    if (pop->pressed >= 0) unpress(pop);
    dgx_ui_focus(pop, -1);
    /* where it lay, and the ones over it, drawn again from this tree up; two closed before a pass: the rectangle
       that holds both */
    ui->popup_was  = hull(ui->popup_was, hull(pop->root_rect, pop->popup_was));
    pop->popup_was = dgx_ui_rect(0, 0, 0, 0);
    pop->level     = 0;
    ui->popup      = NULL;
}
